#include <arpa/inet.h>
#include <fcntl.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "Include/constants.h"

// Function to log a message to the logfile_server_socket2.txt
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
  fp = fopen("../Log/logfile_server_socket2.txt", "a");
  if (fp == NULL) {  // Check for errors
    perror("Error opening logfile");
    return;
  }

  // Print time and message to the file
  fprintf(fp, "[%s] %s ", time_str, message);

  // Print data based on data type
  switch (type) {
    case CHAR_TYPE:
      fprintf(fp, "%c\n", *((char *)data));
      break;
    case INT_TYPE:
      fprintf(fp, "%d\n", *((int *)data));
      break;
    case DOUBLE_TYPE:
      fprintf(fp, "%f\n", *((double *)data));
      break;
    case STRING_TYPE:
      fprintf(fp, "%s\n", ((char *)data));
      break;
    default:
      fprintf(fp, "Unknown data type\n");
  }

  // Close the file
  fclose(fp);
}

// Function to print error message
void error(char *msg) {
  perror(msg);
  exit(1);
}

int check_echo(FILE *fp, char buffer[]) {
  int echo = -1;
  // GE message
  if (strcmp(buffer, GAME_END) == 0) {
    // printf("Recieved echo the Game End target: %s\n", buffer);
    log_message(fp, "Recieved echo GE: ", STRING_TYPE, buffer);
    return echo = 0;
  }
  // Window size
  else if (strcmp(buffer, "100.000, 100.000") == 0) {
    log_message(fp, "Recieved echo window size:", STRING_TYPE, buffer);
    return echo = 0;
  }
  // STOP message
  else if (strcmp(buffer, EXIT_MSG) == 0) {
    log_message(fp, "Recieved echo STOP:", STRING_TYPE, buffer);
    return echo = 2;
  }
  // Wrong echo message
  else {
    error("Wrong echo message from client");
    // return echo = 1;
  }
}

void write_client(FILE *fp, char buffer[], const char *msg, int fd) {
  int n;
  bzero(buffer, MAX_MSG_LEN);
  strcpy(buffer, msg);
  buffer[strlen(buffer)] = '\0';  // Add null-terminating character

  // Send data to client
  n = write(fd, buffer, strlen(buffer));
  if (n < 0) {
    error("ERROR writing to client");
  }

  // Update the log file
  log_message(fp, "Write to client:", STRING_TYPE, buffer);
}

char *read_client(char buffer[], int fd) {
  int n;
  bzero(buffer, MAX_MSG_LEN);

  // Read data to client
  n = read(fd, buffer, MAX_MSG_LEN - 1);
  if (n < 0) {
    error("ERROR reading from client");
  }

  return buffer;
}

