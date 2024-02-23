#include <fcntl.h>
#include <math.h>
#include <ncurses.h>
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

// Function to create a new ncurses window
WINDOW *create_newwin(int height, int width, int starty, int startx) {
  // Initialize a new window
  WINDOW *local_win;

  // Create the window
  local_win = newwin(height, width, starty, startx);
  // Draw a box around the window
  box(local_win, 0, 0);  // Give default characters (lines) as the box
  // Refresh window to show the box
  wrefresh(local_win);

  return local_win;
}

// Function to print the character at a desired location
void print_character(WINDOW *win, int y, int x, char *character, int pair) {
  // Print the character at the desired location in blue
  wattron(win, COLOR_PAIR(pair));
  mvwprintw(win, y, x, "%c", *character);
  wattroff(win, COLOR_PAIR(pair));

  // Update box of the window
  box(win, 0, 0);
  // Refresh the window
  wrefresh(win);
}

// Function to log a message to the logfile_window.txt
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
  fp = fopen("../Log/logfile_window.txt", "a");
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

// // Function to update the positions of obstacles or targets
// void update_object(WINDOW *win, struct shared_data *objects, int num, int
// type,
//                    double co_y, double co_x, double *old_y, double *old_x,
//                    int color_pair, double scale_y, double scale_x) {
//   int main_y, main_x;
//   int new_main_y, new_main_x;

//   // Coordinates from real to main window
//   main_y = (int)(objects[num].real_y / scale_y);
//   main_x = (int)(objects[num].real_x / scale_x);
//   // Delete the object at the current position
//   print_character(win, main_y, main_x, " ", color_pair);

//   // Print the new object and save it in the array
//   objects[num].real_y = co_y;
//   objects[num].real_x = co_x;
//   // Coordinates from real to main window
//   new_main_y = (int)(co_y / scale_y);
//   new_main_x = (int)(co_x / scale_x);

//   // Print the obstacle character
//   if (type == 1) {
//     print_character(win, new_main_y, new_main_x, "O", color_pair);
//   }
//   // Print the target character
//   else {
//     print_character(win, new_main_y, new_main_x, "Y", color_pair);
//   }

//   // Update old coordinates
//   *old_y = co_y;
//   *old_x = co_x;
// }

// Function to print out the initial objects positions
void update_objects(WINDOW *win, FILE *fp, double (*objects)[2],
                    int num_objects, double scale_y, double scale_x,
                    int color_pair) {
  // Print the objects
  for (int i = 0; i < num_objects; i++) {
    // Update the log
    log_message(fp, "Object number ", INT_TYPE, &i);
    log_message(fp, "Received object coordinate y (real) ", DOUBLE_TYPE,
                &objects[i][0]);
    log_message(fp, "Received object coordinate x (real) ", DOUBLE_TYPE,
                &objects[i][1]);

    // Convert to window size
    int main_y = (int)(objects[i][0] / scale_y);
    int main_x = (int)(objects[i][1] / scale_x);

    // Print the obstacles on the window
    if (color_pair == 2) {
      print_character(win, main_y, main_x, "O", color_pair);
    }
    // Print the targets on the window
    else {
      print_character(win, main_y, main_x, "Y", color_pair);
    }

    // Update the log
    log_message(fp, "Printed object coordinate y (main) ", INT_TYPE, &main_y);
    log_message(fp, "Printed object coordinate x (main) ", INT_TYPE, &main_x);
  }
}

// Function to print out the initial objects positions
void delete_objects(WINDOW *win, FILE *fp, double (*objects)[2],
                    int num_objects, double scale_y, double scale_x,
                    int color_pair) {
  // Delete the objects
  for (int i = 0; i < num_objects; i++) {
    // Update the log
    log_message(fp, "Object number ", INT_TYPE, &i);
    log_message(fp, "Old object coordinate y (real) ", DOUBLE_TYPE,
                &objects[i][0]);
    log_message(fp, "Old object coordinate x (real) ", DOUBLE_TYPE,
                &objects[i][1]);

    // Convert to window size
    int main_y = (int)(objects[i][0] / scale_y);
    int main_x = (int)(objects[i][1] / scale_x);

    // Delete the character
    print_character(win, main_y, main_x, " ", color_pair);

    // Update the log
    log_message(fp, "Deleted object coordinate y (main) ", INT_TYPE, &main_y);
    log_message(fp, "Deleted object coordinate x (main) ", INT_TYPE, &main_x);
  }
}

