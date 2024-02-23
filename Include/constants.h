#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <netinet/in.h>

#define MAX_LEN 100       // Buffer to use during array initialization
#define WAIT_TIME 100000  // Time for loops to wait, using usleep()
#define SPEED 10          // Added speed for the server process

// Defining the keys for movement for ncurses
#define KEY_LEFT_UP 119
#define KEY_LEFT_s 115
#define KEY_LEFT_DOWN 120
#define KEY_UP_e 101
#define KEY_DOWN_c 99
#define KEY_RIGHT_DOWN 118
#define KEY_RIGHT_f 102
#define KEY_RIGHT_UP 114
#define KEY_STOP 100
#define ESCAPE 27    // Exit key
#define RESTART 107  // Restart key
#define GE -1

// Constants for the force calculation
#define ETA 10          // Positive scaling factor
#define T 1.0           // Integration interval for 1000 [ms] = 1 [s]
#define M 1.0           // Mass of the drone 1 [kg]
#define K 1.0           // Viscous coefficient [N*s*m]
#define y_direction 1   // Identify force in y direction
#define x_direction 0   // Identify force in x direction
#define Force 1.0       // Force is 1 [N]
#define DRONE_AREA 100  // Geofence dimension [m]
#define FORCE_FIELD 5   // Physical size of the repulsion area [m]

// Watchdog variables
#define NUM_PROCESSES 5  // 6 - with watchdog
#define BLACKBOARD_SYM 0
#define KEYBOARD_SYM 1
#define WINDOW_SYM 2
#define DRONE_SYM 3
#define SOCKET_SYM 4
#define OBSTACLES_SYM 5
#define TARGETS_SYM 6

// Number of process cycles before signaling watchdog
#define PROCESS_SIGNAL_INTERVAL 11
// Time in seconds of process inactivity before watchdog kills all processes
#define PROCESS_TIMEOUT_S 10

// Array for process files for PID sharing
#define PID_FILE_SP                                                         \
  {                                                                         \
    "/tmp/pid_file0", "/tmp/pid_file1", "/tmp/pid_file2", "/tmp/pid_file3", \
        "/tmp/pid_file4"                                                    \
  }
#define PID_FILE_PW "/tmp/pid_filew"  // Filename for watchdog process pid
// Names of the process for logfile
#define PROCESS_NAMES \
  { "Server", "Keyboard", "Window", "Drone", "Socket" }

// Define enum for different data types
enum DataType {
  CHAR_TYPE,
  INT_TYPE,
  DOUBLE_TYPE,
  STRING_TYPE,
};

// SOCKET
#define MAX_MSG_LEN 1024
#define MAX_OBJECT_SIZE 20
#define OBST_TIME 10         // seconds
#define MAX_TARG_ARR_SIZE 2  // Targets: can't be higher than 20
#define MAX_OBST_ARR_SIZE 2  // Obstacles: can't be higher than 20
#define CLIENTS 2            // Number of clients: targets and obstacles
#define ACK_MSG_TARG \
  "TI"  // first message to be sent after a connection is established
#define ACK_MSG_OBST \
  "OI"  // first message to be sent after a connection is established
#define MSG_TARG "T"
#define MSG_OBST "O"
#define EXIT_MSG "STOP"
#define GAME_END "GE"
// Window parameter
double WINDOW_HEIGHT;
double WINDOW_WIDTH;
int INT_WINDOW_HEIGHT;
int INT_WINDOW_WIDTH;

// Defining a struct to hold the data exchanged in shared memory
struct shared_data {
  int ch;         // Key pressed by the user
  double real_y;  // y coordinate of the drone
  double real_x;  // x coordinate of the drone
  int type;  // Type of the process: 3 for drone, 5 for obstacles, 6 for targets
  int num;   // Number of targets or obstacles
  double co_y[MAX_OBJECT_SIZE];  // y coordinate of the object
  double co_x[MAX_OBJECT_SIZE];  // x coordinate of the object
};

/* a structure to contain an internet address
   defined in the include file in.h */
// struct sockaddr_in {
//   short sin_family; /* should be AF_INET */
//   u_short sin_port;
//   struct in_addr sin_addr;
//   char sin_zero[8];
// };

// struct in_addr {
//   unsigned long s_addr;
// };

#endif
