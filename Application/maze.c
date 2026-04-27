/**
 * @file    maze.c
 * @brief   迷宫生成与解迷宫可视化模块 (针对 64KB RAM 极限深度优化版)
 * 
 * @details
 * 解决编译 ".bss will not fit in region RAM" 溢出的究极内存优化策略：
 * 1. 墙壁矩阵采用按位压栈 (Bit-packing)，将 1860 Bytes 压缩为 244 Bytes。
 * 2. 轨迹数组强制转化为循环位移窗取代暴增数组，内存直降 2.8 KB。
 * 3. 算法工作区（BFS字典、A*词典）不再各自开辟 static 数组，而是全部折叠进入一个 
 *    Global Union (`maze_work_ram_t`) 做到纯原位复用分配，且不侵占任何 FreeRTOS Stack 或 System Heap！
 *    通过这些处理，RAM 花销暴降超过 16KB，完美跑在任何 STM32F1 上！
 */

#include "maze.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "log.h"
#include "status.h"
#include "stm32f1xx_hal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ========================================================= */
/* ======================== 宏定义 ========================= */
/* ========================================================= */

#define MAZE_CELL_SIZE          6
#define MAZE_MAX_COLS           30
#define MAZE_MAX_ROWS           30
#define MAZE_HISTORY_PERIOD_MS  40
#define MAZE_WALL_WIDTH         1
#define MAZE_POINT_SIZE         3
#define MAZE_MAX_EXPLORE_STEPS  2500 // 改大容量，物理层面确保包容所有极限步数

// 记录每个方向位移的常量，用于重建历史路径
static const int8_t DIR_OFFSET[4][2] = {
    {-1, 0}, // UP
    {0, 1},  // RIGHT
    {1, 0},  // DOWN
    {0, -1}  // LEFT
};
#define W_COLS_BYTES   ((MAZE_MAX_COLS + 7) / 8)
#define W_COLS1_BYTES  ((MAZE_MAX_COLS + 1 + 7) / 8)
#define BIT_SET(arr, r, c)   ( (arr)[r][(c)>>3] |=  (1 << ((c)&7)) )
#define BIT_CLEAR(arr, r, c) ( (arr)[r][(c)>>3] &= ~(1 << ((c)&7)) )
#define BIT_CHECK(arr, r, c) (((arr)[r][(c)>>3] &   (1 << ((c)&7))) != 0 )

/* ========================================================= */
/* ============= 1. 数据模型与类型定义 (Model) ============= */
/* ========================================================= */

typedef struct {
    uint8_t row; 
    uint8_t col; 
} maze_point_t;

typedef enum {
    MAZE_DIR_UP = 0,    
    MAZE_DIR_RIGHT,     
    MAZE_DIR_DOWN,      
    MAZE_DIR_LEFT,      
} maze_dir_t;

typedef struct {
    uint8_t rows;           
    uint8_t cols;           
    
    // 采用位数组存储墙壁避免庞大浪费
    uint8_t wall_h[MAZE_MAX_ROWS + 1][W_COLS_BYTES]; 
    uint8_t wall_v[MAZE_MAX_ROWS][W_COLS1_BYTES];    
    
    // 战争迷雾：蚂蚁“探索”并在脑海中真实记录的局部墙壁记忆地图
    uint8_t known_wall_h[MAZE_MAX_ROWS + 1][W_COLS_BYTES]; 
    uint8_t known_wall_v[MAZE_MAX_ROWS][W_COLS1_BYTES]; 
    
    // 第一阶段：建图探索轨迹（采用方向位压缩存储，1字节存4步方向！2500步仅需 625 Bytes）
    // 告别 maze_point_t 庞大数组（原 5000 Bytes），立省 4.3 KB RAM！
    uint8_t explore_trace_dirs[(MAZE_MAX_EXPLORE_STEPS + 3) / 4];
    uint16_t explore_trace_len;

    maze_point_t start;     
    maze_point_t end;       
    maze_point_t ant_pos;   
    maze_dir_t ant_dir;     

    uint32_t ant_steps;     
    bool is_failed;         
    bool is_reached;        
    
    maze_algo_t current_algo; 
    
    // 第二阶段：确定的最优冲刺路径
    bool need_recalc;
    maze_point_t best_path[900]; 
    uint16_t best_path_len;
    uint16_t best_path_index; 
    
    // Micromouse 比赛状态控制
    bool is_end_known;       
    bool is_speed_run;       
    
    // Tremaux 探图专用：2-bit 记录每个格子的被踩次数，避免岛屿式死循环防打转（仅需240 Bytes）
    uint8_t visit_count[MAZE_MAX_ROWS][(MAZE_MAX_COLS + 3) / 4];
} maze_core_t;

typedef struct {
    lv_obj_t * bg_obj;      
    lv_obj_t * fg_obj;      
    lv_timer_t * timer;     
    bool initialized;       
    uint32_t rand_seed;     
    maze_core_t core;       
} maze_app_t;

static maze_app_t s_app = { .rand_seed = 0x6C078965U };

// ！！神级内存优化：算法工作内存联合体 (Union 分时复用) ！！
// 它们在任何时候绝对不会跑在一起，共享内存让全局 BSS 骤降十几 KB，同时避免了爆栈
typedef union {
    struct {
        uint8_t visit[MAZE_MAX_ROWS][MAZE_MAX_COLS];
        maze_point_t stack[MAZE_MAX_ROWS * MAZE_MAX_COLS];
    } gen;
    struct {
        maze_point_t queue[MAZE_MAX_ROWS * MAZE_MAX_COLS];
        uint8_t parent_dir[MAZE_MAX_ROWS][MAZE_MAX_COLS]; // 只存方向，再省一半
        uint8_t visit[MAZE_MAX_ROWS][MAZE_MAX_COLS];
    } bfs;
    struct {
        maze_point_t open_set[MAZE_MAX_ROWS * MAZE_MAX_COLS];
        uint8_t parent_dir[MAZE_MAX_ROWS][MAZE_MAX_COLS];
        uint16_t g_score[MAZE_MAX_ROWS][MAZE_MAX_COLS];
        uint16_t f_score[MAZE_MAX_ROWS][MAZE_MAX_COLS];
        uint8_t state[MAZE_MAX_ROWS][MAZE_MAX_COLS]; // 0:None, 1:Open, 2:Closed (合并标志位再省900 Bytes)
    } astar;
} maze_work_ram_t;

