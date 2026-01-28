#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <gpiod.h>
#include <stdint.h>
#include "can_header.h"
#include "can_utils.h"

#define GPIO_SEATBELT   24

int can_socket;
int seatbelt_status;

struct gpiod_chip *gpio_chip;
struct gpiod_line *seatbelt_gpio;

static int initialize_gpio(void);
static void cleanup(void);

static int initialize_gpio(void) {
  gpio_chip = gpiod_chip_open("/dev/gpiochip0");
  if (!gpio_chip) {
    perror("GPIO chip open failed");
    return -1;
  }

  seatbelt_gpio = gpiod_chip_get_line(gpio_chip, GPIO_SEATBELT);
  if (!seatbelt_gpio) {
    perror("GPIO line acquisition failed");
    return -1;
  }

  if(gpiod_line_request_input_flags(seatbelt_gpio, "Seatbelt", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP)<0){
  	perror("GPIO request input failed");
	return -1;
  }

  printf("GPIO initialized successfully\n");
  return 0;
}

static void cleanup(void) {
  if (can_socket >= 0) 
    close(can_socket);

  if (seatbelt_gpio) 
    gpiod_line_release(seatbelt_gpio);

  if (gpio_chip) 
    gpiod_chip_close(gpio_chip);

  printf("Cleanup complete\n");
}

int main(int argc, char *argv[]) {

  struct can_frame frame, response;
  int frame_size = sizeof(struct can_frame);

  struct can_filter seatbelt_filter[1] = {
    {.can_id = SEATBELT_CAN_ID, .can_mask = CAN_SFF_MASK}
  };

  can_socket = initialize_can_socket(CAN_INF, seatbelt_filter, sizeof(seatbelt_filter));
  if (can_socket < 0) {
    fprintf(stderr, "Failed to initialize CAN socket\n");
    return 1;
  }

  if (initialize_gpio() < 0) {
    fprintf(stderr, "Failed to initialize GPIO\n");
    cleanup();
    return 1;
  }

  while (1) {
    read(can_socket, &response, frame_size);
      
    // SEATBELT STATUS READ
    seatbelt_status = !(gpiod_line_get_value(seatbelt_gpio));
    printf("gpio read =%d\n",seatbelt_status);

    // Check if this is an RTR frame requesting seatbelt status
    if ((response.can_id & CAN_RTR_FLAG) && 
        (response.can_id & CAN_SFF_MASK) == SEATBELT_CAN_ID) {
      printf("RTR Request received! Sending seatbelt status...\n");
        
      // Send response with seatbelt status
      memset(&frame, 0, frame_size);
      frame.can_id = SEATBELT_CAN_ID;  // Same ID, NO RTR flag
      frame.can_dlc = 1;
      frame.data[0] = seatbelt_status; // 0 = not fastened, 1 = fastened
      write(can_socket, &frame, frame_size);
    }
  }

  cleanup();
  close(can_socket);
  return 0;
}
