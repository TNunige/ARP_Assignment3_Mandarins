#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <ncurses.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "Include/constants.h"

// Function to log a message to the logfile_drone.txt
void log_message(FILE *fp, const char *message, enum DataType type,
                 void *data) {
  // Get the current time
  time_t now = time(NULL);
  struct tm *timenow;
  time(&now);
  timenow = gmtime(&now);

  // Format time as a string
  char time_str[26];
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timenow);

  // Open the logfile
  fp = fopen("../Log/logfile_drone.txt", "a");
  if (fp == NULL) {  // Check for errors
    perror("Error opening logfile");
    return;
  }

  // Print time and message to the file
  fprintf(fp, "[%s] %s ", time_str, message);

  // Print data based on data type
  switch (type) {
    case CHAR_TYPE:
      fprintf(fp, "%c\n", *((char *)type));
      break;
    case INT_TYPE:
      fprintf(fp, "%d\n", *((int *)data));
      break;
    case DOUBLE_TYPE:
      fprintf(fp, "%f\n", *((double *)data));
      break;
    default:
      fprintf(fp, "Unknown data type\n");
  }

  // Close the file
  fclose(fp);
}

// Function to calculate the repulsive force
double repulsive_force(FILE *fp, double coordinate, double (*obstacles)[2],
                       int dire, double co_y, double co_x, int num_obs) {
  double smaller_border = coordinate;                // Border near the 0 m
  double larger_border = (DRONE_AREA - coordinate);  // Border near the 100 m
  double force_rep = 0.0;                            // Repulsive force
  double obs_distance = 0.0;                         // Obstacle distance
  double euclidean_distance = 0.0;                   // Euclidean distance

  // Check if the drone is near the border
  if (smaller_border <= FORCE_FIELD) {  // Near the smaller border
    force_rep += ETA * pow(2, (1 / coordinate - 1 / smaller_border));
    // Update log
    log_message(fp, "Against the border! Direction ", INT_TYPE, &dire);
    log_message(fp, "Repulsive force ", DOUBLE_TYPE, &force_rep);

  } else if (larger_border <= FORCE_FIELD) {  // Near the larger border
    force_rep -= ETA * pow(2, (1 / coordinate - 1 / larger_border));
    // Update log
    log_message(fp, "Against the border! Direction ", INT_TYPE, &dire);
    log_message(fp, "Repulsive force ", DOUBLE_TYPE, &force_rep);
  }

  // Check if the drone is near an obstacle
  for (int i = 0; i < num_obs; i++) {
    // Calculate the euclidean distance
    euclidean_distance =
        sqrt(pow(co_y - obstacles[i][0], 2) + pow(co_x - obstacles[i][1], 2));

    if (euclidean_distance <= FORCE_FIELD) {
      // Adjust the repulsive force based on the distance
      force_rep -= ETA * pow(2, (1 / coordinate - 1 / euclidean_distance));

      // Update the log
      log_message(fp, "Near the obstacle! Direction ", INT_TYPE, &dire);
      log_message(fp, "Repulsive force ", DOUBLE_TYPE, &force_rep);
    }
  }
  // Return the repulsive force
  return force_rep;
}

// Function to calculate the new coordinates
double command_force(FILE *fp, double co[3][2], double input_force, int dire,
                     double (*obstacles)[2], int num_obs) {
  // Initialize variables
  double force_rep = 0.0;           // Repulsive force
  double force_sum = 0.0;           // Sum of all forces
  double coordinate = co[0][dire];  // Current coordinate
  double coordinate_y = co[0][1];   // y-coordinate
  double coordinate_x = co[0][0];   // x-coordinate

  // Calculate the repulsive force
  force_rep = repulsive_force(fp, coordinate, obstacles, dire, coordinate_y,
                              coordinate_x, num_obs);

  // Retrieve the sum of forces (input + repulsive)
  force_sum = input_force + force_rep;
  // Log the sum of forces
  log_message(fp, "In direction ", INT_TYPE, &dire);
  log_message(fp, "Sum of the forces are", DOUBLE_TYPE, &force_sum);

  // Calculate the general motion equation
  // Calculated in different parts for debugging purposes
  double part1 = pow(2, T) * force_sum;
  double part2 = co[1][dire] * (2 * M + T * K);
  double part3 = -co[2][dire] * M;
  double con = M + (T * K);

  coordinate = (part1 + part2 + part3) / con;
  log_message(fp, "New coordinate is", DOUBLE_TYPE, &coordinate);

  // Check for overflow
  if (coordinate > DBL_MAX) {
    perror("OVERFLOW!");
    return -1;
  }

  // Return the new coordinate
  return coordinate;
}