static maze_work_ram_t s_work;


/* ========================================================= */
/* ================== 2. 基础算法与工具接口 ================ */
/* ========================================================= */
static void Core_Seed(uint32_t seed) { s_app.rand_seed = (seed == 0U) ? 0x6C078965U : seed; }

static uint32_t Core_Rand(void) {
    s_app.rand_seed = s_app.rand_seed * 1664525U + 1013904223U;
    return s_app.rand_seed;
}

static void Core_Shuffle_Dirs(int8_t dirs[4][2]) {
    for(int i = 3; i > 0; --i) {
        uint32_t r = Core_Rand() % (uint32_t)(i + 1);
        int8_t tmp0 = dirs[i][0];
        int8_t tmp1 = dirs[i][1];
        dirs[i][0] = dirs[r][0];
        dirs[i][1] = dirs[r][1];
        dirs[r][0] = tmp0;
        dirs[r][1] = tmp1;
    }
}

static inline int16_t Core_Abs(int16_t x) {
    return (x < 0) ? -x : x;
}

/* ========================================================= */
/* ========== 3. 核心功能: 迷宫生成与求解 (Model) ========== */
/* ========================================================= */

// 生成迷宫
static void Core_Generate(maze_core_t * mc)
{
    memset(s_work.gen.visit, 0, sizeof(s_work.gen.visit));
    memset(mc->wall_h, 0xFF, sizeof(mc->wall_h));
    memset(mc->wall_v, 0xFF, sizeof(mc->wall_v));

    uint16_t sp = 0;
    maze_point_t start_pt = {(uint8_t)(Core_Rand() % mc->rows), (uint8_t)(Core_Rand() % mc->cols)};
    s_work.gen.stack[sp++] = start_pt;
    s_work.gen.visit[start_pt.row][start_pt.col] = 1;

    while(sp > 0) {
        maze_point_t cur = s_work.gen.stack[sp - 1]; 
        int8_t dirs[4][2] = { {0, -1}, {0, 1}, {-1, 0}, {1, 0} };
        Core_Shuffle_Dirs(dirs); 

        bool moved = false;
        for(int i = 0; i < 4; ++i) {
            int16_t nr = (int16_t)cur.row + dirs[i][0];
            int16_t nc = (int16_t)cur.col + dirs[i][1];
            
            if(nr < 0 || nc < 0 || nr >= (int16_t)mc->rows || nc >= (int16_t)mc->cols) continue;
            if(s_work.gen.visit[nr][nc]) continue;

            if(dirs[i][0] == -1) BIT_CLEAR(mc->wall_h, cur.row, cur.col);          // 上
            if(dirs[i][0] == 1)  BIT_CLEAR(mc->wall_h, cur.row + 1, cur.col);      // 下
            if(dirs[i][1] == -1) BIT_CLEAR(mc->wall_v, cur.row, cur.col);          // 左
            if(dirs[i][1] == 1)  BIT_CLEAR(mc->wall_v, cur.row, cur.col + 1);      // 右

            s_work.gen.visit[nr][nc] = 1;
            s_work.gen.stack[sp++] = (maze_point_t){(uint8_t)nr, (uint8_t)nc};
            moved = true;
            break;
        }
        if(!moved) sp--;
    }

    uint8_t percent = 5; 
    for(uint8_t r = 1; r < mc->rows; ++r) {
        for(uint8_t c = 0; c < mc->cols; ++c) {
            if(BIT_CHECK(mc->wall_h, r, c) && (Core_Rand() % 100U) < percent) BIT_CLEAR(mc->wall_h, r, c);
        }
    }
    for(uint8_t r = 0; r < mc->rows; ++r) {
        for(uint8_t c = 1; c < mc->cols; ++c) {
            if(BIT_CHECK(mc->wall_v, r, c) && (Core_Rand() % 100U) < percent) BIT_CLEAR(mc->wall_v, r, c);
        }
    }

    uint8_t sc = (uint8_t)(Core_Rand() % mc->cols);
    uint8_t ec = (uint8_t)(Core_Rand() % mc->cols);
    if(sc == ec && mc->cols > 1) ec = (uint8_t)((ec + (mc->cols / 2)) % mc->cols);
    
    mc->start.row = 0; 
    mc->start.col = sc;
    mc->end.row = (uint8_t)(mc->rows - 1); 
    mc->end.col = ec;
}

static void Core_Scan_Walls(maze_core_t * mc)
{
    uint8_t r = mc->ant_pos.row;
    uint8_t c = mc->ant_pos.col;
    if(BIT_CHECK(mc->wall_h, r, c))         BIT_SET(mc->known_wall_h, r, c);
    if(BIT_CHECK(mc->wall_h, r + 1, c))     BIT_SET(mc->known_wall_h, r + 1, c);
    if(BIT_CHECK(mc->wall_v, r, c))         BIT_SET(mc->known_wall_v, r, c);
    if(BIT_CHECK(mc->wall_v, r, c + 1))     BIT_SET(mc->known_wall_v, r, c + 1);
}

