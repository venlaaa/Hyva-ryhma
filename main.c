#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX      1

enum state { WAITING=1, DATA_READY=2, PRINT=3};
enum state programState = WAITING;

//uint32_t ambientLight;
char lastMessage[64];


static void buttonFxn_1(uint gpio, uint32_t eventMask) {
    toggle_red_led ();
    // Exercise 1: Toggle the LED using the appropriate function from the TKJHAT SDK.
}

static void buttonFxn_2(uint gpio, uint32_t eventMask) {
    
}

static void readingData(void *arg){
    (void)arg;
    init_ICM42670();
   
    for(;;){
        if (programState == WAITING) {
        float ax, ay, az, gx, gy, gz, t;
        ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t);
        printf("ax: %f, ay: %f, az: %f, gx: %f, gy %f, gz %f, t %f \n", ax, ay, az, gx, gy, gz, t);
           //programState = DATA_READY;
        }
        //printf("%d/n", veml6030_read_light());e

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void printTask(void *arg){
    (void)arg;
    /*while(1){
        if (programState == PRINT) {
            for(int i = 0; i < 64; i++) {
                if (lastMessage[i] == ' ' && lastMessage[i + 1] == ' ') {
                    if (lastMessage[i + 2] == rivinvaihto) {
                            string = jokumuokkaus();
                            printf("string");
                            return 0;
                        }
                    }
                printf("%c", lastMessage[i]);
            }
        }
    }*/
    programState = WAITING;
        //printf("printTask\n");
        
    vTaskDelay(pdMS_TO_TICKS(500));
}

int main() {

    stdio_init_all();
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    //Alustetaan sensorit ja aktuaattorit.
    init_sw1();
    init_sw2();
    init_ICM42670();
    
    
    //Keskeytyskäsittelijät
    gpio_set_irq_enabled_with_callback(SW1_PIN, GPIO_IRQ_EDGE_FALL, true, buttonFxn_1);
    gpio_set_irq_enabled_with_callback(SW2_PIN, GPIO_IRQ_EDGE_FALL, true, buttonFxn_2);

    TaskHandle_t hReadingData, hPrintTask, hUSB = NULL;

    BaseType_t result = xTaskCreate(readingData, // (en) Task function
                "sensor",                        // (en) Name of the task 
                DEFAULT_STACK_SIZE,              // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                            // (en) Arguments of the task 
                2,                               // (en) Priority of this task
                &hReadingData);                   // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Sensor task creation failed\n");
        return 0;
    }
    result = xTaskCreate(printTask,  // (en) Task function
                "print",              // (en) Name of the task 
                DEFAULT_STACK_SIZE,   // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                 // (en) Arguments of the task 
                2,                    // (en) Priority of this task
                &hPrintTask);         // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();
    
    return 0;
}

