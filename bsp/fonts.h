#ifndef __FONT_H
#define __FONT_H       

#include "stm32f1xx_hal.h"
#include <stdio.h>

/*中文字符字节宽度*/
#define ILI9341_CHN_CHAR_WIDTH			2		//UTF-8编码格式给3，GB2312编码格式给2

typedef struct{
    const uint8_t *Table;
    uint16_t Width;  // 字符宽度
    uint16_t Height; // 字符高度
} FontDef;

typedef struct{
	char Index[ILI9341_CHN_CHAR_WIDTH + 1];	//汉字索引
	const uint8_t *Data;						 // 汉字点阵数据 (16x16 -> 32 bytes)
} ChineseFontDef;

typedef struct{
	const ChineseFontDef *Table;
	uint16_t Width;
	uint16_t Height;
} ChineseFont;


extern FontDef Font8x16;
extern FontDef Font16x24;
extern FontDef Font16x32;
extern ChineseFont CFont16x16;


#endif /*end of __FONT_H    */