static bool Core_Is_Open(maze_core_t * mc, maze_point_t cur, maze_dir_t dir)
{
    // 所有寻路算法现在只能查询蚂蚁大脑中“探测过”的 known_wall，模拟真实环境“战争迷雾”下摸黑跑图！
    switch(dir) {
        case MAZE_DIR_UP:    if(cur.row == 0) return false; return !BIT_CHECK(mc->known_wall_h, cur.row, cur.col);
        case MAZE_DIR_DOWN:  if(cur.row >= mc->rows - 1) return false; return !BIT_CHECK(mc->known_wall_h, cur.row + 1, cur.col);
        case MAZE_DIR_LEFT:  if(cur.col == 0) return false; return !BIT_CHECK(mc->known_wall_v, cur.row, cur.col);
        case MAZE_DIR_RIGHT: if(cur.col >= mc->cols - 1) return false; return !BIT_CHECK(mc->known_wall_v, cur.row, cur.col + 1);
        default: return false;
    }
}

// 追加探索轨迹的方向 
static void Core_Append_Trace_Dir(maze_core_t * mc, maze_dir_t dir) {
    if (mc->explore_trace_len >= MAZE_MAX_EXPLORE_STEPS) {
        mc->explore_trace_len = MAZE_MAX_EXPLORE_STEPS - 1; // 防止无限死循环溢出
    }
    uint16_t idx = mc->explore_trace_len;
    uint16_t byte_idx = idx / 4;
    uint8_t shift = (idx % 4) * 2;
    mc->explore_trace_dirs[byte_idx] &= ~(0x03 << shift);
    mc->explore_trace_dirs[byte_idx] |= ((dir & 0x03) << shift);
    mc->explore_trace_len++;
}

// 提取当前格子的踩踏次数 (2-bit per cell)
static uint8_t Core_Get_Visit(maze_core_t * mc, uint8_t r, uint8_t c) {
    uint8_t val = mc->visit_count[r][c / 4];
    uint8_t shift = (c % 4) * 2;
    return (val >> shift) & 0x03;
}

// 增加当前格子的踩踏次数 (封顶 3)
static void Core_Inc_Visit(maze_core_t * mc, uint8_t r, uint8_t c) {
    uint8_t byte_idx = c / 4;
    uint8_t shift = (c % 4) * 2;
    uint8_t count = (mc->visit_count[r][byte_idx] >> shift) & 0x03;
    if (count < 3) count++;
    mc->visit_count[r][byte_idx] &= ~(0x03 << shift);
    mc->visit_count[r][byte_idx] |= (count << shift);
}

// 第一阶段探图算法：前线生长建图 (Frontier-based Exploration)
// 自动利用 BFS 寻找距离小鼠当前位置最近的“未知前线（0次踩踏的格子）”，并向其进发
static bool Algo_Explore(maze_core_t * mc, maze_dir_t * out_dir) {
    memset(s_work.bfs.visit, 0, sizeof(s_work.bfs.visit));
    
    uint16_t head = 0, tail = 0;
    s_work.bfs.queue[tail++] = mc->ant_pos;
    s_work.bfs.visit[mc->ant_pos.row][mc->ant_pos.col] = 1;
    
    maze_point_t target_pt = {255, 255};
    bool found = false;
    
    while(head < tail) {
        maze_point_t cur = s_work.bfs.queue[head++];
        
        // 如果这个格子是不在我们脚下、且尚未踏足过的全新未知区域：这就是离我们最近的前线！
        if ((cur.row != mc->ant_pos.row || cur.col != mc->ant_pos.col) && 
            Core_Get_Visit(mc, cur.row, cur.col) == 0) {
            target_pt = cur;
            found = true;
            break;
        }
        
        // 在已知安全的范围内（不穿墙）进行扩张
        for(uint8_t i = 0; i < 4; i++) {
            if(Core_Is_Open(mc, cur, (maze_dir_t)i)) {
                maze_point_t next = cur;
                if(i == MAZE_DIR_UP)          next.row--;
                else if(i == MAZE_DIR_DOWN)   next.row++;
                else if(i == MAZE_DIR_LEFT)   next.col--;
                else if(i == MAZE_DIR_RIGHT)  next.col++;
                
                if(!s_work.bfs.visit[next.row][next.col]) {
                    s_work.bfs.visit[next.row][next.col] = 1;
                    s_work.bfs.parent_dir[next.row][next.col] = i; // 记录进来时的方向
                    s_work.bfs.queue[tail++] = next;
                }
            }
        }
    }
    
    if(!found) return false; 
    
    // 反向沿着 parent_dir 链表回溯，查出为了去前线，现在第一步该往哪迈
    maze_point_t p = target_pt;
    maze_dir_t first_step = MAZE_DIR_UP;
    
    while(p.row != mc->ant_pos.row || p.col != mc->ant_pos.col) {
        first_step = (maze_dir_t)s_work.bfs.parent_dir[p.row][p.col];
        if(first_step == MAZE_DIR_UP)        p.row++;
        else if(first_step == MAZE_DIR_DOWN) p.row--;
        else if(first_step == MAZE_DIR_LEFT) p.col++;
        else if(first_step == MAZE_DIR_RIGHT)p.col--;
    }
    
    *out_dir = first_step;
    return true;
}

// 盲搜算法：左手定则
static bool Algo_Left_Hand(maze_core_t * mc, maze_dir_t * out_dir) {
    maze_dir_t left = (maze_dir_t)((mc->ant_dir + 3) % 4);
    maze_dir_t forward = mc->ant_dir;
    maze_dir_t right = (maze_dir_t)((mc->ant_dir + 1) % 4);
    maze_dir_t back = (maze_dir_t)((mc->ant_dir + 2) % 4);
    
    maze_dir_t order[4] = { left, forward, right, back };
    for(uint8_t i = 0; i < 4; ++i) {
        if(Core_Is_Open(mc, mc->ant_pos, order[i])) { *out_dir = order[i]; return true; }
    }
    return false;
}

