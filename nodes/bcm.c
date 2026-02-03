#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <gpiod.h>
#include <stdint.h>
#include "can_header.h"
#include "can_utils.h"

#define BLINK_ON_TIME   500000
#define BLINK_OFF_TIME  500000
#define POLL_TIMEOUT    100

int can_socket;
int ind_state;
int hl_state;

struct gpiod_chip *gpio_chip;
struct gpiod_line *right_ind_gpio;
struct gpiod_line *left_ind_gpio;
struct gpiod_line *headlight_gpio;

static int initialize_gpio(void);
static void handle_can_event(void);
static void update_outputs(int blink_val);
static void cleanup(void);

static int initialize_gpio(void) {
  gpio_chip = gpiod_chip_open("/dev/gpiochip0");
  if (!gpio_chip) {
    perror("GPIO chip open failed");
    return -1;
  }

  right_ind_gpio = gpiod_chip_get_line(gpio_chip, 17);
  left_ind_gpio  = gpiod_chip_get_line(gpio_chip, 27);
  headlight_gpio = gpiod_chip_get_line(gpio_chip, 22);

  if (!right_ind_gpio || !left_ind_gpio || !headlight_gpio) {
    perror("GPIO line acquisition failed");
    return -1;
  }

  if (gpiod_line_request_output(right_ind_gpio, "Right_Indicator", 0) < 0 ||
      gpiod_line_request_output(left_ind_gpio, "Left_Indicator", 0) < 0 ||
      gpiod_line_request_output(headlight_gpio, "Headlight", 0) < 0) {
    perror("GPIO line request failed");
    return -1;
  }

  printf("GPIO initialized successfully\n");
  return 0;
}

static void handle_can_event(void) {
  struct can_frame frame;
  int nbytes;

  nbytes = read(can_socket, &frame, sizeof(struct can_frame));
  if (nbytes < 0) {
    perror("CAN read failed");
    return;
  }

  if (nbytes < 1)
    return;

  uint8_t cmd = frame.data[0];
  switch (cmd)
  {
    case LI_ON: ind_state = 1; break;
    case RI_ON: ind_state = 2; break;
    case HAZARD_ON: ind_state = 3; break;
    case IND_OFF: ind_state = 0; break;
    case HL_ON: hl_state = 1; break;
    case HL_OFF: hl_state = 0; break;
    default: break;
  }
}

static void update_outputs(int blink_val) {
  int r = 0, l = 0, h = 0;

  if ((ind_state == 2) || (ind_state == 3))
    r = blink_val;

  if ((ind_state == 1) || (ind_state == 3)) 
    l = blink_val;

  if (hl_state == 1) 
    h = 1;

  gpiod_line_set_value(right_ind_gpio, r);
  gpiod_line_set_value(left_ind_gpio, l);
  gpiod_line_set_value(headlight_gpio, h);

  //printf("[OUTPUT] Right=%d, Left=%d, Headlight=%d\n", r, l, h);
}

static void cleanup(void) {
  if (can_socket >= 0) 
    close(can_socket);

  if (right_ind_gpio) 
    gpiod_line_release(right_ind_gpio);
  if (left_ind_gpio) 
    gpiod_line_release(left_ind_gpio);
  if (headlight_gpio) 
    gpiod_line_release(headlight_gpio);

  if (gpio_chip) 
    gpiod_chip_close(gpio_chip);

  printf("Cleanup complete\n");
}

int main(int argc, char *argv[]) {
  struct pollfd fds[1];
  int nfds = 1;
  int ret;

  struct can_filter bcm_filter[1] = {
    {.can_id = BCM_CAN_ID, .can_mask = CAN_SFF_MASK}
  };

  can_socket = initialize_can_socket(CAN_INF, bcm_filter, sizeof(bcm_filter));
  if (can_socket < 0) {
    fprintf(stderr, "Failed to initialize CAN socket\n");
    return 1;
  }

  if (initialize_gpio() < 0) {
    fprintf(stderr, "Failed to initialize GPIO\n");
    cleanup();
    return 1;
  }

  ind_state = 0;
  hl_state = 0;

  fds[0].fd = can_socket;
  fds[0].events = POLLIN;
  fds[0].revents = 0;

  printf("\n=== Entering Event Loop ===\n");
  printf("Waiting for CAN events...\n\n");

  while (1) {
    ret = poll(fds, nfds, POLL_TIMEOUT);

    if (ret < 0) {
      perror("poll failed");
      break;
    }

    if (ret > 0 && (fds[0].revents & POLLIN)) {
      handle_can_event();
      fds[0].revents = 0;
    }

    if (ind_state != 0) {
      update_outputs(1);
      usleep(BLINK_ON_TIME);

      ret = poll(fds, nfds, 0);
      if (ret > 0 && (fds[0].revents & POLLIN)) {
        handle_can_event();
        fds[0].revents = 0;
      }

      update_outputs(0);
      usleep(BLINK_OFF_TIME);

      ret = poll(fds, nfds, 0);
      if (ret > 0 && (fds[0].revents & POLLIN)) {
        handle_can_event();
        fds[0].revents = 0;
      }
    } else {
      update_outputs(0);
    }
  }

  cleanup();
  return 0;
}
