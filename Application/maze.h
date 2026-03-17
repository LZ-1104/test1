#ifndef MAZE_H
#define MAZE_H

#include "status.h"

/**
 * @brief 蚂蚁寻路算法策略枚举
 */
typedef enum {
    MAZE_ALGO_LEFT_HAND = 0,    ///< 局部盲探：左手扶墙定则
    MAZE_ALGO_RIGHT_HAND,       ///< 局部盲探：右手扶墙定则
    MAZE_ALGO_RANDOM,           ///< 局部盲探：不走回头路的随机游走
    MAZE_ALGO_BFS_SHORTEST,     ///< 全局最优：广度优先(BFS)盲扩算法
    MAZE_ALGO_ASTAR_SHORTEST,   ///< 全局最优：A*(A-Star)启发式寻路算法
} maze_algo_t;

status_t Maze_Init(void);
void Maze_Test(void);

/**
 * @brief 设置接下来的找路算法（实时切换或在 Init 后调用）
 * @param algo 算法枚举值
 */
void Maze_Set_Algorithm(maze_algo_t algo);

#endif /* MAZE_H */