/*
Author: SoniC
Faculty of Computer Engineering - UIT
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED_GPIO 12
#define Button_GPIO 13

void button_task(void *pvParameter)
{
    /* Init GPIO */
    gpio_pad_select_gpio(LED_GPIO);
    gpio_pad_select_gpio(Button_GPIO);

    /* Set up GPIO */
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);         

    gpio_set_direction(Button_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(Button_GPIO, GPIO_PULLUP_ONLY);  
        
    while(1) 
    {
        if (gpio_get_level(Button_GPIO) == 0)
            gpio_set_level(LED_GPIO, 0);        

        else
            gpio_set_level(LED_GPIO, 1);
    }
}

void app_main()
{
    xTaskCreate(&button_task, "button_task", 1024, NULL, 5, NULL);
}