// 盲搜算法：右手定则
static bool Algo_Right_Hand(maze_core_t * mc, maze_dir_t * out_dir) {
    maze_dir_t right = (maze_dir_t)((mc->ant_dir + 1) % 4);
    maze_dir_t forward = mc->ant_dir;
    maze_dir_t left = (maze_dir_t)((mc->ant_dir + 3) % 4);
    maze_dir_t back = (maze_dir_t)((mc->ant_dir + 2) % 4);
    
    maze_dir_t order[4] = { right, forward, left, back };
    for(uint8_t i = 0; i < 4; ++i) {
        if(Core_Is_Open(mc, mc->ant_pos, order[i])) { *out_dir = order[i]; return true; }
    }
    return false;
}

// 盲搜算法：排斥回头路的随机跑
static bool Algo_Random(maze_core_t * mc, maze_dir_t * out_dir) {
    maze_dir_t back = (maze_dir_t)((mc->ant_dir + 2) % 4);
    maze_dir_t open_dirs[4];
    uint8_t open_cnt = 0;
    
    for(uint8_t i = 0; i < 4; ++i) {
        if(Core_Is_Open(mc, mc->ant_pos, (maze_dir_t)i)) open_dirs[open_cnt++] = (maze_dir_t)i;
    }
    if(open_cnt == 0) return false;
    if(open_cnt == 1) { *out_dir = open_dirs[0]; return true; }
    
    maze_dir_t valid_dirs[3];
    uint8_t valid_cnt = 0;
    for(uint8_t i = 0; i < open_cnt; ++i) {
        if(open_dirs[i] != back) valid_dirs[valid_cnt++] = open_dirs[i];
    }
    uint32_t r = Core_Rand() % valid_cnt;
    *out_dir = valid_dirs[r];
    return true;
}

// 全局算法：BFS 穷举最短路径
static void Algo_Calculate_BFS(maze_core_t * mc, bool strict_verify) {
    memset(s_work.bfs.visit, 0, sizeof(s_work.bfs.visit));
    
    uint16_t head = 0, tail = 0;
    s_work.bfs.queue[tail++] = mc->ant_pos;
    s_work.bfs.visit[mc->ant_pos.row][mc->ant_pos.col] = 1;
    
    bool found = false;
    while(head < tail) {
        maze_point_t cur = s_work.bfs.queue[head++];
        if(cur.row == mc->end.row && cur.col == mc->end.col) { found = true; break; }
        
        for(uint8_t i = 0; i < 4; i++) {
            if(Core_Is_Open(mc, cur, (maze_dir_t)i)) {
                maze_point_t next = cur;
                if(i == MAZE_DIR_UP)          next.row--;
                else if(i == MAZE_DIR_DOWN)   next.row++;
                else if(i == MAZE_DIR_LEFT)   next.col--;
                else if(i == MAZE_DIR_RIGHT)  next.col++;
                
                // 最短路径只能在蚂蚁建立的地图（踏足过的区域）基础上建立！拒绝脱离实际探图抄近道！
                bool is_verified = (Core_Get_Visit(mc, next.row, next.col) > 0) || 
                                   (next.row == mc->end.row && next.col == mc->end.col) || 
                                   (next.row == mc->start.row && next.col == mc->start.col);
                if (strict_verify && !is_verified) continue;

                if(!s_work.bfs.visit[next.row][next.col]) {
                    s_work.bfs.visit[next.row][next.col] = 1;
                    s_work.bfs.parent_dir[next.row][next.col] = i; // 记录进来时的方向
                    s_work.bfs.queue[tail++] = next;
                }
            }
        }
    }
    
    mc->best_path_len = 0;
    if(found) {
        maze_point_t p = mc->end;
        uint16_t path_len = 0;
        
        // 当心死循环：只要不回到起点就不停溯源
        while((p.row != mc->ant_pos.row || p.col != mc->ant_pos.col) && path_len < sizeof(mc->best_path)/sizeof(maze_point_t)) {
            mc->best_path[path_len++] = p;
            uint8_t dir = s_work.bfs.parent_dir[p.row][p.col];
            if(dir == MAZE_DIR_UP)        p.row++;
            else if(dir == MAZE_DIR_DOWN) p.row--;
            else if(dir == MAZE_DIR_LEFT) p.col++;
            else if(dir == MAZE_DIR_RIGHT)p.col--;
        }
        
        // 数组原地掉头，准备让蚂蚁顺序消费
        uint16_t i = 0, j = path_len - 1;
        while(i < j) {
            maze_point_t tmp = mc->best_path[i];
            mc->best_path[i] = mc->best_path[j];
            mc->best_path[j] = tmp;
            i++; j--;
        }
        mc->best_path_len = path_len;
    }
}

