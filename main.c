#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX      1

enum state { WAITING=1, READING=2, PRINT=3, RECEIVING=4, DATAREADY=5};
enum state programState = WAITING;

//uint32_t ambientLight;
char lastMessage[64];
int messageLenght;
float ax, ay, az, gx, gy, gz, t;


static void buttonFxn_1(uint gpio, uint32_t eventMask) {
    if (programState == WAITING) {
        programState = READING;
    } else if (programState == READING) {
        init_ICM42670();
        ICM42670_start_with_default_values();
        ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t);
        programState = DATAREADY;
        //lastMessage[messageLenght++] = '\n';
        //programState = PRINT;
    }
}

static void buttonFxn_2(uint gpio, uint32_t eventMask) {
    if(programState == READING) {
        lastMessage[messageLenght++] = '\n';
        programState = PRINT;
    }
}

static void readingsFromIMU(void *arg){
    for(;;){
        if (programState == DATAREADY) {
            if (az > 0.8) {
                lastMessage[messageLenght++] = '.';
            } else if (az < -0.8) {
                lastMessage[messageLenght++] = '-';
            } else if (((az < 0.2) || (az < -0.2))) {
                lastMessage[messageLenght++] = ' ';
            }
            programState = READING;
            //printf("ax: %f, ay: %f, az: %f, gx: %f, gy %f, gz %f, t %f \n", ax, ay, az, gx, gy, gz, t);
            //programState = DATA_READY;
        }
        //printf("%d/n", veml6030_read_light());e

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void printTask(void *arg){
    (void)arg;
    for(;;){
        if (programState == PRINT) {
            for (int i = 0; i < messageLenght; i++) {
                printf("%c", lastMessage[i]);
                lastMessage[i] = NULL;
            }
        }
    }
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

    TaskHandle_t hReadingsFromIMU, hPrintTask, hUSB = NULL;

    BaseType_t result = xTaskCreate(readingsFromIMU, // (en) Task function
                "sensor",                        // (en) Name of the task 
                DEFAULT_STACK_SIZE,              // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                            // (en) Arguments of the task 
                2,                               // (en) Priority of this task
                &hReadingsFromIMU);                   // (en) A handle to control the execution of this task

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
