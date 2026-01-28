//Color codes
#define ESCAPE "\e[" //Marks the start of the color code
#define RESET "\e[0m" // Marks the end of the color code

//Regular colors Text   
#define BLK "30m" //Black color
#define RED "31m" //Red color
#define GRN "32m" //Green color
#define YEL "33m" //Yellow color
#define BLU "34m" //Blue color
#define MAG "35m" //Magenta color
#define CYN "36m" //Cyan color
#define WHT "37m" //White color

//High intensty text
#define HBLK "90m" //Dark Gray
#define HRED "91m" //Light Red
#define HGRN "92m" //Light Green
#define HYEL "93m" //Light Yellow
#define HBLU "94m" //Light Blue
#define HMAG "95m" //Light Magenta
#define HCYN "96m" //Light Cyan
#define HWHT "97m" //Light White

#define BOLD "1;" //Bold text
#define UL "4;" //Underline text
#define BLINK "5;" //Blink text

#define print_clr(FUN,FLAG,VAL,STR1,CLR1,STR2,CLR2) printf(FUN); if(FLAG == VAL) printf(ESCAPE CLR1 STR1 RESET); else printf(ESCAPE CLR2 STR2 RESET);
