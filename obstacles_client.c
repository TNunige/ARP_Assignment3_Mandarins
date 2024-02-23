#include <fcntl.h>
#include <math.h>
#include <netdb.h>
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

// Function to log a message to the logfile_obstacles_client.txt
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
  fp = fopen("../Log/logfile_obstacles_client.txt", "a");
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
  exit(0);
}

// Function to send messages to server
void write_server(FILE *fp, char buffer[], const char *msg, int fd) {
  int n;
  bzero(buffer, MAX_MSG_LEN);
  strcpy(buffer, msg);
  buffer[strlen(buffer)] = '\0';  // Add null-terminating character

  // Send data to server
  n = write(fd, buffer, strlen(buffer));
  if (n < 0) {
    error("ERROR writing to server");
  }

  // Update the log
  log_message(fp, "Write to server:", STRING_TYPE, buffer);
}

// Function to read messages from server
char *read_server(FILE *fp, char buffer[], int fd) {
  int n;
  bzero(buffer, MAX_MSG_LEN);

  // Read data from server
  n = read(fd, buffer, MAX_MSG_LEN - 1);
  if (n < 0) {
    error("ERROR reading from socket");
  }
  // Update the log
  log_message(fp, "Read from socket server", STRING_TYPE, buffer);

  return buffer;
}

// Function to check the correct echo
void check_echo(FILE *fp, char buffer[], const char *msg) {
  // Compare message
  if (strcmp(buffer, msg) == 0) {
    // Update the log
    log_message(fp, "Read the correct echo", STRING_TYPE, buffer);
  } else {
    // Update the log
    log_message(fp, "Read the wrong echo", STRING_TYPE, buffer);
    error("ERROR recieved the wrong first message");
  }
}

// Function to generate new obstacles
char *generate_obstacles(char buffer[]) {
  int obstacles[MAX_OBST_ARR_SIZE][2];

  for (int i = 0; i < MAX_OBST_ARR_SIZE; ++i) {
    obstacles[i][0] =
        rand() % (INT_WINDOW_HEIGHT-9) + 5;  // Random x coordinate between 5 and 95
    obstacles[i][1] =
        rand() % (INT_WINDOW_WIDTH-9) + 5;  // Random y coordinate between 5 and 95
  }
  // Form the message
  bzero(buffer, MAX_MSG_LEN);
  sprintf(buffer, "O[%d]", MAX_OBST_ARR_SIZE);
  // Add obstacle coordinates
  for (int i = 0; i < MAX_OBST_ARR_SIZE; ++i) {
    sprintf(buffer + strlen(buffer), "%.3f,%.3f", (float)obstacles[i][0],
            (float)obstacles[i][1]);

    if (i < MAX_OBST_ARR_SIZE - 1) {
      sprintf(buffer + strlen(buffer), "|");
    }
  }
  // Add end of message
  buffer[strlen(buffer)] = '\0';
  // Wait a certain time
  usleep(WAIT_TIME / SPEED);
  // Return the obstacles
  return buffer;
}

// Function to send obstacles
void send_obstacles(FILE *fp, char buffer[], int fd) {
  int n;
  generate_obstacles(buffer);
  // Update the log file
  log_message(fp, "Generated obstacles", STRING_TYPE, buffer);

  // Send data to client
  n = write(fd, buffer, strlen(buffer));
  if (n < 0) {
    error("ERROR writing to server");
  }

  // Update the log file
  log_message(fp, "Write to server:", STRING_TYPE, buffer);
}

