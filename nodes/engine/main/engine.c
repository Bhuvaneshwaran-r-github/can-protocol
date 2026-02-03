/*
 * engine.c - Engine Node
 * Listens for engine start/stop commands from dashboard
 * CAN ID: 0x102
 */

#include "driver/twai.h"
#include "can_header.h"

#define TX_PIN GPIO_NUM_21
#define RX_PIN GPIO_NUM_22

void app_main(void) {

  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t  = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f  = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t ret;
  ret = twai_driver_install(&g, &t, &f);
  if(ret != ESP_OK){
    printf("Failed to install TWAI driver\n");
    return;
  }

  ret = twai_start();
  if(ret != ESP_OK){
    printf("Failed to start TWAI\n");
    return;
  }

  twai_message_t message;

  while(1){
    ret = twai_receive(&message, portMAX_DELAY); //pdMS_TO_TICKS(1000) - The task blocks up to 1s. It returns immediately if a message arrives.

    if(ret == ESP_OK){
      if(message.identifier == ENGINE_CAN_ID){
        if(message.data[0] == 0x08)
          printf("Engine ON\n");
        else if(message.data[0] == 0x00)
          printf("Engine OFF\n");
      }
    }
  }
}
