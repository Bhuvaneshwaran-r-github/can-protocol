/*
 * Thread-Based Dashboard for Raspberry Pi 3B+ (v2)
 * - Input Thread: Non-blocking user input handling
 * - Main Thread: Continuous display refresh (every 500ms)
 * - Sensor Thread: Receives temperature/pressure from CAN
 * - Engine Thread: Handles engine start/stop with safety checks
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
#include <poll.h>
#include <pthread.h>
#include "can_utils.h"
#include "can_header.h"

//Color codes
#define ESCAPE "\e["     //Marks the start of the color code
#define RESET  "\e[0m"   // Marks the end of the color code
#define RED    "31m"     // Red color
#define GRN    "32m"     // Green color
#define YEL    "33m"     // Yellow color
#define WHT    "37m"     // White color
#define BOLD   "1;"      // Bold text
#define BLINK  "5;"      // Blink text

#define print_clr(CON,FLAG,VAL,STR1,CLR1,STR2,CLR2) printf(CON); if(FLAG == VAL) printf(ESCAPE CLR1 STR1 RESET); else printf(ESCAPE CLR2 STR2 RESET);

// Warning thresholds
#define COOLANT_WARN_TEMP  90.0    // °C
#define TYRE_WARN_LOW_PSI  25.0    // PSI
#define TYRE_WARN_HIGH_PSI 40.0    // PSI

// Engine control signals
#define ENGINE_IDLE           0
#define ENGINE_START_REQUEST  1
#define ENGINE_STOP_REQUEST   2
#define ENGINE_EXIT           3

/* ============ Global State ============ */
int can_socket, rtr_socket;

// Sensor values
float coolant_temp  = 0.0;    // °C
float tyre_pressure = 0.0;    // PSI
int engine_command  = ENGINE_IDLE;

// Dashboard flags
int LI_Flag = 0;  // Left Indicator
int RI_Flag = 0;  // Right Indicator
int HL_Flag = 0;  // Headlight
int EN_Flag = 0;  // Engine
int DR_Flag = 0;  // Door
int SB_Flag = 0;  // Seat Belt

// Thread synchronization
pthread_mutex_t display_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t engine_mutex  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t input_mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t engine_cond    = PTHREAD_COND_INITIALIZER;

// Thread running flag
volatile int running = 1;

// Shared input variable for input thread
volatile int user_option = -1;

/* ============ Function Prototypes ============ */
void dashboard_status(void);
int can_rtr(int rtr_id, int rtr_dlc);
void *sensor_receiver_thread(void *arg);
void *engine_control_thread(void *arg);
void *input_thread(void *arg);
void process_option(int option, struct can_frame *frame, int frame_size);

// ============ Input Thread ============
// Runs in background, waits for user input without blocking main loop
void *input_thread(void *arg) {
    (void)arg;
    int temp_option;
    
    while (running) {
        if (scanf("%d", &temp_option) == 1) {
            pthread_mutex_lock(&input_mutex);
            user_option = temp_option;
            pthread_mutex_unlock(&input_mutex);
        }
    }
    return NULL;
}

// ============ Dashboard Display ============ 
void dashboard_status(void) {
    pthread_mutex_lock(&display_mutex);

    print_clr("Indicator: ", LI_Flag, 1, "<=",         BLINK BOLD YEL,       "<=", WHT);
    print_clr(" ● ",         RI_Flag, 1, "=>\n",       BLINK BOLD YEL,     "=>\n", WHT);
    print_clr("Headlight: ", HL_Flag, 1, "🟡\n",       WHT,                "⚪\n", WHT);
    print_clr("Door: ",      DR_Flag, 1, "Closed\n",   BOLD GRN,         "Open\n", BLINK BOLD RED);
    print_clr("Seat Belt: ", SB_Flag, 1, "Fastened\n", BOLD GRN, "Not Fastened\n", BLINK BOLD RED);
    print_clr("Engine: ",    EN_Flag, 1, "ON\n",       BOLD GRN,          "OFF\n", BOLD RED);

    printf("Coolant Temp:"ESCAPE BOLD YEL " %.1f °C " RESET, coolant_temp);
    print_clr(": ", (coolant_temp > COOLANT_WARN_TEMP), 1, "[WARNING: OVERHEATING!]\n",BLINK BOLD RED, "NORMAL\n", BOLD GRN);

    printf("Tyre Pressure: ");
    if (tyre_pressure < TYRE_WARN_LOW_PSI)
        printf(ESCAPE BLINK BOLD RED "%.1f PSI [WARNING: LOW PRESSURE!]"  RESET "\n", tyre_pressure);
    else if (tyre_pressure > TYRE_WARN_HIGH_PSI)
        printf(ESCAPE BLINK BOLD RED "%.1f PSI [WARNING: HIGH PRESSURE!]" RESET "\n", tyre_pressure);
    else if (tyre_pressure > 0)
        printf(ESCAPE BOLD GRN "%.1f PSI" RESET "\n", tyre_pressure);
    else
        printf("--.- PSI\n");

    pthread_mutex_unlock(&display_mutex);
}

