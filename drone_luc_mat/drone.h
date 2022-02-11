// Including libraries.

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <netdb.h>
#include <sys/socket.h>
#include <math.h>
#include <ncurses.h>

#define MAX_CHARGE 100
#define COLUMN 80   // Max X value.
#define ROW 40      // Max Y value.

FILE *logfile;                 // log file.
void SetupMap();

void signal_handler( int sig ) {

    // Function to handle the SIGWINCH signal. The OS send this signal to the process when the size of
    // the terminal changes. 

    if (sig == SIGWINCH) {
    // If the size of the terminal changes, clear and restart the grafic interface.
        endwin();
        initscr(); // Init the console screen.
        clear();
        SetupMap();
        refresh();
    }
}

// Struct we have to send to the master with the positions of the drone.

typedef struct drone_position_t
{   
    // timestamp of message
    time_t timestamp;
    // drone status
    int status;
    int x;
    int y;

} drone_position;

// Function: LogPrint(string).
// This function prints in the logfile with the date and the hour.

void LogPrint(char *string ) {
    time_t ltime = time(NULL);
    fprintf(logfile, "%.19s: %s", ctime( &ltime ), string );
    fflush(logfile);
}

// Declaring global variables.

bool flag = true;
bool originreached = false;
bool goalreached = false;

int battery = MAX_CHARGE;       // this variable takes into account the battry status
int canmove = 0;

drone_position actual_position = {.status = 1, .x = 10, .y = 10};        // Position of the drone in this moment.
drone_position new_position = {.status = 1};    // Position to reach by the drone.
drone_position desired_position;
drone_position landed;

int grid[ROW][COLUMN];
char str[50];                   // string buffer

time_t current_time;

// Function: CHECK(X).
// This function writes on the shell whenever a sistem call returns any kind of error.
// The function will print the name of the file and the line at which it found the error.
// It will end the check exiting with code 1.

#define CHECK(X) ({int __val = (X); (__val == -1 ? ({fprintf(stdout,"ERROR (" __FILE__ ":%d) -- %s\n",__LINE__,strerror(errno)); exit(-1);-1;}) : __val); })

// Function to print with curses with colors

void PRINT(int color, int position_x , int position_y, char c){
       attron(COLOR_PAIR(color));
       mvaddch(position_x, position_y, c);
       attroff(COLOR_PAIR(color));
}

// random float numbers
float float_rand(float min, float max)
{

    /* Function to generate a randomic position. */

    float scale = rand() / (float)RAND_MAX;
    return min + scale * (max - min); // [min, max]
}

// Implementing GoToOrigin() function, in order to get to the 0,0 point before
// starting to do the path coverage.

void GoToOrigin(){
    if (actual_position.x != 0) new_position.x = actual_position.x - 1;
    if (actual_position.y != 0) new_position.y = actual_position.y - 1;
    if (actual_position.x == 0 && actual_position.y == 0) {
        originreached = true;
        goalreached = true;
    }
}

// Implementing NewRandPosition() to check where the drone can move.

void NewRandPosition(){

    do{
        desired_position.x = rand()%80;
        desired_position.y = rand()%40;
    } while(grid[desired_position.y][desired_position.x] == 1);

    goalreached = false;
}

// Implementing NewPositionUpdate() to reach the goal position.

void NewPositionUpdate(){
    if(desired_position.x > actual_position.x) new_position.x = actual_position.x + 1;
    else if (desired_position.x <actual_position.x) new_position.x = actual_position.x -1;
    else (int)round(new_position.x = actual_position.x + float_rand(-0.8,0.8));
    if(desired_position.y > actual_position.y) new_position.y = actual_position.y + 1;
    else if (desired_position.y < actual_position.y) new_position.y = actual_position.y - 1;
    else (int)round(new_position.y = actual_position.y + float_rand(-0.8,0.8));

    if(actual_position.x == desired_position.x && actual_position.y == desired_position.y) goalreached = true;
}

// Implementing SetupColors() function, to create attributes of the colors.

