
#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include "tkjhat/sdk.h"
#include <tusb.h>
#include "usbSerialDebug/helper.h"

#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX      1
#define MORSE_PUSKURIN_KOKO 128


 // Sampo Niemimaa 
 // Sanni Uutela
 // Jaamme pisteet 3 ja 3

static char MorsePuskuri[MORSE_PUSKURIN_KOKO] = ""; //sensor_task morse viestin puskuri
static size_t MorsePituus = 0; // sensor_task morse viestin pituus

static void sensor_task(void *arg){
    (void)arg;
    float ax, ay, az, gx, gy, gz, t; // muuttujat kiihtyvyys- ja gyroskooppiarvoille sekä lämpötilalle

    // katsotaan onko lähetyspainallus jo käsitelty
    int PainallusKasitelty = 0;


    if (init_ICM42670() != 0) // Tarkistetaan että kiihtyvyysanturi alustui oikein
    {
        printf("IMUn alustus epäonnistui");
        vTaskDelete(NULL);
    }
    if (ICM42670_start_with_default_values() != 0)
    {
        printf("kiihtyvyysanturin käynnistys epäonnistui");
        vTaskDelete(NULL);
    }

    for (;;){
        int tila = ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t); 
        int SW1_Painettu = gpio_get(SW1_PIN); // Tunnistaa onko SW1 painettu. 0 = ei painettu, 1 = painettu
        int SW2_Painettu = gpio_get(SW2_PIN); // Tunnistaa onko SW2 painettu. 0 = ei painettu, 1 = painettu

        if (tila == 0) // mikäli palautukoodi 0 voidaan jatkaa ohjelmaa

        {
            char debug[128]; // tallennetaan kiihtyvyysanturin arvot ja tulostetaan ne 
                snprintf(debug, sizeof(debug),
                "Picon asento: X=%f, Y=%f, Z=%f\n", ax, ay, az);
                usb_serial_print(debug);
        }
        else // jos palautuskoodi eroaa arvosta 0 tulostetaan virhekoodi
        {
            printf("IMU datan lukeminen epäonnistui");   
            vTaskDelete(NULL);
        }

        if (SW1_Painettu && SW2_Painettu) // käsitellään lähetyspainallus vain kerran
            {
                if (!PainallusKasitelty)
                {
                    if (MorsePituus > 0 && tud_cdc_n_connected(CDC_ITF_TX))
                    {
                        // Lähetetään morseviesti USB:n CDC 1 -porttiin
                        tud_cdc_n_write(CDC_ITF_TX, MorsePuskuri, strlen(MorsePuskuri));
                        tud_cdc_n_write(CDC_ITF_TX, "  \n", 3);
                        tud_cdc_n_write_flush(CDC_ITF_TX);

                        MorsePuskuri[0] = '\0'; // Tyhjennetään morsepuskuri
                        MorsePituus = 0;
                    }
                    PainallusKasitelty = 1;
                }
            }
        else
            {
                PainallusKasitelty = 0;
            }

        if (SW1_Painettu && !SW2_Painettu) // tunnistetaan napin painallus luodaan piste tai viiva x-akselin arvon perusteella
        {
            char merkki = ' ';
            if (ax > 0.0f) 
            {
                merkki = '.';
                buzzer_play_tone(700, 100); // soitetaan pisteen äänimerkki
            }
            else if (ax < 0.0f)
            {
                merkki = '-';
                buzzer_play_tone(700, 300); // soitetaan viivan äänimerkki
            }
            if (merkki != ' ' && MorsePituus < MORSE_PUSKURIN_KOKO - 1) // tarkistetaan että puskurissa on tilaa
            {
                MorsePuskuri[MorsePituus] = merkki; // asetetaan merkki puskurin alkioihin alkaen nollasta
                MorsePituus++;
                MorsePuskuri[MorsePituus] = '\0'; // laitetaan nolla merkkijonon loppuun ennen tulostusta
                char debug[160];
                snprintf(debug, sizeof(debug), "Morse puskuri: %s\n", MorsePuskuri);
                usb_serial_print(debug);
            }
            else if (MorsePituus == MORSE_PUSKURIN_KOKO - 1) 
            {
                MorsePuskuri[MorsePituus] = '\0'; // Loppunolla merkkijonon päätteeksi
                char debug[160];
                snprintf(debug, sizeof(debug), "Morse puskuri täynnä\n");
                usb_serial_print(debug);
            }
        }
        if (SW2_Painettu && !SW1_Painettu) // tunnistetaan napin painallus ja tulostetaan puskurin sisältö
        {
            char välilyönti = ' ';
            if (MorsePituus < MORSE_PUSKURIN_KOKO - 1)
            {
                MorsePuskuri[MorsePituus] = välilyönti; // asetetaan välilyönti MorsePituus kohtaan puskurissa
                MorsePituus++;
                MorsePuskuri[MorsePituus] = '\0'; // loppunolla korvataan jos merkkejä tulee lisää
                char debug[160];
                    snprintf(debug, sizeof(debug), "Morse puskuri: %s\n", MorsePuskuri);
                    usb_serial_print(debug);
            }
            else if (MorsePituus == MORSE_PUSKURIN_KOKO - 1) 
            {
                MorsePuskuri[MorsePituus] = '\0'; // Loppunolla merkkijonon päätteeksi
                char debug[160];
                snprintf(debug, sizeof(debug), "Morse puskuri täynnä\n");
                usb_serial_print(debug);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Tarkistetaan 1 sekunnin intervalleilla onko sw1 tai sw2 painettu.
    }
}

static void usbTask(void *arg) {
    (void)arg;
    while (1) {
        tud_task();              // With FreeRTOS wait for events
                                 // Do not add vTaskDelay. 
    }
}

int main() {

    init_hat_sdk(); // Alustetaan TKJHAT SDK 
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.
    init_buzzer(); // Alustetaan summeri
    init_sw1 (); // Alustetaan painikkeet
    init_sw2();
    init_led();

    TaskHandle_t hSensorTask, hUSB = NULL; // USB ja sensoritehtävän taskien käsittelijät
    BaseType_t result; 

    
    result = xTaskCreate(usbTask, "usb", DEFAULT_STACK_SIZE, NULL, 3, &hUSB);
    if (result != pdPASS)
    {
        printf("USB taskin luominen epäonnistui\n");
        return 1;
    }
    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(hUSB, 1u << 0);
    #endif
    


    // Create the tasks with xTaskCreate
    result = xTaskCreate(sensor_task, // (en) Task function
                "sensor",                        // (en) Name of the task 
                DEFAULT_STACK_SIZE,              // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                            // (en) Arguments of the task 
                2,                               // (en) Priority of this task
                &hSensorTask);                   // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Sensor task creation failed\n");
        return 0;
    }


    // Start the scheduler (never returns)
    tusb_init();
    usb_serial_init();
    vTaskStartScheduler();
    
    // Never reach this line.
    return 0;
}