int main(int argc, char *argv[]) {
  /* VARIABLES */
  int ch;  // Key value
  // Force initialization
  double Fx = 0.0, Fy = .0;
  double real_y = DRONE_AREA / 2;
  double real_x = DRONE_AREA / 2;
  double new_real_y = 0.0, new_real_x = 0.0;
  double obs_y = 0.0;
  double obs_x = 0.0;
  int num_obs = 0, type = 0;
  // Initialize the struct
  struct shared_data data = {.real_y = 0.0,
                             .real_x = 0.0,
                             .ch = 0,
                             .num = 0,
                             .type = 0,
                             .co_y = 0.0,
                             .co_x = 0.0};
  // Save previous xi-1 and xi-2
  double coordinates[3][2] = {
      {real_y, real_x}, {real_y, real_x}, {real_y, real_x}};
  // Initialize the counter for signal sending
  int counter = 0;
  // Logfile
  FILE *fp;

  // Open the logfile to either create or delete the content
  fp = fopen("../Log/logfile_drone.txt", "w");
  if (fp == NULL) {  // Check for error
    perror("Error opening logfile");
    return -1;
  }
  fclose(fp);  // Close the file

  /* WATCHDOG */
  pid_t watchdog_pid;          // For watchdog PID
  pid_t drone_pid = getpid();  // Recieve the process PID

  // Get the file locations
  char *fnames[NUM_PROCESSES] = PID_FILE_SP;

  // Open the drone process tmp file to write its PID
  FILE *pid_fp = fopen(fnames[DRONE_SYM], "w");
  if (pid_fp == NULL) {  // Check for errors
    perror("Error opening Drone tmp file");
    return -1;
  }
  fprintf(pid_fp, "%d", drone_pid);
  fclose(pid_fp);  // Close file
  log_message(fp, "Printed my pid on the file ", INT_TYPE, &drone_pid);

  // Read watchdog PID
  FILE *watchdog_fp = NULL;
  struct stat sbuf;

  // Check if the file size is bigger than 0
  if (stat(PID_FILE_PW, &sbuf) == -1) {
    perror("error-stat");
    return -1;
  }
  // Wait until the file has data
  while (sbuf.st_size <= 0) {
    if (stat(PID_FILE_PW, &sbuf) == -1) {
      perror("error-stat");
      return -1;
    }
    usleep(50000);
  }

  // Open the watchdog tmp file
  watchdog_fp = fopen(PID_FILE_PW, "r");
  if (watchdog_fp == NULL) {  // Check for errors
    perror("Error opening watchdog PID file");
    return -1;
  }

  // Read the watchdog PID from the file
  if (fscanf(watchdog_fp, "%d", &watchdog_pid) != 1) {
    perror("Error reading Watchdog PID from file.\n");
    fclose(watchdog_fp);
    return -1;
  }

  // Close the file
  fclose(watchdog_fp);
  log_message(fp, "Recieved the watchdog pid ", INT_TYPE, &watchdog_pid);

  // Seed the random number generator with the current time
  srand(time(NULL));
  int random_pipe = 0;

  // Create a fifo to recieve the value of the key pressed
  char *myfifo = "/tmp/myfifo";
  mkfifo(myfifo, 0666);
  int fd;
  char str[MAX_LEN];
  char format_string[MAX_LEN] = "%d";

  /* PIPES */
  // Creating file descriptors
  int drone_server[2];
  int server_drone[2];

  // Scanning and close unnecessary fds
  sscanf(argv[2], "%d %d %d %d", &drone_server[0], &drone_server[1],
         &server_drone[0], &server_drone[1]);
  close(drone_server[0]);
  close(server_drone[1]);

  // For the use of select
  fd_set rdfds, rdfds2;
  int retval, retval2;
  // Set the timeout to 0 milliseconds
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  // Find the max fd
  int num_processes = 2, max_fd;
  int FD[num_processes];
  FD[0] = server_drone[0];

  double obstacles[MAX_OBJECT_SIZE][2];

  // Open fifo for reading the key from the keyboard process
  fd = open(myfifo, O_RDONLY);
  if (fd == -1) {
    perror("Opening fifo failed ");
    return -1;
  }
  // Add fifo to the fd array
  FD[1] = fd;

  // Highest-numbered file descriptor in any of the sets, plus 1
  max_fd = -1;
  for (int i = 0; i < num_processes; i++) {
    if (FD[i] > max_fd) {
      max_fd = FD[i];
    }
  }

  while (1) {
    FD_ZERO(&rdfds);        // Reset the reading set
    FD_SET(FD[0], &rdfds);  // Put the server fd in the set
    FD_SET(FD[1], &rdfds);  // Put the keyboard fd in the set
    retval = select(max_fd + 1, &rdfds, NULL, NULL, &tv);

    // Check for errors
    if (retval == -1) {
      perror("Select() failed ");
      return -1;
    }
    // Data is available
    else if (retval) {
      // Generate a random number to choose between available pipes
      random_pipe = rand() % num_processes;

      // If data from the keyboard is available
      if (FD_ISSET(FD[1], &rdfds) && random_pipe == 1) {
        log_message(fp, "Data from keyboard ", INT_TYPE, &FD[1]);
        //  Read from the file descriptor
        if ((read(fd, str, sizeof(str))) == -1) {
          perror("Drone: reading from fifo failed ");
          return -1;
        }
        // Convert the string back to integer
        ch = atoi(str);
        // Log the recieved key
        log_message(fp, "Recieved key ", INT_TYPE, &ch);

        // Update the character position based on user input
        switch (ch) {
          // Move left up 'w'
          case KEY_LEFT_UP:
            Fy += -Force;
            Fx += -Force;
            break;
          // Move left 's'
          case KEY_LEFT_s:
            Fy = 0.0;
            Fx += -Force;
            break;
          // Move left down 'x'
          case KEY_LEFT_DOWN:
            Fy += Force;
            Fx += -Force;
            break;
          // Move up 'e'
          case KEY_UP_e:
            Fy += -Force;
            Fx = 0.0;
            break;
          // Move down 'c'
          case KEY_DOWN_c:
            Fy += Force;
            Fx = 0.0;
            break;
          // Move right down 'v'
          case KEY_RIGHT_DOWN:
            Fy += Force;
            Fx += Force;
            break;
          // Move right 'f'
          case KEY_RIGHT_f:
            Fy = 0;
            Fx += Force;
            break;
          // Move up right 'r'
          case KEY_RIGHT_UP:
            Fy += -Force;
            Fx += Force;
            break;
          // Break
          case KEY_STOP:
            Fy = 0;
            Fx = 0;
            break;
        }
      }
      // If data from the server is available
      else if (FD_ISSET(FD[0], &rdfds) && random_pipe == 0) {
        // Update the log
        log_message(fp, "Data from obstacles ", INT_TYPE, &FD[0]);

        // Read data from obstacles and check for errors
        if (read(server_drone[0], &data, sizeof(struct shared_data)) == -1) {
          perror("Drone: reading from the server_drone failed");
        }
        // Save the data type
        type = data.type;
        // Check if it is about obstacles
        if (type == OBSTACLES_SYM) {
          num_obs = data.num;
          for (int i = 0; i < num_obs; i++) {
            // Get the new obstacle y coordinate
            obstacles[i][0] = data.co_y[i];
            // Get the new obstacle x coordinate
            obstacles[i][1] = data.co_x[i];
          }
        }
      }
    }
    // Data is not available
    else {
      // Set key to not pressed
      ch = 0;
      // Update the log
      log_message(fp, "Key not pressed ", INT_TYPE, &ch);
    }

    // // Log the values before calculating new coordinates
    log_message(fp, "Before recieving input force Fy", DOUBLE_TYPE, &Fy);
    log_message(fp, "Before recieving input force Fx", DOUBLE_TYPE, &Fx);

    log_message(fp, "Before recieving coordinates real_y", DOUBLE_TYPE,
                &coordinates[0][y_direction]);
    log_message(fp, "Before recieving coordinates real_x", DOUBLE_TYPE,
                &coordinates[0][x_direction]);

    // Check if the ESC key is pressed
    if (ch == ESCAPE) {
      // Update the log
      log_message(fp, "Exit key pressed ", INT_TYPE, &ch);

      // Write the data to server
      data.ch = ch;
      if (write(drone_server[1], &data, sizeof(struct shared_data)) == -1) {
        perror("Drone: error writing to pipe drone_server");
      }

      break;  // Leave the loop
    }
    // If 'K' is pressed restart game
    else if (ch == RESTART) {
      // Set the drone in the middle of the game arena
      new_real_y = DRONE_AREA / 2;
      new_real_x = DRONE_AREA / 2;
      // Log the reset and new coordinates
      log_message(fp, "RESET! New coordinates y ", DOUBLE_TYPE, &new_real_y);
      log_message(fp, "RESET! New coordinates x ", DOUBLE_TYPE, &new_real_x);

      // Reset all previous coordinates
      coordinates[0][0] = new_real_x;
      coordinates[0][1] = new_real_y;
      coordinates[2][0] = new_real_x;
      coordinates[2][1] = new_real_y;
      coordinates[1][0] = new_real_x;
      coordinates[1][1] = new_real_y;

      // Appoint forces to zero
      Fy = 0;
      Fx = 0;
    }
    // Game continues
    else {
      // Print the obstacles on the log
      for (int i = 0; i < num_obs; i++) {
        log_message(fp, "Obstacle coordinates real_y", DOUBLE_TYPE,
                    &obstacles[i][0]);
        log_message(fp, "Obstacle coordinates real_x", DOUBLE_TYPE,
                    &obstacles[i][0]);
      }
      // Calculate the new coordinates from the dynamic force
      // Force in the y direction
      coordinates[0][y_direction] =
          command_force(fp, coordinates, Fy, y_direction, obstacles, num_obs);
      // Force in the x direction
      coordinates[0][x_direction] =
          command_force(fp, coordinates, Fx, x_direction, obstacles, num_obs);

      // Calculate the new locations
      new_real_y = coordinates[0][y_direction];
      new_real_x = coordinates[0][x_direction];

      // Update previous coordinates
      coordinates[2][0] = coordinates[1][0];
      coordinates[2][1] = coordinates[1][1];
      coordinates[1][0] = new_real_x;
      coordinates[1][1] = new_real_y;
    }

    // Log the calculated forces and coordinates
    log_message(fp, "New coordinates real_y", DOUBLE_TYPE, &new_real_y);
    log_message(fp, "New coordinates real_x", DOUBLE_TYPE, &new_real_x);

    // Update shared memory with the key pressed and forces
    data.ch = ch;
    data.real_y = new_real_y;
    data.real_x = new_real_x;
    data.type = DRONE_SYM;

    // Write the data to server and check for errors
    if (write(drone_server[1], &data, sizeof(struct shared_data)) == -1) {
      perror("Error writing to pipe drone_server");
    }

    // Send a signal after certain iterations have passed
    if (counter == PROCESS_SIGNAL_INTERVAL) {
      log_message(fp, "Sent a signal to watchdog ", INT_TYPE, &watchdog_pid);
      // Send signal to watchdog every process signal interval
      if (kill(watchdog_pid, SIGUSR1) < 0) {
        perror("kill");
      }
      // Set counter to zero (start over)
      counter = 0;
    } else {
      // Increment the counter
      counter++;
    }

    // Open the logfile to create space
    fp = fopen("../Log/logfile_drone.txt", "a");
    if (fp == NULL) {  // Check for errors
      perror("Error opening logfile");
      return -1;
    }
    fprintf(fp, "\n");  // Start from the new row
    fclose(fp);         // Close the file

    // Wait for amount of time
    usleep(WAIT_TIME);
  }

  // Clean up
  close(fd);
  // Close pipes
  close(drone_server[1]);
  close(server_drone[0]);
  // Remove the FIFO file
  remove("/tmp/myfifo");

  // End the drone program
  return 0;
}