// Main function starts
int main(int argc, char *argv[]) {
  /* VARIABLES */
  struct shared_data data;  // Struct initalization
  int counter = 0;          // Initialize the counter for signal sending
  FILE *fp;                 // Logfile for logfile_server_socket2.txt
  int ch;                   // To chek the key pressed by the usser
  // Socket variables
  int sockfd, newsockfd, portno, clilen;
  struct sockaddr_in serv_addr, cli_addr;
  // Client variables
  int num_clients = 0;
  int client_fds[CLIENTS];
  char buffer[MAX_MSG_LEN];
  int n, fd;
  int echo_target = 0, echo_obstacle = 0;
  // Select variables
  fd_set rdfds;
  int retval;
  struct timeval tv;
  tv.tv_sec = 0;  // Set the timeout to 0
  tv.tv_usec = 0;
  // Seed the random number generator with the current time
  srand(time(NULL));
  int random = 0;  // Randomly chosen process

  // Check if all the required arguments were given
  if (argc < 2) {
    error("ERROR, no port provided\n");
  }
  // Save the portnumber
  else {
    portno = atoi(argv[1]);  // !! check the port number argument location
    // Update the log file
    log_message(fp, "Given port number:", INT_TYPE, &portno);
  }

  // Initialise all client_fds[] to 0 so not checked
  for (int i = 0; i < CLIENTS; i++) {
    client_fds[i] = 0;
  }

  // Open the logfile to either create or delete the content
  fp = fopen("../Log/logfile_server_socket2.txt", "w");
  if (fp == NULL) {  // Check for errors
    perror("Error opening logfile");
    return -1;
  }
  // Close the file
  fclose(fp);

  /* WATCHDOG */
  pid_t watchdog_pid;           // For watchdog PID
  pid_t socket_pid = getpid();  // Recieve the process PID

  // Get the file locations
  char *fnames[NUM_PROCESSES] = PID_FILE_SP;

  // Open the keyboard process tmp file to write its PID
  FILE *pid_fp = fopen(fnames[SOCKET_SYM], "w");
  if (pid_fp == NULL) {  // Check for errors
    perror("Error opening socket tmp file");
    return -1;
  }
  fprintf(pid_fp, "%d", socket_pid);
  fclose(pid_fp);  // Close file
  // Update log file
  log_message(fp, "My PID: ", INT_TYPE, &socket_pid);

  // Read watchdog pid
  FILE *watchdog_fp;
  struct stat sbuf;

  // Check if the file size is bigger than 0
  if (stat(PID_FILE_PW, &sbuf) == -1) {
    perror("error-stat");
    return -1;
  }
  // Waits until the file has data
  while (sbuf.st_size <= 0) {
    if (stat(PID_FILE_PW, &sbuf) == -1) {
      perror("error-stat");
      return -1;
    }
    usleep(50000);
  }

  // Open the watchdog tmp file
  watchdog_fp = fopen(PID_FILE_PW, "r");
  if (watchdog_fp == NULL) {
    perror("Error opening watchdog PID file");
    return -1;
  }

  // Read the watchdog PID from the file
  if (fscanf(watchdog_fp, "%d", &watchdog_pid) != 1) {
    printf("Error reading watchdog PID from file.\n");
    fclose(watchdog_fp);
    return -1;
  }

  // Close the file
  fclose(watchdog_fp);
  // Update the log file
  log_message(fp, "Watchdog PID: ", INT_TYPE, &watchdog_pid);

  /*SOCKETS*/
  // Initialize the socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  // Check for socket creation errors
  if (sockfd < 0) {
    error("ERROR opening socket");
  }

  // Sets all values in a buffer to zero
  bzero((char *)&serv_addr, sizeof(serv_addr));

  // Prepare the server aadress structure
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  // Binds a socket to the current host and port number
  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding");

  // Listen for connections
  if (listen(sockfd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }
  // Update the log file
  log_message(fp, "Started listening to new clients", INT_TYPE, &sockfd);

  // Size of the client address
  clilen = sizeof(cli_addr);

  /* PIPES */
  // Creating file descriptors for server
  int socket_server[2];
  int server_socket[2];

  // Scanning the fds given as arguments
  sscanf(argv[2], "%d %d %d %d", &socket_server[0], &socket_server[1],
         &server_socket[0], &server_socket[1]);
  // Closing unecessary fds
  close(socket_server[0]);
  close(server_socket[1]);

  // Get the highest-numbered file descriptor in any of the sets
  int num_processes = 2;
  int FD[num_processes + CLIENTS];
  FD[0] = server_socket[0];
  FD[1] = sockfd;

  int max_fd = -1;

  while (1) {
    FD_ZERO(&rdfds);        // Clear the socket set
    FD_SET(FD[0], &rdfds);  // Add server_socket[0] to set
    FD_SET(FD[1], &rdfds);  // Add master socket to set

    // Find the max fd
    for (int i = 0; i < num_processes; i++) {
      if (FD[i] > max_fd) {
        max_fd = FD[i];  // Update max_fd if needed
      }
    }

    // Add child sockets to set
    for (int i = 0; i < CLIENTS; i++) {
      // Socket descriptor
      fd = client_fds[i];
      // If valid socket descriptor then add to read list
      if (fd > 0) {
        FD_SET(fd, &rdfds);
      }
      // Highest file descriptor number
      if (fd > max_fd) {
        max_fd = fd;
      }
    }

    // Select: wait for data to be available
    retval = select(max_fd + 1, &rdfds, NULL, NULL, &tv);
    // Check for errors
    if (retval == -1) {
      perror("Socket Server: select error");
      break;
    }
    // Data is available
    else if (retval) {
      // Generate a random number to choose between processes with data
      random = rand() % num_processes;

      // Server (pipe connection)
      if (FD_ISSET(FD[0], &rdfds) && random == 0) {
        // Read the data and check for errors
        if (read(server_socket[0], &data, sizeof(struct shared_data)) == -1) {
          error("Server: Error reading to pipe server_socket");
        }

        // Update the log
        log_message(fp, "Recieved info from Server! ", INT_TYPE, &FD[0]);

        // Send the data to the clients trough socket
        // Check if the GE message
        if (data.ch == GE) {
          // Write to target
          write_client(fp, buffer, "GE", client_fds[0]);

          // Check for echo
          echo_target = 1;
        }
        // Check if the exit key has been pressed
        else if (data.ch == ESCAPE) {
          // Send data to targets and obstacles
          write_client(fp, buffer, "STOP", client_fds[0]);
          write_client(fp, buffer, "STOP", client_fds[1]);

          // Check for echo
          echo_obstacle = 1;
          echo_target = 1;

        } else {
          error("Wrong message from server");
        }
      }
      // Master socket (first client connection)
      else if (FD_ISSET(FD[1], &rdfds) && random == 1) {
        // Accept a new client
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
          error("ERROR on accept");
        }
        // Update the log file
        log_message(fp, "New client connection", INT_TYPE, &newsockfd);

        // Read from the client
        read_client(buffer, newsockfd);
        // printf("Recieved the first message from client: %s\n", buffer);
        log_message(fp, "Recieved the first message from client:", STRING_TYPE,
                    buffer);

        // Check if the first message from target
        if (strcmp(buffer, ACK_MSG_TARG) == 0) {
          log_message(fp, "First message from target", STRING_TYPE, buffer);
          // Add client to the FD_SET
          client_fds[0] = newsockfd;
          num_clients++;
          num_processes++;

        }
        // Check if the first message from obstacles
        else if (strcmp(buffer, ACK_MSG_OBST) == 0) {
          log_message(fp, "First message from obstacles", STRING_TYPE, buffer);
          // Add client to the FD_SET
          client_fds[1] = newsockfd;
          num_clients++;
          num_processes++;
        }

        // Send echo
        buffer[strlen(buffer)] = '\0';
        n = write(newsockfd, buffer, strlen(buffer));
        if (n < 0) {
          error("ERROR writing back to the client\n");
        }
        // Update the log file
        log_message(fp, "Sent echo to client", STRING_TYPE, &buffer);

        // Wait a certain time
        usleep(WAIT_TIME / SPEED);

        // Send window size
        write_client(fp, buffer, "100.000, 100.000", newsockfd);
        log_message(fp, "Sent window size to client", STRING_TYPE, &buffer);

        // Wait for echo
        read_client(buffer, newsockfd);
        echo_target = check_echo(fp, buffer);
      }
      // Targets
      else if (FD_ISSET(client_fds[0], &rdfds) && random == 1) {
        // Read from the client
        read_client(buffer, client_fds[0]);
        // Update the log file
        log_message(fp, "Recieved a message from target", STRING_TYPE, buffer);

        // Check if need to wait for echo
        if (echo_target == 1) {
          echo_target = check_echo(fp, buffer);
          if (echo_target == 2) {
            log_message(fp, "Recieved STOP message. Echo target", INT_TYPE,
                        &echo_target);
          }

          // Check if the echo has been recieved from both clients
          if (echo_target == 2 && echo_obstacle == 2) {
            // Exit loop
            log_message(fp, "Recieved both exit messages", STRING_TYPE,
                        &buffer);
            break;
          }
        }
        // Check if it is new targets
        else if (buffer[0] == 'T') {
          // Send echo
          buffer[strlen(buffer)] = '\0';
          n = write(client_fds[0], buffer, strlen(buffer));
          if (n < 0) {
            error("ERROR writing back to the client\n");
          }
          // Update the log file
          log_message(fp, "Sent echo to client", STRING_TYPE, &buffer);

          // Save the type
          data.type = TARGETS_SYM;  // target type
          // Update the log file
          log_message(fp, "Data type:", INT_TYPE, &data.type);

          // Save the number of objects
          char *ptr = buffer + 1;
          sscanf(ptr, "[%d]", &data.num);
          log_message(fp, "Number of targets:", INT_TYPE, &data.num);

          // Move the buffer
          ptr = buffer + 4;

          // Save x and y components to the array
          for (int i = 0; i < data.num; i++) {
            sscanf(ptr, "%lf,%lf", &data.co_x[i], &data.co_y[i]);
            // Update the log file
            log_message(fp, "Recieved target x", DOUBLE_TYPE, &data.co_x[i]);
            log_message(fp, "Recieved target y", DOUBLE_TYPE, &data.co_y[i]);

            // Move past '|'
            ptr = strchr(ptr, '|');
            if (ptr != NULL) {
              ptr++;
            }
          }

          // Send the struct data through the pipe to server
          if (write(socket_server[1], &data, sizeof(struct shared_data)) ==
              -1) {
            error("Write target objects to server failed");
          }
          // Update the log file
          log_message(fp, "Wrote data to server:", INT_TYPE, &socket_server[1]);

        } else {
          error("Recieved wrong message from targets\n");
        }
      }
      // Obstacles
      else if (FD_ISSET(client_fds[1], &rdfds) && random == 2) {
        // Read from the client
        read_client(buffer, client_fds[1]);
        // Update the log file
        log_message(fp, "Recieved a message from obstacles", STRING_TYPE,
                    buffer);

        // Check if need to wait for echo
        if (echo_obstacle == 1) {
          echo_obstacle = check_echo(fp, buffer);
          if (echo_obstacle == 2) {
            log_message(fp, "Recieved STOP message. Echo obstacle", INT_TYPE,
                        &echo_obstacle);
          }

          // Check if the echo has been recieved from both clients
          if (echo_target == 2 && echo_obstacle == 2) {
            // Exit loop
            log_message(fp, "Recieved both exit messages", STRING_TYPE,
                        &buffer);
            break;
          }
        }
        // Check if it is new obstacles
        else if (buffer[0] == 'O') {
          // Send echo
          buffer[strlen(buffer)] = '\0';
          n = write(client_fds[1], buffer, strlen(buffer));
          if (n < 0) {
            error("ERROR writing back to the client\n");
          }
          // Update the log file
          log_message(fp, "Sent echo to client", STRING_TYPE, &buffer);

          // Save the type
          data.type = OBSTACLES_SYM;  // obstacle type
          // Update the log file
          log_message(fp, "Data type:", INT_TYPE, &data.type);

          // Save the number of objects
          char *ptr = buffer + 1;
          sscanf(ptr, "[%d]", &data.num);
          // Update the log file
          log_message(fp, "Number of obstacles:", INT_TYPE, &data.num);

          // Move the buffer
          ptr = buffer + 4;

          // Save x and y components to the array
          for (int i = 0; i < data.num; i++) {
            sscanf(ptr, "%lf,%lf", &data.co_x[i], &data.co_y[i]);
            // Update the log file
            log_message(fp, "Recieved obstacle x", DOUBLE_TYPE, &data.co_x[i]);
            log_message(fp, "Recieved obstacle y", DOUBLE_TYPE, &data.co_y[i]);

            // Move past '|'
            ptr = strchr(ptr, '|');
            if (ptr != NULL) {
              ptr++;
            }
          }

          // Send the struct data through the pipe to server
          if (write(socket_server[1], &data, sizeof(struct shared_data)) ==
              -1) {
            error("Write obstacle objects to server failed");
          }
          // Update the log file
          log_message(fp, "Wrote data to server:", INT_TYPE, &socket_server[1]);

        }
        // Wrong message
        else {
          error("Recieved wrong message from obstacles\n");
        }
      }
    }

    // Send a signal after certain iterations have passed
    if (counter == PROCESS_SIGNAL_INTERVAL) {
      log_message(fp, "Sending a signal to watchdog", INT_TYPE, &watchdog_pid);
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

    // Wait for a certain time
    usleep(WAIT_TIME);
  }

  // Clean up
  close(sockfd);         // Close the server socket
  close(client_fds[0]);  // Close the client socket targets
  close(client_fds[1]);  // Close the client socket obstacles
  close(socket_server[1]);
  close(server_socket[0]);

  return 0;
}