// ============ RTR Request ============ 
int can_rtr(int rtr_id, int rtr_dlc) {
    struct can_frame frame;
    struct can_frame response;
    int frame_size = sizeof(struct can_frame);

    memset(&frame, 0, frame_size);
    frame.can_id   = rtr_id | CAN_RTR_FLAG;
    frame.can_dlc  = rtr_dlc;
    write(rtr_socket, &frame, frame_size);

    struct pollfd fds[1];
    fds[0].fd      = rtr_socket;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    int ret = poll(fds, 1, 1000);  // 1 second timeout

    if (ret > 0 && (fds[0].revents & POLLIN)) {
        read(rtr_socket, &response, sizeof(response));
        if ((response.can_id & CAN_SFF_MASK) == rtr_id)
            return (response.data[0] == 1) ? 1 : 0;
    }

    printf("Timeout: No response from node\n");
    return 0;
}

// ============ Sensor Receiver Thread ============ 
void *sensor_receiver_thread(void *arg) {
    (void)arg;
    struct can_frame frame;

    while (running) {
        read(can_socket, &frame, sizeof(frame));
        canid_t id = frame.can_id & CAN_SFF_MASK;
        
        unsigned int raw_value = (frame.data[0] << 24) |
                                 (frame.data[1] << 16) |
                                 (frame.data[2] << 8)  |
                                 frame.data[3];

        pthread_mutex_lock(&display_mutex);

        if (id == COOLANT_CAN_ID) 
            coolant_temp  = raw_value / 100.0;
        else if (id == TYRE_PR_CAN_ID)
            tyre_pressure = raw_value / 6894.76;

        pthread_mutex_unlock(&display_mutex);
    }

    return NULL;
}

// ============ Engine Control Thread ============ 
void *engine_control_thread(void *arg) {
    (void)arg;
    struct can_frame frame;
    int frame_size = sizeof(struct can_frame);

    while (running) {
        pthread_mutex_lock(&engine_mutex);

        while (running && (engine_command == ENGINE_IDLE))
            pthread_cond_wait(&engine_cond, &engine_mutex);

        if (!running || (engine_command == ENGINE_EXIT)) {
            pthread_mutex_unlock(&engine_mutex);
            break;
        }

        int cmd = engine_command;
        engine_command = ENGINE_IDLE;
        pthread_mutex_unlock(&engine_mutex);

        memset(&frame, 0, frame_size);

        if (cmd == ENGINE_START_REQUEST) {
            DR_Flag = can_rtr(DOOR_CAN_ID, 1);
            SB_Flag = can_rtr(SEATBELT_CAN_ID, 1);

            if ((DR_Flag == 1) && (SB_Flag == 1)) {
                frame.can_id  = ENGINE_CAN_ID;
                frame.can_dlc = 1;
                frame.data[0] = EN_ON;
                write(can_socket, &frame, frame_size);
                EN_Flag = 1;
            } else
                printf(ESCAPE BOLD RED "Error: Check Door and Seat Belt before starting engine\n" RESET);
        } else if (cmd == ENGINE_STOP_REQUEST) {
            frame.can_id  = ENGINE_CAN_ID;
            frame.can_dlc = 1;
            frame.data[0] = EN_OFF;
            write(can_socket, &frame, frame_size);
            EN_Flag = 0;
        }
    }

    return NULL;
}

