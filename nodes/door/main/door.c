/*
 * engine.c - Engine Node
 * Listens for engine start/stop commands from dashboard
 * CAN ID: 0x102
 */

#include "driver/twai.h"
#include "driver/gpio.h"
#include "../../can_header.h"
#include "soc/gpio_num.h"

#define TX_PIN GPIO_NUM_21
#define RX_PIN GPIO_NUM_22
#define DOOR_PIN GPIO_NUM_25
#define SEATBELT_PIN GPIO_NUM_26

void app_main(void) {

  int door_status,seatbelt_status;
  esp_err_t ret;
  twai_message_t message;

  //Configure GPIO
  gpio_set_direction(DOOR_PIN, GPIO_MODE_INPUT);
  gpio_pullup_en(DOOR_PIN);
  gpio_set_direction(SEATBELT_PIN, GPIO_MODE_INPUT);
  gpio_pullup_en(SEATBELT_PIN);

  //Configure TWAI
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t  = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f  = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  
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

  while(1){
    ret = twai_receive(&message, portMAX_DELAY); //pdMS_TO_TICKS(1000) - The task blocks up to 1s. It returns immediately if a message arrives.

    if((ret == ESP_OK) && (message.rtr == 1)){
      if(message.identifier == DOOR_CAN_ID){
        door_status = gpio_get_level(DOOR_PIN);
        message.data[0] = door_status;
        message.identifier = DOOR_CAN_ID;
        message.rtr = 0;
        message.data_length_code = 1;
      }else if(message.identifier == SEATBELT_CAN_ID){
        seatbelt_status = gpio_get_level(SEATBELT_PIN);
        message.data[0] = seatbelt_status;
        message.identifier = SEATBELT_CAN_ID;
        message.rtr = 0;
        message.data_length_code = 1;
      }
      twai_transmit(&message, portMAX_DELAY);
    }
  }
}
