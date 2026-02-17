/*
 * ESP32 Motor Control with L298N H-Bridge Driver + Potentiometer Speed Control
 * ESP-IDF Framework Implementation
 * 
 * Hardware Setup:
 * - ESP32 WROOM-32 Dev Kit
 * - L298N Dual H-Bridge Motor Driver
 * - 5V DC Motor
 * - 9V Battery for motor power
 * - HS-S28-L Rotary Potentiometer
 * 
 * Connections:
 * Motor Control:
 * ESP32 GPIO26 → L298N IN1
 * ESP32 GPIO27 → L298N IN2  
 * ESP32 GPIO25 → L298N ENA (PWM)
 * ESP32 GND → L298N GND (Common Ground - CRITICAL!)
 * 9V Battery (+) → L298N +12V
 * 9V Battery (-) → L298N GND
 * Motor Wire 1 → L298N OUT1
 * Motor Wire 2 → L298N OUT2
 * 
 * Potentiometer:
 * Potentiometer VCC → ESP32 3.3V
 * Potentiometer Wiper → ESP32 GPIO15 (ADC2_CH3)
 * Potentiometer GND → ESP32 GND
 */

#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "../../can_header.h"
#include "driver/twai.h"

// Pin Definitions
#define MOTOR_IN1       GPIO_NUM_26  // Direction control 1
#define MOTOR_IN2       GPIO_NUM_27  // Direction control 2
#define MOTOR_ENA       GPIO_NUM_25  // Enable pin (PWM speed control)
#define TX_PIN          GPIO_NUM_21  // CAN TX pin
#define RX_PIN          GPIO_NUM_22  // CAN RX pin  

#define LEDC_FREQUENCY  (5000)       // PWM Configuration - 5 kHz frequency
#define MAX_SAFE_SPEED  100          // Maximum motor speed (0-100%)

// UART Configuration

static const char *TAG1 = "MOTOR_CONTROL";
static uint8_t motor_state = 0;  // false = OFF, true = ON
volatile uint8_t motor_speed = 0;  // Motor speed percentage (0-100%)
esp_err_t ret;


// Function prototypes
void motor_init(void);
void pwm_init(void);
void motor_set_operation(uint32_t state);
void motor_set_speed(void *pvParameters);
void can_init(void);

void app_main(void)
{
    can_init();
    pwm_init();  // Initialize PWM for speed control
    motor_init(); // Initialize motor control
 
    // Create motor control task
    xTaskCreate(motor_set_speed, "motor_set_speed", 2048, NULL, 10, NULL);

    twai_message_t message;

    while(1){
        do{
            twai_receive(&message, portMAX_DELAY);
        }while(message.identifier != ENGINE_CAN_ID);
        
        if((message.identifier == ENGINE_CAN_ID)&&(message.data[0] == EN_ON)){
            motor_set_operation(1); // Start with motor ON
            motor_state = 1;
        }


        while(1){
            twai_receive(&message, pdMS_TO_TICKS(20));
            
            if(message.identifier == ACCELERATOR_CAN_ID){
                uint16_t value = (message.data[1] << 8) | message.data[0];
                motor_speed = 40 + (value*60)/4095;
                ESP_LOGI(TAG1, ">>> motor speed: %x", value);
            }
            else if((message.identifier == ENGINE_CAN_ID)&&(message.data[0] == EN_OFF)){
                motor_set_operation(0);
                motor_state = 0;
                break;
            }
        }
    }
}

void can_init(void){
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
}

//Initialize pwm for motor speed control for ENA pin
void pwm_init(void)
{
    // Prepare and set configuration of timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and set configuration of PWM channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = MOTOR_ENA,
        .duty           = 0,  // Start with 0% duty cycle (motor off)
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_LOGI(TAG1, "[INIT] PWM configured: %d Hz, %d-bit resolution", LEDC_FREQUENCY, LEDC_TIMER_8_BIT);
}

//Initialize motor input 1&2 pins
void motor_init(void)
{
    // Configure GPIO pins for motor direction control (IN1, IN2)
    // Note: ENA is configured as PWM channel, not regular GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MOTOR_IN1) | (1ULL << MOTOR_IN2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Initialize all pins to LOW
    gpio_set_level(MOTOR_IN1, 0);
    gpio_set_level(MOTOR_IN2, 0);
    
    ESP_LOGI(TAG1, "[INIT] Motor control pins configured");
}

void motor_set_operation(uint32_t state){
    uint32_t duty;
    // state = 1 (true) -> Motor ON, state = 0 (false) -> Motor OFF
    gpio_set_level(MOTOR_IN1, state);  // Forward direction
    gpio_set_level(MOTOR_IN2, 0);

    if(state){
        vTaskDelay(pdMS_TO_TICKS(10));     // Small delay to prevent sudden current surge, if motor is ON
        duty = (motor_speed * 255) / 100;  // Set PWM duty cycle based on motor_speed (0-100%) - Convert percentage to 8-bit value
    }
    else
        duty = 0;

    //motor_state = state;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    ESP_LOGI(TAG1, ">>> Motor: %s at %d%% speed",(state?"ON":"OFF"),motor_speed);
}

void motor_set_speed(void *pvParameters)  // FreeRTOS task signature
{
    uint8_t last_speed = 0;  // Track previous speed
    
    while(1){
        if(motor_speed != last_speed){
            // Clamp speed to 0-100%
            if (motor_speed > 100)
                motor_speed = 100;

            // If motor is currently running, update PWM immediately
            if (motor_state) {
                uint32_t duty = (motor_speed * 255) / 100;
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
                ESP_LOGI(TAG1, ">>> Motor speed updated to %d%% (running)", motor_speed);
            }
            last_speed = motor_speed;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
