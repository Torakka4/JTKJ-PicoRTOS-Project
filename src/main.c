
#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include "tkjhat/sdk.h"
#include <tusb.h>
#include "usbSerialDebug/helper.h"

// Exercise 4. Include the libraries necessaries to use the usb-serial-debug, and tinyusb
// Tehtävä 4 . Lisää usb-serial-debugin ja tinyusbin käyttämiseen tarvittavat kirjastot.



#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX      1
#define MORSE_PUSKURIN_KOKO 128



// Tehtävä 3: Tilakoneen esittely Add missing states.
// Exercise 3: Definition of the state machine. Add missing states.
enum state { WAITING=1, DATA_READY=2 };
enum state programState = WAITING;

// Tehtävä 3: Valoisuuden globaali muuttuja
// Exercise 3: Global variable for ambient light
uint32_t ambientLight;

static void btn_fxn(uint gpio, uint32_t eventMask) {
    // Tehtävä 1: Vaihda LEDin tila käyttämällä TKJHAT SDK:n sopivaa funktiota.
    // Exercise 1: Toggle the LED using the appropriate function from the TKJHAT SDK.
    toggle_led();
    

}

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

 
// Exercise 4: Uncomment the following line to activate the TinyUSB library.  
// Tehtävä 4:  Poista seuraavan rivin kommentointi aktivoidaksesi TinyUSB-kirjaston. 


static void usbTask(void *arg) {
    (void)arg;
    while (1) {
        tud_task();              // With FreeRTOS wait for events
                                 // Do not add vTaskDelay. 
    }
}

int main() {

    // Exercise 4: Comment the statement stdio_init_all(); 
    //             Instead, add AT THE END OF MAIN (before vTaskStartScheduler();) adequate statements to enable the TinyUSB library and the usb-serial-debug.
    //             You can see hello_dual_cdc for help
    //             In CMakeLists.txt add usb_serial_debug to target_link_libraries.
    //             In CMakeLists.txt deactivate pico_enable_stdio_usb
    // Tehtävä 4:  Kommentoi lause stdio_init_all();
    //             Sen sijaan lisää MAIN LOPPUUN (ennen vTaskStartScheduler();) tarvittavat komennot aktivoidaksesi TinyUSB-kirjaston ja usb-serial-debugin.
    //             Voit katsoa apua esimerkistä hello_dual_cdc.
    //             Lisää CMakeLists.txt-tiedoston target_link_libraries-kohtaan usb_serial_debug.
    //             Poista CMakeLists.txt-tiedostosta käytöstä pico_enable_stdio_usb

    //stdio_init_all();

    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/ 
    
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.
    init_buzzer();
    init_sw1 ();
    init_sw2();
    init_led();
    // gpio_set_irq_enabled_with_callback(SW1_PIN, GPIO_IRQ_EDGE_FALL, true, btn_fxn);

    // Exercise 1: Initialize the button and the led and define an register the corresponding interrupton.
    //             Interruption handler is defined up as btn_fxn
    // Tehtävä 1:  Alusta painike ja LEd ja rekisteröi vastaava keskeytys.
    //             Keskeytyskäsittelijä on määritelty yläpuolella nimellä btn_fxn



   
    
    TaskHandle_t hSensorTask, hUSB = NULL;
    BaseType_t result;

    // Exercise 4: Uncomment this xTaskCreate to create the task that enables dual USB communication.
    // Tehtävä 4: Poista tämän xTaskCreate-rivin kommentointi luodaksesi tehtävän,
    // joka mahdollistaa kaksikanavaisen USB-viestinnän.

    
    result = xTaskCreate(usbTask, "usb", 2048, NULL, 3, &hUSB);
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