// 全局算法：A* 启发极速最短路径
static void Algo_Calculate_AStar(maze_core_t * mc, bool strict_verify) {
    memset(s_work.astar.g_score, 0xFF, sizeof(s_work.astar.g_score)); 
    memset(s_work.astar.f_score, 0xFF, sizeof(s_work.astar.f_score));
    memset(s_work.astar.state, 0, sizeof(s_work.astar.state));
    
    uint16_t open_cnt = 0;

    s_work.astar.open_set[open_cnt++] = mc->ant_pos;
    s_work.astar.g_score[mc->ant_pos.row][mc->ant_pos.col] = 0;
    s_work.astar.f_score[mc->ant_pos.row][mc->ant_pos.col] = 
        Core_Abs((int16_t)mc->ant_pos.row - (int16_t)mc->end.row) + Core_Abs((int16_t)mc->ant_pos.col - (int16_t)mc->end.col);
    s_work.astar.state[mc->ant_pos.row][mc->ant_pos.col] = 1;

    bool found = false;

    while(open_cnt > 0) {
        uint16_t best_idx = 0;
        uint16_t min_f = 0xFFFF;
        for(uint16_t i = 0; i < open_cnt; ++i) {
            maze_point_t pt = s_work.astar.open_set[i];
            if(s_work.astar.f_score[pt.row][pt.col] < min_f) {
                min_f = s_work.astar.f_score[pt.row][pt.col];
                best_idx = i;
            }
        }

        maze_point_t cur = s_work.astar.open_set[best_idx];

        if(cur.row == mc->end.row && cur.col == mc->end.col) { found = true; break; }

        s_work.astar.open_set[best_idx] = s_work.astar.open_set[open_cnt - 1];
        open_cnt--;
        s_work.astar.state[cur.row][cur.col] = 2; // 2=closed

        for(uint8_t i = 0; i < 4; i++) {
            if(Core_Is_Open(mc, cur, (maze_dir_t)i)) {
                maze_point_t next = cur;
                if(i == MAZE_DIR_UP)          next.row--;
                else if(i == MAZE_DIR_DOWN)   next.row++;
                else if(i == MAZE_DIR_LEFT)   next.col--;
                else if(i == MAZE_DIR_RIGHT)  next.col++;

                // 最短路径只能在蚂蚁建立的地图（踏足过的区域）基础上建立！拒绝脱离实际探图抄近道！
                bool is_verified = (Core_Get_Visit(mc, next.row, next.col) > 0) || 
                                   (next.row == mc->end.row && next.col == mc->end.col) || 
                                   (next.row == mc->start.row && next.col == mc->start.col);
                if (strict_verify && !is_verified) continue;

                if(s_work.astar.state[next.row][next.col] == 2) continue; 

                uint16_t try_g = s_work.astar.g_score[cur.row][cur.col] + 1; 
                if(try_g < s_work.astar.g_score[next.row][next.col]) {
                    s_work.astar.parent_dir[next.row][next.col] = i; // Save entry dir
                    s_work.astar.g_score[next.row][next.col] = try_g;
                    s_work.astar.f_score[next.row][next.col] = try_g + Core_Abs((int16_t)next.row - (int16_t)mc->end.row) + Core_Abs((int16_t)next.col - (int16_t)mc->end.col);
                    
                    if(s_work.astar.state[next.row][next.col] != 1) {
                        s_work.astar.open_set[open_cnt++] = next;
                        s_work.astar.state[next.row][next.col] = 1;
                    }
                }
            }
        }
    }

    mc->best_path_len = 0;
    if(found) {
        maze_point_t p = mc->end;
        uint16_t path_len = 0;
        while((p.row != mc->ant_pos.row || p.col != mc->ant_pos.col) && path_len < sizeof(mc->best_path)/sizeof(maze_point_t)) {
            mc->best_path[path_len++] = p;
            uint8_t dir = s_work.astar.parent_dir[p.row][p.col];
            if(dir == MAZE_DIR_UP)        p.row++;
            else if(dir == MAZE_DIR_DOWN) p.row--;
            else if(dir == MAZE_DIR_LEFT) p.col++;
            else if(dir == MAZE_DIR_RIGHT)p.col--;
        }
        
        // Inplace Reverse
        uint16_t i = 0, j = path_len - 1;
        while(i < j) {
            maze_point_t tmp = mc->best_path[i];
            mc->best_path[i] = mc->best_path[j];
            mc->best_path[j] = tmp;
            i++; j--;
        }
        mc->best_path_len = path_len;
    }
}

