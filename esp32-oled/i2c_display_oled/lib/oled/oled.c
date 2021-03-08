#include "oled.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "string.h"

static spi_device_handle_t spi;
const uint8_t F6x8[][6];
static void oledWrite(oled_dc_t dc, uint8_t byte);

//此函数调用再传输之前。
//它可以设置用户DC引脚。
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc=(int)t->user;
    gpio_set_level(OLED_DC_IO, dc);
}

//初始化oled
void oledInit(void)
{
    //配置SPI
    
    esp_err_t ret;
    spi_bus_config_t buscfg={
        .miso_io_num=-1,                        //IO配置
        .mosi_io_num=OLED_DAT_IO,
        .sclk_io_num=OLED_CLK_IO,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=1                      //最大传输字节数
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=SPI_MASTER_FREQ_80M,    //配置时钟
        .mode=0,                                //SPI mode 0
        .spics_io_num=-1,                       //CS引脚未使用
        .queue_size=7,                          //同时排队7个事务
        .pre_cb=lcd_spi_pre_transfer_callback,  //指定传输前回调以处理DC引脚
        .flags = SPI_DEVICE_NO_DUMMY            //只使用输出不使用输入，这可以使SPI传输速度达到最快。且可以使用IO矩阵方式不使用IO_MUX都可以达到最高频率。（禁用虚拟位变通和频率检查，禁用时输出频率可达80MHz，即使使用GPIO matrix）
        //官方解析SPI_DEVICE_NO_DUMMY宏定义都用法：
        //If the Host only writes data, the dummy bit workaround and the frequency check can be disabled by setting the bit SPI_DEVICE_NO_DUMMY in the member spi_device_interface_config_t::flags. 
        //When disabled, the output frequency can be 80MHz, even if the GPIO matrix is used.
    };
    //Initialize the SPI bus
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 0);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
    
    gpio_pad_select_gpio(OLED_RST_IO);
    gpio_set_direction(OLED_RST_IO,GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(OLED_DC_IO);
    gpio_set_direction(OLED_DC_IO,GPIO_MODE_OUTPUT);

    //复位OLED
    OLED_RST_SET(0);  
    vTaskDelay(100);
    OLED_RST_SET(1);

    //配置OLED
    oledSetDisplayOnOff(0x00);     // Display Off (0x00/0x01)
    oledSetDisplayClock(0x80);     // Set Clock as 100 Frames/Sec
    oledSetMultiplexRatio(0x3F);   // 1/64 Duty (0x0F~0x3F)
    oledSetDisplayOffset(0x00);    // Shift Mapping RAM Counter (0x00~0x3F)
    oledSetStartLine(0x00);        // Set Mapping RAM Display Start Line (0x00~0x3F)
    oledSetChargePump(0x04);       // Enable Embedded DC/DC Converter (0x00/0x04)
    oledSetAddressingMode(0x02);   // Set Page Addressing Mode (0x00/0x01/0x02)
    oledSetSegmentRemap(0x01);     // Set SEG/Column Mapping     0x00���ҷ��� 0x01����
    oledSetCommonRemap(0x08);      // Set COM/Row Scan Direction 0x00���·��� 0x08����
    oledSetCommonConfig(0x10);     // Set Sequential Configuration (0x00/0x10)
    oledSetContrastControl(0xCF);  // Set SEG Output Current
    oledSetPrechargePeriod(0xF1);  // Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
    oledSetVCOMH(0x40);            // Set VCOM Deselect Level
    oledSetEntireDisplay(0x00);    // Disable Entire Display On (0x00/0x01)
    oledSetInverseDisplay(0x00);   // Disable Inverse Display On (0x00/0x01)  
    oledSetDisplayOnOff(0x01);     // Display On (0x00/0x01)
    oledFill(0x00);            // Clean display
}

//显示字符串
void oledShowString(uint8_t x, uint8_t y,char *str)
{
    while(*str != '\0')
    {
        oledShowChrP6x8(x,y,*str);
        y+=6;
        if(y >= 128 - 6)
        {
            if(x < 8)x++;
            y = 0;
        }
        str++;
    }
}

void oledShowChrP6x8(uint8_t ucIdxX, uint8_t ucIdxY, uint8_t ucData)
{
    uint8_t i, ucDataTmp;     
    ucDataTmp = ucData-32;
    if(ucIdxX > 122)
    {
        ucIdxX = 0;
        ucIdxY++;
    }
    oledSetPos(ucIdxY, ucIdxX);
    for(i = 0; i < 6; i++)
        oledWrite(OLED_WRITE_DATA,F6x8[ucDataTmp][i]);
}