int main(int argc, char *argv[]) {
  // Variables
  FILE *fp;
  pid_t obstacles_pid = getpid();  // Recieve the process PID
  srand(obstacles_pid);            // Seed the time with the process PID
  int sockfd, portno, n;
  int echo = 1;
  time_t lastUpdateTime = time(NULL);
  int rand_obs = 0;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  char buffer[MAX_MSG_LEN];
  char buffer2[MAX_MSG_LEN];
  // For the use of select
  fd_set rdfds;
  int retval;
  struct timeval tv;
  tv.tv_sec = 0;  // Set the timeout to zero
  tv.tv_usec = 0;

  // Open the logfile to either create or delete the content
  fp = fopen("../Log/logfile_obstacles_client.txt", "w");
  if (fp == NULL) {  // Check for errors
    perror("Error opening logfile");
    return -1;
  }
  // Close the file
  fclose(fp);

  // Check the correct arguments
  if (argc < 3) {
    error("ERROR, no port provided\n");
  } else {
    // Port number
    portno = atoi(argv[2]);
    // Update the log
    log_message(fp, "Given port number", INT_TYPE, &portno);

    // Host name
    server = gethostbyname(argv[1]);
    if (server == NULL) {
      error("ERROR, no such host\n");
    }
    // Update the log
    log_message(fp, "Given hostname", STRING_TYPE, argv[1]);
  }

  /* SOCKETS */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error("ERROR opening socket");
  }

  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
  serv_addr.sin_port = htons(portno);

  // Connect to the socket server
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    error("ERROR connecting to the socket server");
  }

  // Write the first message "OI"
  write_server(fp, buffer, ACK_MSG_OBST, sockfd);

  // Wait for echo
  read_server(fp, buffer, sockfd);
  check_echo(fp, buffer, ACK_MSG_OBST);

  // Recieve the window size from socket
  read_server(fp, buffer, sockfd);
  // Send echo
  buffer[strlen(buffer)] = '\0';
  n = write(sockfd, buffer, strlen(buffer));
  if (n < 0) {
    error("ERROR writing to socket");
  }
  // Update the log
  log_message(fp, "Writing echo for socket server", STRING_TYPE, buffer);

  // Read the window size
  sscanf(buffer, "%lf,%lf", &WINDOW_HEIGHT, &WINDOW_WIDTH);
  // Round the window size
  INT_WINDOW_HEIGHT = (int)WINDOW_HEIGHT;
  INT_WINDOW_WIDTH = (int)WINDOW_WIDTH;
  printf("WINDOW_HEIGHT: %.lf\n", WINDOW_HEIGHT);
  printf("WINDOW_WIDTH: %.lf\n", WINDOW_WIDTH);
  printf("INT_WINDOW_HEIGHT: %d\n", INT_WINDOW_HEIGHT);
  printf("INT_WINDOW_WIDTH: %d\n", INT_WINDOW_WIDTH);

  // Generate random obstacles and send to the server
  send_obstacles(fp, buffer, sockfd);

  // Wait for echo
  read_server(fp, buffer2, sockfd);
  check_echo(fp, buffer2, buffer);

  // Set the max fd
  int max_fd = sockfd;

  while (1) {
    // Check if the socket server has data
    FD_ZERO(&rdfds);         // Reset the reading set
    FD_SET(sockfd, &rdfds);  // Add the sockfd to the set
    retval = select(max_fd + 1, &rdfds, NULL, NULL, &tv);
    // Check for errors
    if (retval == -1) {
      error("Obstacles client: select failed");
    }
    // Data is available
    else if (retval) {
      if (FD_ISSET(sockfd, &rdfds)) {
        // Read the data from the socket server
        read_server(fp, buffer, sockfd);

        // Check if the exit message has been sent
        if (strcmp(buffer, EXIT_MSG) == 0) {
          buffer[strlen(buffer)] = '\0';
          // Send echo
          n = write(sockfd, buffer, strlen(buffer));
          log_message(fp, "Write to socket server", STRING_TYPE, buffer);
          if (n < 0) {
            error("ERROR writing to socket");
          }
          // Wait a certain time
          usleep(WAIT_TIME);

          // Update the log
          log_message(fp, "Exit key pressed, terminating the process",
                      STRING_TYPE, &buffer);
          break;
        }
      }
    }
    // Check if the certain time interval is passed
    time_t currentTime = time(NULL);
    if (currentTime - lastUpdateTime >= OBST_TIME) {
      // Reset lastUpdateTime
      lastUpdateTime = currentTime;

      // Generate random obstacles and send to the server
      send_obstacles(fp, buffer, sockfd);

      // Wait for echo
      read_server(fp, buffer2, sockfd);
      check_echo(fp, buffer2, buffer);
    }

    // Wait for a certain time
    usleep(WAIT_TIME);
  }

  // Clean up
  // Close the socket connection
  close(sockfd);
  // End the obstacles client process
  return 0;
}