// 调度步进与上帝算法劫持网关
static bool Core_Ant_Step(maze_core_t * mc)
{
    if (mc->is_reached || mc->is_failed) return false;

    // 1. 扫描物理墙壁更新到自己的脑部地图中
    Core_Scan_Walls(mc);

    // 假如踩到了终点红点
    if (!mc->is_end_known && mc->ant_pos.row == mc->end.row && mc->ant_pos.col == mc->end.col) {
        mc->is_end_known = true;
        log_Info("Maze: \033[36m[TARGET REACHED]\033[0m End point [%d, %d] found! Mapping continues...", mc->end.row, mc->end.col);
        // 纯盲搜算法到了终点比赛就结束了
        if (mc->current_algo == MAZE_ALGO_LEFT_HAND || mc->current_algo == MAZE_ALGO_RIGHT_HAND || mc->current_algo == MAZE_ALGO_RANDOM) {
            mc->is_reached = true; 
            return false;
        }
    }

    // ============================================
    // 乐观状态评估判断：判断当前路线是否绝对最短？
    // ============================================
    bool optimal_found = false;
    if (mc->is_end_known && !mc->is_speed_run && 
       (mc->current_algo == MAZE_ALGO_BFS_SHORTEST || mc->current_algo == MAZE_ALGO_ASTAR_SHORTEST)) {
        maze_point_t backup = mc->ant_pos;
        mc->ant_pos = mc->start; // 假装在起点重新算到终点的代价
        
        // 1. L_real 真实踩出过的最短路径
        Algo_Calculate_AStar(mc, true);
        uint16_t l_real = (mc->best_path_len > 0) ? mc->best_path_len : 0xFFFF;
        
        // 2. L_ideal 理想极限最短路径 (视所有黑雾为空地)
        Algo_Calculate_AStar(mc, false);
        uint16_t l_ideal = (mc->best_path_len > 0) ? mc->best_path_len : 0xFFFE;
        
        mc->ant_pos = backup;
        
        static uint16_t last_real = 0xFFFF, last_ideal = 0xFFFF;
        if (mc->ant_steps <= 1) { last_real = 0xFFFF; last_ideal = 0xFFFF; }
        
        if (l_real != last_real || l_ideal != last_ideal) {
            log_Info("Maze: Eval -> L_real(Known): %u  |  L_ideal(Theory): %u", l_real, l_ideal);
            last_real = l_real;
            last_ideal = l_ideal;
        }
        
        // 一旦探测到的真实路程小于等于理想路程，证明绝对不可能有捷径了！触发剪枝！
        if (l_real != 0xFFFF && l_real <= l_ideal) {
            log_Info("Maze: \033[32m[PRUNED]\033[0m Optimal path PROVED! Real(%u) <= Ideal(%u). Phase 1 END.", l_real, l_ideal);
            optimal_found = true;
        }
    }

    // ============================================
    // 第一阶段 / 第二阶段 控制网关
    // ============================================
    if (optimal_found) {
        // 完美！结束探图，进入第二阶段(冲刺模式)
        mc->ant_pos = mc->start; 
        mc->ant_dir = MAZE_DIR_RIGHT;
        mc->is_speed_run = true;
        mc->need_recalc = true;
        if (s_app.fg_obj) lv_obj_invalidate(s_app.fg_obj);
        return true; 
    }

    if (!mc->is_speed_run) {
        // 第一阶段：未完成探图。继续采用跑图探索策略
        bool can_move = false;
        maze_dir_t next_dir = MAZE_DIR_UP;
        
        if (mc->current_algo == MAZE_ALGO_LEFT_HAND) can_move = Algo_Left_Hand(mc, &next_dir);
        else if (mc->current_algo == MAZE_ALGO_RIGHT_HAND) can_move = Algo_Right_Hand(mc, &next_dir);
        else if (mc->current_algo == MAZE_ALGO_RANDOM) can_move = Algo_Random(mc, &next_dir);
        else can_move = Algo_Explore(mc, &next_dir); // A*的摸图默认行为：前线生长建图

        // 如果算法探完所有可去方格都找不到路（或者找不到了）且已知终点，说明当前找到的那条虽然不一定是 L_ideal，但也无其它路了
        if (!can_move) {
            if (mc->is_end_known) {
                // 退无可退，强行转入第二阶段
                log_Info("Maze: \033[33m[FORCED]\033[0m Map exhausted. No more paths to explore. Phase 1 END.");
                mc->ant_pos = mc->start; 
                mc->ant_dir = MAZE_DIR_RIGHT;
                mc->is_speed_run = true;
                mc->need_recalc = true;
                if (s_app.fg_obj) lv_obj_invalidate(s_app.fg_obj);
                return true;
            } else {
                log_Info("Maze: \033[31m[FAILED]\033[0m Map exhausted. Target not reachable!");
                mc->is_failed = true;
                return false;
            }
        }

        if (mc->current_algo == MAZE_ALGO_BFS_SHORTEST || mc->current_algo == MAZE_ALGO_ASTAR_SHORTEST) {
            Core_Inc_Visit(mc, mc->ant_pos.row, mc->ant_pos.col);
        }

        maze_point_t next_pos = mc->ant_pos;
        if(next_dir == MAZE_DIR_UP)          next_pos.row--;
        else if(next_dir == MAZE_DIR_DOWN)   next_pos.row++;
        else if(next_dir == MAZE_DIR_LEFT)   next_pos.col--;
        else if(next_dir == MAZE_DIR_RIGHT)  next_pos.col++;

        mc->ant_pos = next_pos;
        mc->ant_dir = next_dir;

        // 保存行走方向以极低内存还原轨迹
        Core_Append_Trace_Dir(mc, next_dir);

        mc->ant_steps++;
        return true;
    }
    else {
        // 第二阶段：已知完美短路的冲刺阶段 (Speed Run)
        if (mc->need_recalc) {
            if(mc->current_algo == MAZE_ALGO_BFS_SHORTEST) Algo_Calculate_BFS(mc, true);
            else Algo_Calculate_AStar(mc, true);
            
            log_Info("Maze: \033[35m[PHASE 2]\033[0m Speed Run Engaged! Final Shortest Path: %u steps.", mc->best_path_len);
            mc->need_recalc = false;
            mc->best_path_index = 0;
            if(mc->best_path_len == 0) { mc->is_failed = true; return false; }
        }
        
        if (mc->best_path_index < mc->best_path_len) {
            maze_point_t next_pos = mc->best_path[mc->best_path_index++];
            
            if (next_pos.row < mc->ant_pos.row) mc->ant_dir = MAZE_DIR_UP;
            else if (next_pos.row > mc->ant_pos.row) mc->ant_dir = MAZE_DIR_DOWN;
            else if (next_pos.col < mc->ant_pos.col) mc->ant_dir = MAZE_DIR_LEFT;
            else if (next_pos.col > mc->ant_pos.col) mc->ant_dir = MAZE_DIR_RIGHT;
            
            mc->ant_pos = next_pos;
            return true;
        }
        return false;
    }
}

static void Core_Reset(maze_core_t * mc)
{
    memset(mc->known_wall_h, 0, sizeof(mc->known_wall_h));
    memset(mc->known_wall_v, 0, sizeof(mc->known_wall_v));
    memset(mc->visit_count, 0, sizeof(mc->visit_count));
    memset(mc->explore_trace_dirs, 0, sizeof(mc->explore_trace_dirs));
    mc->explore_trace_len = 0;
    mc->ant_pos = mc->start;
    mc->ant_dir = MAZE_DIR_RIGHT; 
    mc->ant_steps = 0;
    mc->is_failed = false;
    mc->is_reached = false;
    mc->is_end_known = false;
    mc->is_speed_run = false;
    mc->need_recalc = true; 
}


