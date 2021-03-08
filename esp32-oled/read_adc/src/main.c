
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#define ADC1_TEST_CHANNEL 0         //define ADC pin

void adc1task(void* parameter)
{
    // initialize ADC
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_TEST_CHANNEL, ADC_ATTEN_DB_11);
    
    while(1)
    {
        //read ADC and print
        printf("The adc1 value: %d\n", adc1_get_raw(ADC1_TEST_CHANNEL));
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void app_main()
{
    xTaskCreate(adc1task, "adc1task", 1024*3, NULL, 5, NULL);
}