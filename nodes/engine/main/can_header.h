//node data
#define LI_ON 0x01 // Left Indicator
#define RI_ON 0x02 // Right Indicator
#define HAZARD_ON 0x03 // Hazard Indicator
#define HL_ON 0x04 // Headlight
#define IND_OFF 0x10 // Indicator OFF
#define HL_OFF 0x20 // Headlight OFF
#define EN_ON 0x08 // Engine
#define EN_OFF 0x00 // Engine OFF
#define DR 0x10 // Door
#define SB 0x20 // Seat Belt

//Vcan interface
#define CAN_INF "vcan5"

//node ids
#define COOLANT_CAN_ID 0x080
#define TYRE_PR_CAN_ID 0x099
#define BCM_CAN_ID 0x101
#define ENGINE_CAN_ID 0x102
#define DOOR_CAN_ID 0x103
#define SEATBELT_CAN_ID 0x104