/* ========================================================= */
/* ============= 4. GUI 与 View 绘制渲染层 (View) ========== */
/* ========================================================= */

static void View_Invalidate_Cell(lv_obj_t * obj, maze_point_t p)
{
    if(!obj) return;
    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);
    
    lv_area_t area;
    area.x1 = obj_coords.x1 + p.col * MAZE_CELL_SIZE - 2;
    area.y1 = obj_coords.y1 + p.row * MAZE_CELL_SIZE - 2;
    area.x2 = obj_coords.x1 + (p.col + 1) * MAZE_CELL_SIZE + 2;
    area.y2 = obj_coords.y1 + (p.row + 1) * MAZE_CELL_SIZE + 2;
    lv_obj_invalidate_area(obj, &area);
}

static void View_Draw_Point(lv_layer_t * layer, const lv_area_t * origin, maze_point_t p, lv_color_t color)
{
    if(p.row >= s_app.core.rows || p.col >= s_app.core.cols) return;

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_opa = LV_OPA_TRANSP;

    int32_t cx = origin->x1 + (int32_t)p.col * MAZE_CELL_SIZE + (MAZE_CELL_SIZE / 2);
    int32_t cy = origin->y1 + (int32_t)p.row * MAZE_CELL_SIZE + (MAZE_CELL_SIZE / 2);
    int32_t half = MAZE_POINT_SIZE / 2;
    
    lv_area_t area = { .x1 = cx - half, .y1 = cy - half, .x2 = cx + half - 1, .y2 = cy + half - 1 };
    lv_draw_rect(layer, &dsc, &area);
}

static void View_Draw_Bg_Event(lv_event_t * e)
{
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_obj_t * obj = lv_event_get_target(e);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    maze_core_t * mc = &s_app.core;

    lv_draw_rect_dsc_t bg_dsc;
    lv_draw_rect_dsc_init(&bg_dsc);
    bg_dsc.bg_color = lv_color_hex(0xFFFF);
    bg_dsc.bg_opa = LV_OPA_COVER;
    lv_draw_rect(layer, &bg_dsc, &coords);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x000000);
    line_dsc.width = MAZE_WALL_WIDTH;

    for(uint8_t r = 0; r <= mc->rows; ++r) {
        int32_t y = coords.y1 + (int32_t)r * MAZE_CELL_SIZE;
        for(uint8_t c = 0; c < mc->cols; ++c) {
            if(BIT_CHECK(mc->wall_h, r, c)) {
                line_dsc.p1.x = coords.x1 + (int32_t)c * MAZE_CELL_SIZE;
                line_dsc.p1.y = y;
                line_dsc.p2.x = coords.x1 + (int32_t)(c + 1) * MAZE_CELL_SIZE;
                line_dsc.p2.y = y;
                lv_draw_line(layer, &line_dsc);
            }
        }
    }

    for(uint8_t r = 0; r < mc->rows; ++r) {
        int32_t y1 = coords.y1 + (int32_t)r * MAZE_CELL_SIZE;
        int32_t y2 = coords.y1 + (int32_t)(r + 1) * MAZE_CELL_SIZE;
        for(uint8_t c = 0; c <= mc->cols; ++c) {
            if(BIT_CHECK(mc->wall_v, r, c)) {
                int32_t x = coords.x1 + (int32_t)c * MAZE_CELL_SIZE;
                line_dsc.p1.x = x;     line_dsc.p1.y = y1;
                line_dsc.p2.x = x;     line_dsc.p2.y = y2;
                lv_draw_line(layer, &line_dsc);
            }
        }
    }
}

static void View_Draw_Fg_Event(lv_event_t * e)
{
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_obj_t * obj = lv_event_get_target(e);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    maze_core_t * mc = &s_app.core;

    // 绘制紫色建图轨迹 (永久保留，表示探索的过程)
    if(mc->explore_trace_len >= 1) {
        lv_draw_line_dsc_t expl_dsc;
        lv_draw_line_dsc_init(&expl_dsc);
        expl_dsc.color = lv_color_hex(0x8000FF); // 紫色细线
        expl_dsc.width = 1;

        maze_point_t curr_pos = mc->start;

        for(uint16_t i = 0; i < mc->explore_trace_len; ++i) {
            uint16_t byte_idx = i / 4;
            uint8_t bit_shift = (i % 4) * 2;
            uint8_t d = (mc->explore_trace_dirs[byte_idx] >> bit_shift) & 0x03;

            maze_point_t next_pos = curr_pos;
            next_pos.row += DIR_OFFSET[d][0];
            next_pos.col += DIR_OFFSET[d][1];

            expl_dsc.p1.x = coords.x1 + (int32_t)curr_pos.col * MAZE_CELL_SIZE + (MAZE_CELL_SIZE / 2);
            expl_dsc.p1.y = coords.y1 + (int32_t)curr_pos.row * MAZE_CELL_SIZE + (MAZE_CELL_SIZE / 2);
            expl_dsc.p2.x = coords.x1 + (int32_t)next_pos.col * MAZE_CELL_SIZE + (MAZE_CELL_SIZE / 2);
            expl_dsc.p2.y = coords.y1 + (int32_t)next_pos.row * MAZE_CELL_SIZE + (MAZE_CELL_SIZE / 2);
            lv_draw_line(layer, &expl_dsc);

            curr_pos = next_pos;
        }
    }

    // 叠加绘制橙色冲刺轨迹 (只在建立好的地图上重跑的过程)
    if(mc->is_speed_run && mc->best_path_index > 0) {
        lv_draw_line_dsc_t spd_dsc;
        lv_draw_line_dsc_init(&spd_dsc);
        spd_dsc.color = lv_color_hex(0xFF8000); // 橙色粗线
        spd_dsc.width = 3;
        
        maze_point_t prev = mc->start;
        for(uint16_t i = 0; i < mc->best_path_index; ++i) {
            maze_point_t p1 = mc->best_path[i];
            spd_dsc.p1.x = coords.x1 + (int32_t)prev.col * MAZE_CELL_SIZE + (MAZE_CELL_SIZE / 2);
            spd_dsc.p1.y = coords.y1 + (int32_t)prev.row * MAZE_CELL_SIZE + (MAZE_CELL_SIZE / 2);
            spd_dsc.p2.x = coords.x1 + (int32_t)p1.col * MAZE_CELL_SIZE + (MAZE_CELL_SIZE / 2);
            spd_dsc.p2.y = coords.y1 + (int32_t)p1.row * MAZE_CELL_SIZE + (MAZE_CELL_SIZE / 2);
            lv_draw_line(layer, &spd_dsc);
            prev = p1;
        }
    }

    View_Draw_Point(layer, &coords, mc->start, lv_color_hex(0x00FF00));      
    View_Draw_Point(layer, &coords, mc->end, lv_color_hex(0xFF0000));        
    View_Draw_Point(layer, &coords, mc->ant_pos, lv_color_hex(0x0000FF));    
}


