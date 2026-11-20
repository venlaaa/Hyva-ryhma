#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX      1

void playMusic();

volatile static uint32_t button_last_click = 0;
volatile static int button_counter = 0;

volatile enum state { WAITING=1, READING=2, PRINT=3, POSITIONREADY=4, SHOWANDPLAY=5, READING_LUX=6};
volatile enum state programState = WAITING;

//uint32_t ambientLight;
volatile char lastMessage[64];
volatile int messageLenght = 0;
float ax, ay, az, gx, gy, gz, t;

//Soittaa lyhyen melodian kun työasemalta saapunut viesti on näytetty
//ledin ja näytön avulla.
void playMusic() {
    buzzer_play_tone(247, 500);
    buzzer_play_tone(262, 500);
    buzzer_play_tone(165, 1000);
    buzzer_play_tone(196, 2000);
    buzzer_play_tone(247, 500);
    buzzer_play_tone(262, 500);
    buzzer_play_tone(165, 1000);
    buzzer_play_tone(196, 1000);
    buzzer_play_tone(175, 1000);
}


static void buttonFxn(uint gpio, uint32_t eventMask) {
    if (gpio == SW1_PIN) {
       if (programState == WAITING) {
            programState = READING;
        } else if (programState == READING) {
            programState = POSITIONREADY;
        }
    } else if (gpio == SW2_PIN) {
        if (programState == READING || programState == READING_LUX) {
            uint32_t now = time_us_32();
            if (now - button_last_click < 800000 && button_counter == 1) {
                // Toinen painallus -> rivinvaihto
                button_counter = 0;
                lastMessage[messageLenght++] = '\n';
                programState = PRINT;
            } else {
                // Ensimmäinen painallus -> välilyönti
                button_counter = 1;
                button_last_click = now;
            }
        }
    }
}


//Tarkistetaan IMU-anturin asento napin 1 painalluksen jälkeen.
//Vaaka-asento on piste, ja toisinpäin vaaka-asento on viiva.
static void readingsFromIMU(void *arg){
    (void) arg;
    while(1) {
        if (programState == POSITIONREADY) {
            ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t);
            //aluperin az vaihdoimme ay
            if (ay > 0.8) {
                lastMessage[messageLenght++] = '.';
            } else if (ay < -0.8) {
                lastMessage[messageLenght++] = '-';
            }
            programState = READING;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

//Tulostaa lähetetyn viestin, jonka jälkeen siirrytään odotustilaan.
static void printTask(void *arg){
    (void) arg;
    while(1) {
        if (programState == PRINT) {
            for (int i = 0; i < messageLenght; i++) {
                printf("%c", lastMessage[i]);
                lastMessage[i] = '\0';
            }
            messageLenght = 0;
            programState = WAITING;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

//Vastaanotetaan työasemalta viestejä.
//Kun on tullut kaksi välilyöntiä ja rivinvaihto, siirrytään
//näyttämään viesti ledin ja näytön avulla.
static void receivingData(void *arg) {
    (void) arg;
    while(1) {
        int t = getchar_timeout_us(0);
        if (t >= 0) {
            lastMessage[messageLenght++] = (char)t;
            if (messageLenght >= 3) {
                if ((lastMessage[messageLenght - 3] == ' ' && 
                        lastMessage[messageLenght - 2] == ' ' && 
                        lastMessage[messageLenght - 1] == '\n')) {
                    programState = SHOWANDPLAY;
                }
            }
        }
        if (button_counter == 1 &&
            time_us_32() - button_last_click >= 800000) {

            lastMessage[messageLenght++] = ' ';
            button_counter = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

//Näytetään työasemalta tullut viesti ledin ja näytön avulla
//merkki kerrallaan.
static void LEDAndDisplay(void *arg) {
    (void) arg;
    while(1) {
        if (programState == SHOWANDPLAY) {
            for (int i = 0; i < messageLenght; i++) {
                if (lastMessage[i] == '.') {
                    blink_led_red(1);
                } else if (lastMessage[i] == '-') {
                    toggle_red_led();
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    toggle_red_led();
                } else if (lastMessage[i] == ' ') {
                    vTaskDelay(pdMS_TO_TICKS(1500));
                }
            }
            messageLenght = 0;
            playMusic();
            programState = WAITING;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }   
}

//Luetaan merkkejä luxejen avulla kun ei käytetä IMU-anturia.
//Aloitetaan viestintä jos on tosi pimeää.
static void readingLight(void *arg) {
    (void)arg;
    while(1) {
        uint32_t lux = veml6030_read_light();
        if (programState == WAITING) {
            if (lux > 800){
                programState = READING_LUX;
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }

        if (programState == READING_LUX) {
            lux = veml6030_read_light();
            if (lux < 20) {
                lastMessage[messageLenght ++] = '.';            
            } else if (lux > 800) {
                lastMessage[messageLenght ++] = '-';
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main() {

    stdio_init_all();
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    //Alustetaan sensorit ja aktuaattorit.
    init_sw1(); //nappi 1
    init_sw2(); //nappi 2
    init_ICM42670();
    ICM42670_start_with_default_values();
    init_led();
    init_veml6030();

    
    
    //Keskeytyskäsittelijät
    gpio_set_irq_enabled_with_callback(SW1_PIN, GPIO_IRQ_EDGE_FALL, true, &buttonFxn);
    gpio_set_irq_enabled(SW2_PIN, GPIO_IRQ_EDGE_FALL, true);

    TaskHandle_t hReadingsFromIMU, hPrintTask, hReceivingData, hLEDAndDisplay, hReadingLight = NULL;

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

    result = xTaskCreate(printTask, 
                "print",              
                DEFAULT_STACK_SIZE,   
                NULL,                 
                2,                    
                &hPrintTask);         

    if(result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }

    result = xTaskCreate(receivingData,
                "receive",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &hReceivingData);

    if(result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }

    result = xTaskCreate(LEDAndDisplay, 
                "show", 
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &hLEDAndDisplay);

    if(result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }

    result = xTaskCreate(readingLight,
                "light",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &hReadingLight);
    
    if(result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }
    

    // Start the scheduler (never returns)
    vTaskStartScheduler();
    
    return 0;
}


/*
- main() keskeytyskäsittelijät
-receivingData välilyöntilogiikka
-readingsFromImu kolmas ehto
-taulukon ylivuodon tarkistuksen puute
-volatile muuttujat (button_last_click, button_counter, 
messageLenght, lastMessage)
*/
