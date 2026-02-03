/*
 * engine.c - Engine Node
 * Listens for engine start/stop commands from dashboard
 * CAN ID: 0x102
 */

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include "can_header.h"
#include "can_utils.h"

int main(int argc, char *argv[]) {
  int sock, command;
  struct can_frame frame;

  struct can_filter filter[1] = {
    {.can_id = ENGINE_CAN_ID, .can_mask = CAN_SFF_MASK}
  };

  printf("Engine Node - Starting...\n");

  sock = initialize_can_socket(CAN_INF, filter, sizeof(filter));
  if (sock < 0) {
    fprintf(stderr, "Failed to initialize CAN socket\n");
    return 1;
  }

  printf("Engine Node listening on CAN ID: 0x%03X\n\n", ENGINE_CAN_ID);

  unsigned char engine_status = 0;  // 0 = OFF, 1 = ON

  while (1) {
    read(sock, &frame, sizeof(struct can_frame));
    command = frame.data[0];
    // Process engine command
    if (command == EN_ON) 
      engine_status = 1; 
    else if (command == EN_OFF) 
      engine_status = 0;
    
    printf("Engine Status: %s\n", engine_status ? "RUNNING" : "OFF");
  }

  close(sock);
  return 0;
}