void SetupColors() {

    if (!has_colors()) {
        endwin();
        printf("This terminal is not allowed to print colors.\n");
        exit(1);
    }

    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);     // Green on black.
    init_pair(2, COLOR_RED, COLOR_BLACK);       // Red on black.
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);    // Yellow on black.
    init_pair(4, COLOR_WHITE, COLOR_BLACK);     // White on black.
    init_pair(5, COLOR_BLACK, COLOR_BLACK);     // Totally black.
    init_pair(6, COLOR_WHITE, COLOR_WHITE);     // Totally white.
    init_pair(7,COLOR_WHITE, COLOR_BLACK);      // White on black.
    init_pair(8, COLOR_MAGENTA, COLOR_BLACK);   // Magenta on black.
    init_pair(9, COLOR_BLUE, COLOR_BLACK);      // Blue on black.
}

// Implementing SetupMap() function, to create the map.

void SetupMap() {
    for (int i = 0; i < 40; i++)
    {
        for (int j = 0; j < 80; j++){
            if(grid[i][j] == 0) {
                PRINT(6, i+1, j+1, '0');
            } else {
                PRINT(5, i+1, j+1, '1');
            }
            PRINT(7, 0, j + 1, '-');
            PRINT(7, 41, j +1, '-');
            PRINT(7, i + 1, 0, '|');
            PRINT(7, i + 1, 81, '|');

        }
    }
}

// Implementing loading_bar() function, to display graphically the battery of the drone.

void LoadingBattery(int percent, int buf_size)
{

    /*This is a simple graphical feature that we implemented. It is a loading bar that graphically
        rapresents the progress percentage of battery recharging.  */

    const int PROG = 30;
    int num_chars = (percent / (buf_size / 100)) * PROG / 100;

    PRINT(1, 43, 0, '[');

    int i;
    for (i = 0; i <= num_chars; i++) PRINT(8, 43, i+1, '#');

    for (int j = 0; j < PROG - num_chars - 1; j++) mvaddch(43, i+j+1, ' ');

    attron(COLOR_PAIR(1));
    sprintf(str,"] %d %% BATTERY  ", percent / (buf_size / 100));
    mvaddstr(43, 31, str);
    attroff(COLOR_PAIR(1));
    refresh();
}

// Implementing recharge() function, the one dedicated to the battery of the
// drone.

void RechargingBattery(int sockfd)
{
    attron(COLOR_PAIR(2));
    mvaddstr(44, 0, "Low battery, landing for recharging.");
    attroff(COLOR_PAIR(2));

    LogPrint("Low battery, landing for recharging.\n");

    landed = actual_position;
    landed.status = 0;
    landed.timestamp = time(NULL);

    CHECK(write(sockfd, &landed, sizeof(drone_position)));

    attron(COLOR_PAIR(1));
    mvaddstr(43, 50, "Recharging...");
    attroff(COLOR_PAIR(1));

    refresh();

    for (int i = 1; i <= MAX_CHARGE; i++)
    {
        
        usleep(50000);
        LoadingBattery(i, MAX_CHARGE); // graphical tool that represents a recharging bar
        PRINT(3, actual_position.y+1, actual_position.x+1, 'R'); 
        PRINT(3,actual_position.y , actual_position.x + 2, 'X');
        PRINT(3,actual_position.y , actual_position.x , 'X');
        PRINT(3,actual_position.y + 2, actual_position.x +2 , 'X');
        PRINT(3,actual_position.y + 2, actual_position.x, 'X');
        refresh();


    }
    battery = MAX_CHARGE;
    LogPrint("Battery recharged!\n");

    landed = actual_position;
    landed.status = 1;
    landed.timestamp = time(NULL);
    CHECK(write(sockfd, &landed, sizeof(drone_position))); // dummy command for idle status

    mvaddstr(43, 50, "               ");
    refresh();
}

void PrintGrid(){
    int number;
    for (int i = 0; i < 40; i++)
    {
        for (int j = 0; j < 80; j++){
                number = grid[i][j];
                char charValue = number+'0';
                if(charValue == '0'){
                    PRINT(2, i+1, j + 85, '0');
                }
                else {
                    PRINT(1, i+1, j + 85, '1');
                }
        }
    }
    refresh();
}