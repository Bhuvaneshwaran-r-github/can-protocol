#ifndef CAN_UTILS_H
#define CAN_UTILS_H

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int initialize_can_socket(const char *ifname, struct can_filter *filter, int filter_count);

#endif