//写入数据/命令
static void oledWrite(oled_dc_t dc, uint8_t byte)
{
    spi_transaction_t st;
    memset(&st,0,sizeof(st));
    st.flags = SPI_TRANS_USE_TXDATA;
    st.length = 8;//8位数据
    st.tx_data[0] = byte;
    st.user = (void*)dc;
    spi_device_polling_transmit(spi,&st);
}

//设定坐标
void oledSetPos(uint8_t ucIdxX, uint8_t ucIdxY)
{
    oledWrite(OLED_WRITE_CMD,0xb0 + ucIdxY);
    oledWrite(OLED_WRITE_CMD,((ucIdxX & 0xf0) >> 4) | 0x10);
    oledWrite(OLED_WRITE_CMD,(ucIdxX & 0x0f) | 0x00);
}

//填充
void oledFill(uint8_t byte)
{
    uint8_t ucPage,ucColumn;
    for(ucPage = 0; ucPage < 8; ucPage++)
    {
        oledWrite(OLED_WRITE_CMD,0xb0 + ucPage);
        oledWrite(OLED_WRITE_CMD,0x00);
        oledWrite(OLED_WRITE_CMD,0x10);
        for(ucColumn = 0; ucColumn < 128; ucColumn++)
        {
            oledWrite(OLED_WRITE_DATA,byte);
        }
    }
}

void oledSetDisplayOnOff(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0xAE|ucData); // Set Display On/Off
                            // Default => 0xAE
                            // 0xAE (0x00) => Display Off
                            // 0xAF (0x01) => Display On
}

void oledSetDisplayClock(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0xD5);        // Set Display Clock Divide Ratio / Oscillator Frequency
    oledWrite(OLED_WRITE_CMD,ucData);      // Default => 0x80
                            // D[3:0] => Display Clock Divider
                            // D[7:4] => Oscillator Frequency
}

void oledSetMultiplexRatio(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0xA8);        // Set Multiplex Ratio
    oledWrite(OLED_WRITE_CMD,ucData);      // Default => 0x3F (1/64 Duty)
}

void oledSetDisplayOffset(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0xD3);        // Set Display Offset
    oledWrite(OLED_WRITE_CMD,ucData);      // Default => 0x00
}

void oledSetStartLine(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0x40|ucData); // Set Display Start Line
                            // Default => 0x40 (0x00)
}

void oledSetChargePump(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0x8D);        // Set Charge Pump
    oledWrite(OLED_WRITE_CMD,0x10|ucData); // Default => 0x10
                            // 0x10 (0x00) => Disable Charge Pump
                            // 0x14 (0x04) => Enable Charge Pump
}

void oledSetAddressingMode(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0x20);        // Set Memory Addressing Mode
    oledWrite(OLED_WRITE_CMD,ucData);      // Default => 0x02
                            // 0x00 => Horizontal Addressing Mode
                            // 0x01 => Vertical Addressing Mode
                            // 0x02 => Page Addressing Mode
}

void oledSetSegmentRemap(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0xA0|ucData); // Set Segment Re-Map
                            // Default => 0xA0
                            // 0xA0 (0x00) => Column Address 0 Mapped to SEG0
                            // 0xA1 (0x01) => Column Address 0 Mapped to SEG127
}

void oledSetCommonRemap(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0xC0|ucData); // Set COM Output Scan Direction
                            // Default => 0xC0
                            // 0xC0 (0x00) => Scan from COM0 to 63
                            // 0xC8 (0x08) => Scan from COM63 to 0
}

void oledSetCommonConfig(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0xDA);        // Set COM Pins Hardware Configuration
    oledWrite(OLED_WRITE_CMD,0x02|ucData); // Default => 0x12 (0x10)
                            // Alternative COM Pin Configuration
                            // Disable COM Left/Right Re-Map
}

void oledSetContrastControl(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0x81);        // Set Contrast Control
    oledWrite(OLED_WRITE_CMD,ucData);      // Default => 0x7F
}

void oledSetPrechargePeriod(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0xD9);        // Set Pre-Charge Period
    oledWrite(OLED_WRITE_CMD,ucData);      // Default => 0x22 (2 Display Clocks [Phase 2] / 2 Display Clocks [Phase 1])
                            // D[3:0] => Phase 1 Period in 1~15 Display Clocks
                            // D[7:4] => Phase 2 Period in 1~15 Display Clocks
}

void oledSetVCOMH(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0xDB);        // Set VCOMH Deselect Level
    oledWrite(OLED_WRITE_CMD,ucData);      // Default => 0x20 (0.77*VCC)
}