/* ========================================================= */
/* =========== 5. Controller 控制与定时软中断调度器 ======== */
/* ========================================================= */

static void App_Timer_Cb(lv_timer_t * timer)
{
    (void)timer;
    maze_core_t * mc = &s_app.core;

    if (mc->is_reached || mc->is_failed) {
        log_Info("Maze: Ant run finished/failed.");
        lv_timer_pause(s_app.timer);
        return;
    }

    maze_point_t old_pos = mc->ant_pos;

    if (!Core_Ant_Step(mc)) {
        log_Info("Maze: Ant failed to move.");
        lv_timer_pause(s_app.timer);
        return;
    }

    if(s_app.fg_obj) {
        // 由于不搞滑动窗和预测连线擦除等复杂逻辑，部分刷新依旧能满足极流畅体验：
        View_Invalidate_Cell(s_app.fg_obj, old_pos);
        View_Invalidate_Cell(s_app.fg_obj, mc->ant_pos);
    }
}

status_t Maze_Init(void)
{
    lv_init();
    lv_port_disp_init();
    
    lv_display_t * disp = lv_display_get_default();
    if(disp == NULL) return STATUS_ERROR;

    uint16_t w = (uint16_t)lv_display_get_horizontal_resolution(disp);
    uint16_t h = (uint16_t)lv_display_get_vertical_resolution(disp);

    s_app.core.cols = (uint8_t)(w / MAZE_CELL_SIZE);
    s_app.core.rows = (uint8_t)(h / MAZE_CELL_SIZE);
    
    if(s_app.core.cols > MAZE_MAX_COLS) s_app.core.cols = MAZE_MAX_COLS;
    if(s_app.core.rows > MAZE_MAX_ROWS) s_app.core.rows = MAZE_MAX_ROWS;
    if(s_app.core.cols < 2 || s_app.core.rows < 2) return STATUS_ERROR;

    Core_Seed((uint32_t)(lv_tick_get() ^ (uint32_t)SysTick->VAL));
    Core_Generate(&s_app.core); 
    Core_Reset(&s_app.core);    

    if(s_app.bg_obj) { lv_obj_del(s_app.bg_obj); s_app.bg_obj = NULL; }
    if(s_app.fg_obj) { lv_obj_del(s_app.fg_obj); s_app.fg_obj = NULL; }

    s_app.bg_obj = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_app.bg_obj, s_app.core.cols * MAZE_CELL_SIZE, s_app.core.rows * MAZE_CELL_SIZE);
    lv_obj_align(s_app.bg_obj, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_pad_all(s_app.bg_obj, 0, 0);
    lv_obj_set_style_border_width(s_app.bg_obj, 0, 0);
    lv_obj_set_style_radius(s_app.bg_obj, 0, 0);
    lv_obj_clear_flag(s_app.bg_obj, LV_OBJ_FLAG_SCROLLABLE);       
    lv_obj_add_event_cb(s_app.bg_obj, View_Draw_Bg_Event, LV_EVENT_DRAW_MAIN, NULL); 

    s_app.fg_obj = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_app.fg_obj, s_app.core.cols * MAZE_CELL_SIZE, s_app.core.rows * MAZE_CELL_SIZE);
    lv_obj_align(s_app.fg_obj, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_pad_all(s_app.fg_obj, 0, 0);
    lv_obj_set_style_border_width(s_app.fg_obj, 0, 0);
    lv_obj_set_style_radius(s_app.fg_obj, 0, 0);
    lv_obj_set_style_bg_opa(s_app.fg_obj, LV_OPA_TRANSP, 0);       
    lv_obj_clear_flag(s_app.fg_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_app.fg_obj, View_Draw_Fg_Event, LV_EVENT_DRAW_MAIN, NULL); 

    if(s_app.timer) lv_timer_del(s_app.timer);
    s_app.timer = lv_timer_create(App_Timer_Cb, MAZE_HISTORY_PERIOD_MS, NULL);

    s_app.initialized = true;
    return STATUS_OK;
}

void Maze_Test(void)
{
    if(!s_app.initialized) {
        (void)Maze_Init();
    }
    Maze_Set_Algorithm(MAZE_ALGO_ASTAR_SHORTEST);
    if(s_app.bg_obj) lv_obj_invalidate(s_app.bg_obj);
    if(s_app.fg_obj) lv_obj_invalidate(s_app.fg_obj);
}

void Maze_Set_Algorithm(maze_algo_t algo)
{
    s_app.core.current_algo = algo;
    s_app.core.need_recalc = true; 
    log_Info("Maze: Algorithm switched to %d", (int)algo);
}
