#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
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

// Function to log a message to the logfile_server.txt
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
  fp = fopen("../Log/logfile_server.txt", "a");
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

// Main function starts
int main(int argc, char *argv[]) {
  /* VARIABLES */
  struct shared_data data;  // Initialize struct
  int counter = 0;          // Initialize the counter for signal sending
  FILE *fp;                 // Logfile for logfile_server.txt
  int ch;                   // To chek the ESC key

  // Open the logfile to either create or delete the content
  fp = fopen("../Log/logfile_server.txt", "w");
  if (fp == NULL) {  // Check for errors
    perror("Error opening logfile");
    return -1;
  }
  // Close the file
  fclose(fp);

  /* WATCHDOG */
  pid_t Watchdog_pid;               // For watchdog PID
  pid_t blackboard_pid = getpid();  // Recieve the process PID

  // Get the file locations
  char *fnames[NUM_PROCESSES] = PID_FILE_SP;

  // Open the drone process tmp file to write its PID
  FILE *pid_fp = fopen(fnames[BLACKBOARD_SYM], "w");
  if (pid_fp == NULL) {  // Check for errors
    perror("Error opening Blackboard tmp file");
    return -1;
  }
  fprintf(pid_fp, "%d", blackboard_pid);
  fclose(pid_fp);  // Close file
  log_message(fp, "Printed my pid on the file ", INT_TYPE, &blackboard_pid);

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
  if (fscanf(watchdog_fp, "%d", &Watchdog_pid) != 1) {
    printf("Error reading Watchdog PID from file.\n");
    fclose(watchdog_fp);
    return -1;
  }

  // Close the file
  fclose(watchdog_fp);
  log_message(fp, "Recieved the watchdog pid ", INT_TYPE, &Watchdog_pid);

  /* FOR PIPES*/
  // For use of select
  fd_set rdfds;
  int retval;
  // Set the timeout to 0
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  // Seed the random number generator with the current time
  srand(time(NULL));
  int random_pipe = 0;  // Randomly chosen pipe

  // Creating file descriptors for clients
  int window_server[2];  // Window
  int server_window[2];
  int drone_server[2];  // Drone
  int server_drone[2];
  int socket_server[2];  // Socket
  int server_socket[2];

  // Scanning data from clients and close unnecessary fds
  // Window (send and recieve)
  sscanf(argv[1], "%d %d %d %d", &window_server[0], &window_server[1],
         &server_window[0], &server_window[1]);
  close(window_server[1]);
  close(server_window[0]);
  // Drone (send and recieve)
  sscanf(argv[2], "%d %d %d %d", &drone_server[0], &drone_server[1],
         &server_drone[0], &server_drone[1]);
  close(drone_server[1]);
  close(server_drone[0]);
  // Socket (send and recieve)
  sscanf(argv[3], "%d %d %d %d", &socket_server[0], &socket_server[1],
         &server_socket[0], &server_socket[1]);
  close(socket_server[1]);
  close(server_socket[0]);

  // Find the max fd
  int num_processes = 3;
  int FD[num_processes];
  FD[0] = drone_server[0];
  FD[1] = window_server[0];
  FD[2] = socket_server[0];

  // Get the highest-numbered file descriptor in any of the sets, plus 1
  int max_fd = -1;
  for (int i = 0; i < num_processes; i++) {
    if (FD[i] > max_fd) {
      max_fd = FD[i];
    }
  }

  // Start the loop
  while (1) {
    // Recieve data from the processes using select
    FD_ZERO(&rdfds);        // Reset the reading set
    FD_SET(FD[0], &rdfds);  // Add drone_server[0] to the set
    FD_SET(FD[1], &rdfds);  // Add window_server[0] to the set
    FD_SET(FD[2], &rdfds);  // Add socket_server[0] to the set
    retval = select(max_fd + 1, &rdfds, NULL, NULL, &tv);

    // Check for errors
    if (retval == -1) {
      perror("Server: select error");
      break;
    }
    // Data is available
    else if (retval) {
      // Generate a random number to choose between available pipes
      random_pipe = rand() % num_processes;

      // Data is available from the drone
      if (FD_ISSET(FD[0], &rdfds) && random_pipe == 0) {
        // Read the data and check for errors
        if (read(drone_server[0], &data, sizeof(struct shared_data)) == -1) {
          perror("Server: Error reading to pipe drone_server");
        }

        // Update the log
        log_message(fp, "Recieved info from Drone! ", INT_TYPE, &FD[0]);

        // Send the data to the window
        if (write(server_window[1], &data, sizeof(struct shared_data)) == -1) {
          perror("Server: Error writing to pipe server_window");
        }

        // Update the log
        log_message(fp, "Wrote data to window!", INT_TYPE, &FD[1]);

      }
      // Data is available from the window
      else if (FD_ISSET(FD[1], &rdfds) && random_pipe == 1) {
        // Read the data and check for errors
        if (read(window_server[0], &data, sizeof(struct shared_data)) == -1) {
          perror("Server: Error reading to pipe window_server");
        }

        // Update the log
        log_message(fp, "Recieved data from Window! ", INT_TYPE, &FD[1]);

        // Send data to the socket
        if (write(server_socket[1], &data, sizeof(struct shared_data)) == -1) {
          perror("Error writing to pipe server_socket");
        }

        // Update the log
        log_message(fp, "Wrote data to socket! ", INT_TYPE, &FD[1]);
      }
      // Data is available from the socket
      else if (FD_ISSET(FD[2], &rdfds) && random_pipe == 2) {
        // Read the data and check for errors
        if (read(socket_server[0], &data, sizeof(struct shared_data)) == -1) {
          perror("Server: error reading from pipe socket_server");
        }

        // Update the log
        log_message(fp, "Recieved data from Sockets! ", INT_TYPE, &FD[2]);

        // About targets
        if (data.type == TARGETS_SYM) {
          // Send data to window
          if (write(server_window[1], &data, sizeof(struct shared_data)) ==
              -1) {
            perror("Server: error writing to pipe server_window");
          }

        }
        // About obstacles
        else if (data.type == OBSTACLES_SYM) {
          // Send data to window and drone
          if (write(server_window[1], &data, sizeof(struct shared_data)) ==
              -1) {
            perror("Server: error writing to pipe server_window");
          }
          if (write(server_drone[1], &data, sizeof(struct shared_data)) == -1) {
            perror("Server: error writing to pipe server_drone");
          }
        } else {
          // Update the log
          log_message(fp, "ERROR, wrong message from socket! ", INT_TYPE,
                      &FD[2]);
        }

        // Update the log
        log_message(fp, "Wrote data to window! ", INT_TYPE, &FD[1]);
      }

      // Open the logfile to create a space
      fp = fopen("../Log/logfile_server.txt", "a");
      if (fp == NULL) {  // Check for errors
        perror("Error opening logfile");
        return -1;
      }
      fprintf(fp, "\n");  // Start from the new row
      fclose(fp);         // Close the file
    }

    // Check if the ESC button has been pressed
    if (data.ch == ESCAPE) {
      // Send data to the socket
      if (write(server_socket[1], &data, sizeof(struct shared_data)) == -1) {
        perror("Error writing to pipe server_socket");
      }
      // Update the log
      log_message(fp, "Escape! Wrote data to socket.", INT_TYPE, &FD[1]);

      // Log the exit
      log_message(fp, "Exiting the program", INT_TYPE, &FD[0]);

      sleep(1);  // Wait for 1 second
      break;     // Leave the loop
    }

    // Send a signal after certain iterations have passed
    if (counter == PROCESS_SIGNAL_INTERVAL * SPEED) {
      // Send signal to watchdog every process_signal_interval
      if (kill(Watchdog_pid, SIGUSR1) < 0) {
        perror("kill");
      }
      // Set counter to zero (start over)
      counter = 0;
    } else {
      // Increment the counter
      counter++;
    }

    // Wait for a certain time (faster)
    usleep(WAIT_TIME / SPEED);
  }

  // Clean up
  close(drone_server[0]);
  close(server_drone[1]);
  close(server_window[0]);
  close(server_window[1]);
  close(socket_server[0]);
  close(server_socket[1]);

  // End the blackboard program
  return 0;
}
