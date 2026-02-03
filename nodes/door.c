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

#define GPIO_DOOR     23

struct gpiod_chip *gpio_chip;
struct gpiod_line *door_gpio;

int can_socket;
int door_status;

static int initialize_gpio(void);
static void cleanup(void);

static int initialize_gpio(void) {
  gpio_chip = gpiod_chip_open("/dev/gpiochip0");
  if (!gpio_chip) {
    perror("GPIO chip open failed");
    return -1;
  }

  door_gpio = gpiod_chip_get_line(gpio_chip, GPIO_DOOR);
  if (!door_gpio) {
    perror("GPIO line acquisition failed");
    return -1;
  }
  
  gpiod_line_request_input_flags(door_gpio, "Door", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);

  printf("GPIO initialized successfully\n");
  return 0;
}

static void cleanup(void) {
  if (can_socket >= 0) 
    close(can_socket);

  if (door_gpio) 
    gpiod_line_release(door_gpio);

  if (gpio_chip) 
    gpiod_chip_close(gpio_chip);

  printf("Cleanup complete\n");
}

int main(int argc, char *argv[]) {

  struct can_frame frame, response;
  int frame_size = sizeof(struct can_frame);

  struct can_filter door_filter[1] = {
    {.can_id = DOOR_CAN_ID, .can_mask = CAN_SFF_MASK}
  };

  can_socket = initialize_can_socket(CAN_INF, door_filter, sizeof(door_filter));
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
    if (read(can_socket, &response, frame_size) < 0) {
      perror("read failed");
      continue;
    }

    // DOOR STATUS READ
    door_status = !gpiod_line_get_value(door_gpio);
    printf("gpio read=%d\n", door_status);

    // Check if this is an RTR frame requesting door status
    if ((response.can_id & CAN_RTR_FLAG) && 
        (response.can_id & CAN_SFF_MASK) == DOOR_CAN_ID) {
      printf("RTR Request received! Sending door status...\n");

      // Send response with door status
      memset(&frame, 0, frame_size);
      frame.can_id = DOOR_CAN_ID;  // Same ID, NO RTR flag
      frame.can_dlc = 1;
      frame.data[0] = door_status; // 0 = locked, 1 = open
      write(can_socket, &frame, frame_size);
    }
  }

  cleanup();
  return 0;
}