// Function to check if the drone has reached a target
void reached_target(FILE *fp, WINDOW *win, double real_y, double real_x,
                    double (*targets)[2], int *num_objects, double scale_y,
                    double scale_x, int *score, int starty1, int height1,
                    int startx1) {
  double euclidean_distance = 0.0;
  double main_y = 0.0, main_x = 0.0;
  double rand_tar = 0.0;

  // Check if the drone is close to the target
  for (int i = 0; i < *num_objects; i++) {
    // Calculate the euclidean_distance between drone and target
    euclidean_distance =
        sqrt(pow(real_y - targets[i][0], 2) + pow(real_x - targets[i][1], 2));

    // Check if euclidean distance is smaller than the threshold
    if (euclidean_distance <= FORCE_FIELD) {
      // Delete the target
      main_y = (int)(targets[i][0] / scale_y);
      main_x = (int)(targets[i][1] / scale_x);
      print_character(win, main_y, main_x, " ", 3);

      // Update the log
      log_message(fp, "Deleted a target at coordinate y ", DOUBLE_TYPE,
                  &targets[i][0]);
      log_message(fp, "Deleted a target at coordinate x ", DOUBLE_TYPE,
                  &targets[i][1]);

      (*score)++;
      // Print the current score next to "SCORE"
      mvprintw(starty1 + height1, startx1 + strlen("SCORE:") + 1, "%d", *score);
      refresh();

      // Remove it from the targets list
      for (int j = i; j < *num_objects - 1; j++) {
        targets[j][0] = targets[j + 1][0];
        targets[j][1] = targets[j + 1][1];
      }
      (*num_objects)--;

      // Update the log
      for (int i = 0; i < *num_objects; ++i) {
        log_message(fp, "Target coordinate y", DOUBLE_TYPE, &targets[i][0]);
        log_message(fp, "Target coordinate x", DOUBLE_TYPE, &targets[i][1]);
      }
      log_message(fp, "SCORE ", INT_TYPE, score);
    }
  }
}