// ============ Process User Option ============
void process_option(int option, struct can_frame *frame, int frame_size) {
    memset(frame, 0, frame_size);

    // BCM commands (options 1-6)
    if (option >= 1 && option <= 6) {
        frame->can_id  = BCM_CAN_ID;
        frame->can_dlc = 1;

        switch (option) {
            case 1: frame->data[0] = LI_ON;     RI_Flag = 0; LI_Flag = 1; break;
            case 2: frame->data[0] = RI_ON;     RI_Flag = 1; LI_Flag = 0; break;
            case 3: frame->data[0] = HAZARD_ON; RI_Flag = 1; LI_Flag = 1; break;
            case 4: frame->data[0] = IND_OFF;   RI_Flag = 0; LI_Flag = 0; break;
            case 5: frame->data[0] = HL_ON;     HL_Flag = 1;              break;
            case 6: frame->data[0] = HL_OFF;    HL_Flag = 0;              break;
        }
        write(can_socket, frame, frame_size);
    }
    // Engine commands (options 7-8)
    else if ((option == 7) || (option == 8)) {
        pthread_mutex_lock(&engine_mutex);
        engine_command = (option == 7) ? ENGINE_START_REQUEST : ENGINE_STOP_REQUEST;
        pthread_cond_signal(&engine_cond);
        pthread_mutex_unlock(&engine_mutex);
        usleep(100000);
    }
}

// ============ MAIN ============ 
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    struct can_frame frame;
    int frame_size = sizeof(struct can_frame);
    int option;

    struct can_filter main_filter[4] = {
        {.can_id = TYRE_PR_CAN_ID,  .can_mask = CAN_SFF_MASK},
        {.can_id = COOLANT_CAN_ID,  .can_mask = CAN_SFF_MASK},
        {.can_id = BCM_CAN_ID,      .can_mask = CAN_SFF_MASK},
        {.can_id = ENGINE_CAN_ID,   .can_mask = CAN_SFF_MASK},
    };
    
    struct can_filter rtr_filter[2] = {
        {.can_id = DOOR_CAN_ID,     .can_mask = CAN_SFF_MASK},
        {.can_id = SEATBELT_CAN_ID, .can_mask = CAN_SFF_MASK},
    };
    
    can_socket = initialize_can_socket(CAN_INF, main_filter, sizeof(main_filter));
    if (can_socket < 0) return 1;

    rtr_socket = initialize_can_socket(CAN_INF, rtr_filter, sizeof(rtr_filter));
    if (rtr_socket < 0) return 1;

    // Create threads
    pthread_t sensor_tid, engine_tid, input_tid;
    pthread_create(&sensor_tid, NULL, sensor_receiver_thread, NULL);
    pthread_create(&engine_tid, NULL, engine_control_thread,  NULL);
    pthread_create(&input_tid,  NULL, input_thread,           NULL);

    printf("Dashboard started. Display refreshes every 500ms.\n");
    sleep(1);

    // Main loop - continuous display refresh
    while (1) {
        system("clear");

        printf("=========== Dashboard ============\n");
        printf("1. Left Indicator\n");
        printf("2. Right Indicator\n");
        printf("3. Hazard Light\n");
        printf("4. Indicator OFF\n");
        printf("5. Headlight ON\n");
        printf("6. Headlight OFF\n");
        printf("7. Start Engine\n");
        printf("8. Stop Engine\n");
        printf("0. Exit\n");
        printf("===================================\n");
        dashboard_status();

        printf("\nEnter option: ");
        fflush(stdout);

        // Check if user entered an option (non-blocking)
        pthread_mutex_lock(&input_mutex);
        option = user_option;
        user_option = -1;  // Reset
        pthread_mutex_unlock(&input_mutex);

        if (option != -1) {
            if (option == 0) break;  // Exit
            process_option(option, &frame, frame_size);
        }

        //sleep(500000);  // Refresh display every 500ms
        sleep(1);  // Refresh display every 1sec
    }

    // Cleanup
    running = 0;

    // Cancel input thread (blocked on scanf)
    pthread_cancel(input_tid);

    // Signal engine thread to exit
    pthread_mutex_lock(&engine_mutex);
    engine_command = ENGINE_EXIT;
    pthread_cond_signal(&engine_cond);
    pthread_mutex_unlock(&engine_mutex);

    // Join threads
    pthread_join(input_tid,  NULL);
    pthread_join(sensor_tid, NULL);
    pthread_join(engine_tid, NULL);

    printf("\nDashboard shutdown complete.\n");
    close(can_socket);
    close(rtr_socket);
    return 0;
}

