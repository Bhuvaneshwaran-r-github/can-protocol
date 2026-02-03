#include "can_utils.h"

// ============ Initialize CAN Socket ============ 
int initialize_can_socket(const char *ifname, struct can_filter *filter, int filter_count) {
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;

    if ((sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    strcpy(ifr.ifr_name, ifname);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl failed - is can interface up?");
        close(sock);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(sock);
        return -1;
    }

    if (filter != NULL && filter_count > 0) 
        setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, filter, filter_count);

    return sock;
}