// Main function starts
int main(int argc, char *argv[]) {
  /* VARIABLES */
  // Values for creating the winow
  WINDOW *main_win, *inspection_win;
  int height1, width1;
  int startx1, starty1, titlex1;
  int height2, width2, startx2, starty2, titlex2;
  // Initialize the struct
  struct shared_data data = {.real_y = 0.0,
                             .real_x = 0.0,
                             .ch = 0,
                             .num = 0,
                             .type = 0,
                             .co_y = 0.0,
                             .co_x = 0.0};
  // Logfile
  FILE *fp;
  // For coordinates of the drone, obstacles and targets
  int main_x = 0, main_y = 0;
  int ch, num = 0, type;
  double co_y = 0.0, co_x = 0.0;
  double old_obs_y = 0.0, old_obs_x = 0.0;
  double old_tar_y = 0.0, old_tar_x = 0.0;
  // Initialize the counter for signal sending
  int counter = 0;
  // SCORE counter
  int score = 0;

  // Open the logfile to either create or delete the content
  fp = fopen("../Log/logfile_window.txt", "w");
  if (fp == NULL) {  // Check for error
    perror("Error opening logfile");
    return -1;
  }
  // Close the file
  fclose(fp);

  /* WATCHDOG */
  pid_t Watchdog_pid;           // For watchdog PID
  pid_t window_pid = getpid();  // Recieve the process PID

  // Randomness
  srand(window_pid);

  // Get the file locations
  char *fnames[NUM_PROCESSES] = PID_FILE_SP;

  // Open the window process tmp file to write its PID
  FILE *pid_fp = fopen(fnames[WINDOW_SYM], "w");
  if (pid_fp == NULL) {  // Check for errors
    perror("Error opening Window tmp file");
    return -1;
  }
  fprintf(pid_fp, "%d", window_pid);
  fclose(pid_fp);  // Close file
  log_message(fp, "Printed my pid on the file ", INT_TYPE, &window_pid);

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
  if (watchdog_fp == NULL) {
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

  /* PIPES */
  // For use of select
  fd_set rdfds;
  int retval;
  // Set the timeout to 0 milliseconds
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  // Creating file descriptors
  int window_server[2];
  int server_window[2];

  // Scanning and close unnecessary fds
  sscanf(argv[1], "%d %d %d %d", &window_server[0], &window_server[1],
         &server_window[0], &server_window[1]);
  close(window_server[0]);
  close(server_window[1]);

  // Max fd
  int max_fd = server_window[0];

  /* Start ncurses mode */
  initscr();
  cbreak();
  noecho();

  // Enable color and create color pairs
  start_color();
  init_pair(1, COLOR_BLUE, COLOR_BLACK);  // Color pair for blue -> drone
  init_pair(2, COLOR_MAGENTA,
            COLOR_BLACK);  // Color pair for purple -> obstacles
  init_pair(3, COLOR_GREEN,
            COLOR_BLACK);  // Color pair for green -> targets

  // Print out a welcome message on the screen
  printw("Welcome to the Drone simulator\n");
  refresh();

  /* CREATING WINDOWS */
  // Create the main game window
  height1 = LINES * 0.8;
  width1 = COLS;
  starty1 = (LINES - height1) / 2;
  startx1 = 0;
  main_win = create_newwin(height1, width1, starty1, startx1);
  // Print the title of the window
  titlex1 = (width1 - strlen("Drone game")) / 2;
  mvprintw(starty1 - 1, startx1 + titlex1, "Drone game");
  wrefresh(main_win);  // Refresh window

  // Update the log
  log_message(fp, "Height of the main window ", INT_TYPE, &height1);
  log_message(fp, "Lenght of the main window ", INT_TYPE, &width1);

  // Print "SCORE" under the main window
  mvprintw(starty1 + height1, startx1, "SCORE:");
  refresh();

  /* PRINTING THE DRONE */
  // Real life to ncurses window scaling factors
  double scale_y = (double)DRONE_AREA / height1;
  double scale_x = (double)DRONE_AREA / width1;
  // Setting the real coordinates to middle of the geofence
  double real_y = DRONE_AREA / 2;
  double real_x = DRONE_AREA / 2;
  // Convert real coordinates for ncurses window
  main_y = (int)(real_y / scale_y);
  main_x = (int)(real_x / scale_x);
  // Add the character in the middle of the window
  print_character(main_win, main_y, main_x, "X", 1);

  // Update character coordinates to the logfile
  log_message(fp, "Printed character coordinate y (main) ", INT_TYPE, &main_y);
  log_message(fp, "Printed character coordinate x (main) ", INT_TYPE, &main_x);

  // Open the logfile to create a space
  fp = fopen("../Log/logfile_window.txt", "a");
  if (fp == NULL) {  // Check for errors
    perror("Error opening logfile");
    return -1;
  }
  fprintf(fp, "\n");  // Start from the new row
  fclose(fp);         // Close the file

  double targets[MAX_OBJECT_SIZE][2];
  double obstacles[MAX_OBJECT_SIZE][2];
  int reached_targets = 0;

  // Start the loop
  while (1) {
    // Recieve key pressed and new coordinates from the server (pipes)
    FD_ZERO(&rdfds);         // Reset the reading set
    FD_SET(max_fd, &rdfds);  // Add the fd to the set
    retval = select(max_fd + 1, &rdfds, NULL, NULL, &tv);

    // Check for errors
    if (retval == -1) {
      perror("Window: select error");
      break;
    }
    // Data is available
    else if (retval > 0 && FD_ISSET(max_fd, &rdfds)) {
      // Read from server and check for errors
      if (read(server_window[0], &data, sizeof(struct shared_data)) == -1) {
        perror("Window: error reading from server_window");
      }
      // Save the recieved data
      ch = data.ch;      // Get the key pressed by the user
      type = data.type;  // Get the type of the object

      // If the data is from the drone
      if (type == DRONE_SYM) {
        real_y = data.real_y;  // Get the new y coordinate (real life)
        real_x = data.real_x;  // Get the new x coordinate (real life)

        // Update the log
        log_message(fp, "Recieved from drone coordinate y (real) ", DOUBLE_TYPE,
                    &real_y);
        log_message(fp, "Recieved from drone coordinate x (real) ", DOUBLE_TYPE,
                    &real_x);

        // Check if the restart key has been pressed
        if (ch == RESTART) {
          // If the score is not zero
          if (score > 0) {
            // Reset the score to zero
            score = 0;
            // Print the current score next to "SCORE"
            mvprintw(starty1 + height1, startx1 + strlen("SCORE:") + 1, "%d",
                     score);
            refresh();
          }
        }
      }
      /* Update the positions of the obstacles */
      else if (type == OBSTACLES_SYM) {
        // Delete old obstacles
        delete_objects(main_win, fp, obstacles, num, scale_y, scale_x, 2);

        // Get the number of the targets or obstacles
        num = data.num;

        for (int i = 0; i < num; i++) {
          obstacles[i][0] = data.co_y[i];  // Get the new obstacle y coordinate
          obstacles[i][1] = data.co_x[i];  // Get the new obstacle x coordinate
        }
        // Update character coordinates to the logfile
        log_message(fp, "Recieved new obstacles. Amount: ", INT_TYPE, &num);
        // Update the obstacle coordinates
        update_objects(main_win, fp, obstacles, num, scale_y, scale_x, 2);
      }
      /* Update the positions of the targets */
      else if (type == TARGETS_SYM) {
        // Get the number of the targets or obstacles
        num = data.num;

        for (int i = 0; i < num; i++) {
          targets[i][0] = data.co_y[i];  // Get the new target y coordinate
          targets[i][1] = data.co_x[i];  // Get the new target x coordinate
        }
        // Update character coordinates to the logfile
        log_message(fp, "Recieved new targets. Amount: ", INT_TYPE, &num);
        reached_targets = num;
        // Update the target coordinates
        update_objects(main_win, fp, targets, num, scale_y, scale_x, 3);

        // Print the current score next to "SCORE"
        mvprintw(starty1 + height1, startx1 + strlen("SCORE:") + 1, "%d",
                 score);
        refresh();
      }
    }

    // Check if the esc button has been pressed
    if (ch == ESCAPE) {
      // Log the exit of the program
      log_message(fp, "Recieved ESCAPE key ", INT_TYPE, &ch);
      break;  // Leave the loop
    }

    // Delete drone at the current position
    print_character(main_win, main_y, main_x, " ", 1);

    // Convert the real coordinates for the ncurses window using scales
    main_y = (int)(real_y / scale_y);
    main_x = (int)(real_x / scale_x);

    // Add drone to the desired position
    print_character(main_win, main_y, main_x, "X", 1);

    // Update character coordinates to the logfile
    log_message(fp, "Printed character coordinate y (main) ", INT_TYPE,
                &main_y);
    log_message(fp, "Printed character coordinate x (main) ", INT_TYPE,
                &main_x);

    // Check if the drone has reached a target
    reached_target(fp, main_win, real_y, real_x, targets, &reached_targets,
                   scale_y, scale_x, &score, starty1, height1, startx1);

    // Check if all the targets have been reached
    if (reached_targets == 0 && score > reached_targets) {
      log_message(fp, "All the targets have been reached ", INT_TYPE, &score);
      // Send end of the game message to server
      data.ch = GE;
      // Send data to the server
      if (write(window_server[1], &data, sizeof(struct shared_data)) == -1) {
        perror("Error writing to pipe window_server");
      }
      reached_targets = 0;
      score = 0;
    }

    // Send a signal o watchdog after certain iterations have passed
    if (counter == PROCESS_SIGNAL_INTERVAL) {
      // send signal to watchdog every process signal interval
      if (kill(Watchdog_pid, SIGUSR1) < 0) {
        perror("kill");
      }
      // Set counter to zero (start over)
      counter = 0;
    } else {
      // Increment the counter
      counter++;
    }

    // Open the logfile to create a space
    fp = fopen("../Log/logfile_window.txt", "a");
    if (fp == NULL) {  // Check for errors
      perror("Error opening logfile");
      return -1;
    }
    fprintf(fp, "\n");  // Start from the new row
    fclose(fp);         // Close the file

    // Wait for a certain time
    usleep(WAIT_TIME);
  }

  // End ncurses mode
  endwin();

  // Clean up
  close(server_window[0]);

  // End the window program
  return 0;
}
