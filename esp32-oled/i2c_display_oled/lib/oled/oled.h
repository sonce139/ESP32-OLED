#ifndef _OLED_H_
#define _OLED_H_
#include <stdio.h>

#define OLED_DAT_IO                     GPIO_NUM_13
#define OLED_CLK_IO                     GPIO_NUM_14
#define OLED_RST_IO                     GPIO_NUM_18
#define OLED_DC_IO                      GPIO_NUM_5

#define OLED_RST_SET(x)                 gpio_set_level(OLED_RST_IO,x)
#define OLED_DC_SET(x)                  gpio_set_level(OLED_DC_IO,x)

typedef enum{
    OLED_WRITE_DATA = 1,
    OLED_WRITE_CMD = 0,
}oled_dc_t;

void oledInit(void);
void oledSetPos(uint8_t ucIdxX, uint8_t ucIdxY);
void oledFill(uint8_t byte);
void oledShowString(uint8_t x, uint8_t y,char *str);

void oledShowChrP6x8(uint8_t ucIdxX, uint8_t ucIdxY, uint8_t ucData);

void oledSetDisplayOnOff(uint8_t ucData);
void oledSetDisplayClock(uint8_t ucData);
void oledSetMultiplexRatio(uint8_t ucData);
void oledSetDisplayOffset(uint8_t ucData);
void oledSetStartLine(uint8_t ucData);
void oledSetChargePump(uint8_t ucData);
void oledSetAddressingMode(uint8_t ucData);
void oledSetSegmentRemap(uint8_t ucData);
void oledSetCommonRemap(uint8_t ucData);
void oledSetCommonConfig(uint8_t ucData);
void oledSetContrastControl(uint8_t ucData);
void oledSetPrechargePeriod(uint8_t ucData);
void oledSetVCOMH(uint8_t ucData);
void oledSetEntireDisplay(uint8_t ucData);
void oledSetInverseDisplay(uint8_t ucData);
#endif