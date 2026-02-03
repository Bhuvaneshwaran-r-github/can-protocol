/*
 * engine.c - Engine Node
 * Listens for engine start/stop commands from dashboard
 * CAN ID: 0x102
 */

#include "driver/twai.h"

#define TX_PIN GPIO_NUM_21
#define RX_PIN GPIO_NUM_22

void app_main(void) {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t  = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f  = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  twai_driver_install(&g, &t, &f);
  twai_start();

  twai_message_t message;

  while(1){
    esp_err_t ret = twai_receive(&message, portMAX_DELAY); //pdMS_TO_TICKS(1000) - The task blocks up to 1s. It returns immediately if a message arrives.

    if(ret == ESP_OK){
      printf("MSG ID : %03X\nDATA   : ", message.identifier);
      for(int i = 0; i < message.data_length_code; i++)
        printf("%02X ",message.data[i]);  
      printf("\n");
    }
  }
}
