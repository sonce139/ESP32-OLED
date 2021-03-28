/*
Author: SoniC
Faculty of Computer Engineering - UIT
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"

/* TIMER_BASE_CLK = 80 MHZ */ 
// sample period interval for the timer 0 (1/TIMER_PERIOD = Timer frequency in HZ)
#define TIMER_PERIOD   1 

#define TIMER_SENSOR TIMER_0        // Timer for counting 
#define TIMER_DIVIDER         8     // Hardware timer clock divider
//convert counter value to seconds
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER) 
#define TEST_WITHOUT_RELOAD   0     // testing will be done without auto reload
#define TEST_WITH_RELOAD      1     // testing will be done with auto reload


//Task ID for calling the task from the interrupt (timer_isr)
TaskHandle_t timer_taskID; 

/* Timer ISR handler */
void IRAM_ATTR timer_isr()
{
    /* Start the interrupt and lock timer_group 0 */
    timer_spinlock_take(TIMER_GROUP_0);

    /* Clear the interrupt (with reload)*/
    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
    
   /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_SENSOR);
    
    /* Notify timer_task that the buffer is full which means that 
    timer_task gets activated every time the isr is called */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(timer_taskID, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }

    /*Finish the interrupt and unlock timer_group 0 (i guess)*/
    timer_spinlock_give(TIMER_GROUP_0);
}

void conf_timer(){
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TEST_WITH_RELOAD,
    }; // default clock source is APB
    timer_init(TIMER_GROUP_0, TIMER_SENSOR, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, TIMER_SENSOR, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_SENSOR, TIMER_PERIOD * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, TIMER_SENSOR);
    timer_isr_register(TIMER_GROUP_0, TIMER_SENSOR, timer_isr, NULL, ESP_INTR_FLAG_IRAM, NULL);
    timer_start(TIMER_GROUP_0, TIMER_SENSOR);
}


void timer_task(void *pvParameter)
{
    while (1) {
        // Sleep until the ISR gives us something to do, if nothing is recieved then waits forever
        uint32_t temp = ulTaskNotifyTake(pdFALSE, portMAX_DELAY); 
        // if ISR give us something to do then print the following message 
        printf("Timer task!!! %d seconds has passed\n",TIMER_PERIOD);
    }
}


void app_main(void)
{
    conf_timer(); //configurate the timer 0 for this example
    xTaskCreate(timer_task, "timer_task", 2048, NULL, 5, &timer_taskID); 
}