void oledSetEntireDisplay(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0xA4|ucData); // Set Entire Display On / Off
                            // Default => 0xA4
                            // 0xA4 (0x00) => Normal Display
                            // 0xA5 (0x01) => Entire Display On
}

void oledSetInverseDisplay(uint8_t ucData)
{
    oledWrite(OLED_WRITE_CMD,0xA6|ucData); // Set Inverse Display On/Off
                            // Default => 0xA6
                            // 0xA6 (0x00) => Normal Display
                            // 0xA7 (0x01) => Inverse Display On
}

const uint8_t F6x8[][6] =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   //sp0
    { 0x00, 0x00, 0x00, 0x2f, 0x00, 0x00 },   // !1
    { 0x00, 0x00, 0x07, 0x00, 0x07, 0x00 },   // "2
    { 0x00, 0x14, 0x7f, 0x14, 0x7f, 0x14 },   // #3
    { 0x00, 0x24, 0x2a, 0x7f, 0x2a, 0x12 },   // $4
    { 0x00, 0x62, 0x64, 0x08, 0x13, 0x23 },   // %5
    { 0x00, 0x36, 0x49, 0x55, 0x22, 0x50 },   // &6
    { 0x00, 0x00, 0x05, 0x03, 0x00, 0x00 },   // '7
    { 0x00, 0x00, 0x1c, 0x22, 0x41, 0x00 },   // (8
    { 0x00, 0x00, 0x41, 0x22, 0x1c, 0x00 },   // )9
    { 0x00, 0x14, 0x08, 0x3E, 0x08, 0x14 },   // *10
    { 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08 },   // +11
    { 0x00, 0x00, 0x00, 0xA0, 0x60, 0x00 },   // ,12
    { 0x00, 0x08, 0x08, 0x08, 0x08, 0x08 },   // -13
    { 0x00, 0x00, 0x60, 0x60, 0x00, 0x00 },   // .14
    { 0x00, 0x20, 0x10, 0x08, 0x04, 0x02 },   // /15
    { 0x00, 0x3E, 0x51, 0x49, 0x45, 0x3E },   // 016
    { 0x00, 0x00, 0x42, 0x7F, 0x40, 0x00 },   // 117
    { 0x00, 0x42, 0x61, 0x51, 0x49, 0x46 },   // 218
    { 0x00, 0x21, 0x41, 0x45, 0x4B, 0x31 },   // 319
    { 0x00, 0x18, 0x14, 0x12, 0x7F, 0x10 },   // 420
    { 0x00, 0x27, 0x45, 0x45, 0x45, 0x39 },   // 521
    { 0x00, 0x3C, 0x4A, 0x49, 0x49, 0x30 },   // 622
    { 0x00, 0x01, 0x71, 0x09, 0x05, 0x03 },   // 723
    { 0x00, 0x36, 0x49, 0x49, 0x49, 0x36 },   // 824
    { 0x00, 0x06, 0x49, 0x49, 0x29, 0x1E },   // 925
    { 0x00, 0x00, 0x36, 0x36, 0x00, 0x00 },   // :26
    { 0x00, 0x00, 0x56, 0x36, 0x00, 0x00 },   // ;27
    { 0x00, 0x08, 0x14, 0x22, 0x41, 0x00 },   // <28
    { 0x00, 0x14, 0x14, 0x14, 0x14, 0x14 },   // =29
    { 0x00, 0x00, 0x41, 0x22, 0x14, 0x08 },   // >30
    { 0x00, 0x02, 0x01, 0x51, 0x09, 0x06 },   // ?31
    { 0x00, 0x32, 0x49, 0x59, 0x51, 0x3E },   // @32
    { 0x00, 0x7C, 0x12, 0x11, 0x12, 0x7C },   // A33
    { 0x00, 0x7F, 0x49, 0x49, 0x49, 0x36 },   // B34
    { 0x00, 0x3E, 0x41, 0x41, 0x41, 0x22 },   // C35
    { 0x00, 0x7F, 0x41, 0x41, 0x22, 0x1C },   // D36
    { 0x00, 0x7F, 0x49, 0x49, 0x49, 0x41 },   // E37
    { 0x00, 0x7F, 0x09, 0x09, 0x09, 0x01 },   // F38
    { 0x00, 0x3E, 0x41, 0x49, 0x49, 0x7A },   // G39
    { 0x00, 0x7F, 0x08, 0x08, 0x08, 0x7F },   // H40
    { 0x00, 0x00, 0x41, 0x7F, 0x41, 0x00 },   // I41
    { 0x00, 0x20, 0x40, 0x41, 0x3F, 0x01 },   // J42
    { 0x00, 0x7F, 0x08, 0x14, 0x22, 0x41 },   // K43
    { 0x00, 0x7F, 0x40, 0x40, 0x40, 0x40 },   // L44
    { 0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F },   // M45
    { 0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F },   // N46
    { 0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E },   // O47
    { 0x00, 0x7F, 0x09, 0x09, 0x09, 0x06 },   // P48
    { 0x00, 0x3E, 0x41, 0x51, 0x21, 0x5E },   // Q49
    { 0x00, 0x7F, 0x09, 0x19, 0x29, 0x46 },   // R50
    { 0x00, 0x46, 0x49, 0x49, 0x49, 0x31 },   // S51
    { 0x00, 0x01, 0x01, 0x7F, 0x01, 0x01 },   // T52
    { 0x00, 0x3F, 0x40, 0x40, 0x40, 0x3F },   // U53
    { 0x00, 0x1F, 0x20, 0x40, 0x20, 0x1F },   // V54
    { 0x00, 0x3F, 0x40, 0x38, 0x40, 0x3F },   // W55
    { 0x00, 0x63, 0x14, 0x08, 0x14, 0x63 },   // X56
    { 0x00, 0x07, 0x08, 0x70, 0x08, 0x07 },   // Y57
    { 0x00, 0x61, 0x51, 0x49, 0x45, 0x43 },   // Z58
    { 0x00, 0x00, 0x7F, 0x41, 0x41, 0x00 },   // [59
    { 0x00, 0x02, 0x04, 0x08, 0x10, 0x20 },   // \60
    { 0x00, 0x00, 0x41, 0x41, 0x7F, 0x00 },   // ]61
    { 0x00, 0x04, 0x02, 0x01, 0x02, 0x04 },   // ^62
    { 0x00, 0x40, 0x40, 0x40, 0x40, 0x40 },   // _63
    { 0x00, 0x00, 0x01, 0x02, 0x04, 0x00 },   // '64
    { 0x00, 0x20, 0x54, 0x54, 0x54, 0x78 },   // a65
    { 0x00, 0x7F, 0x48, 0x44, 0x44, 0x38 },   // b66
    { 0x00, 0x38, 0x44, 0x44, 0x44, 0x20 },   // c67
    { 0x00, 0x38, 0x44, 0x44, 0x48, 0x7F },   // d68
    { 0x00, 0x38, 0x54, 0x54, 0x54, 0x18 },   // e69
    { 0x00, 0x08, 0x7E, 0x09, 0x01, 0x02 },   // f70
    { 0x00, 0x18, 0xA4, 0xA4, 0xA4, 0x7C },   // g71
    { 0x00, 0x7F, 0x08, 0x04, 0x04, 0x78 },   // h72
    { 0x00, 0x00, 0x44, 0x7D, 0x40, 0x00 },   // i73
    { 0x00, 0x40, 0x80, 0x84, 0x7D, 0x00 },   // j74
    { 0x00, 0x7F, 0x10, 0x28, 0x44, 0x00 },   // k75
    { 0x00, 0x00, 0x41, 0x7F, 0x40, 0x00 },   // l76
    { 0x00, 0x7C, 0x04, 0x18, 0x04, 0x78 },   // m77
    { 0x00, 0x7C, 0x08, 0x04, 0x04, 0x78 },   // n78
    { 0x00, 0x38, 0x44, 0x44, 0x44, 0x38 },   // o79
    { 0x00, 0xFC, 0x24, 0x24, 0x24, 0x18 },   // p80
    { 0x00, 0x18, 0x24, 0x24, 0x18, 0xFC },   // q81
    { 0x00, 0x7C, 0x08, 0x04, 0x04, 0x08 },   // r82
    { 0x00, 0x48, 0x54, 0x54, 0x54, 0x20 },   // s83
    { 0x00, 0x04, 0x3F, 0x44, 0x40, 0x20 },   // t84
    { 0x00, 0x3C, 0x40, 0x40, 0x20, 0x7C },   // u85
    { 0x00, 0x1C, 0x20, 0x40, 0x20, 0x1C },   // v86
    { 0x00, 0x3C, 0x40, 0x30, 0x40, 0x3C },   // w87
    { 0x00, 0x44, 0x28, 0x10, 0x28, 0x44 },   // x88
    { 0x00, 0x1C, 0xA0, 0xA0, 0xA0, 0x7C },   // y89
    { 0x00, 0x44, 0x64, 0x54, 0x4C, 0x44 },   // z90
    { 0x14, 0x14, 0x14, 0x14, 0x14, 0x14 }    // horiz lines91
};