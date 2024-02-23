#include <fcntl.h>
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

// Function to log a message to the logfile_keyboard.txt
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
  fp = fopen("../Log/logfile_keyboard.txt", "a");
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

// Function to create a new ncurses window
WINDOW *create_newwin(int height, int width, int starty, int startx) {
  WINDOW *local_win;  // Initialize a new window

  // Create the window
  local_win = newwin(height, width, starty, startx);
  box(local_win, 0, 0);  // Give default characters (lines) as the box
  // Refresh window to show the box
  wrefresh(local_win);

  return local_win;
}

// Function to print message on the window
void print_message(WINDOW *win, const char *message, int *message_index,
                   int width, int height) {
  int max_messages = height - 2;

  // Enable scrolling for the window temporarily
  scrollok(win, TRUE);

  // Clear the oldest message if the buffer is full
  if (*message_index >= max_messages) {
    wmove(win, 1, 1);
    wdeleteln(win);
    (*message_index)--;
  }

  // Print the new message at the top within the specified width
  mvwprintw(win, *message_index + 1, 1, "%-*s", width - 2, message);

  // Update the box
  box(win, 0, 0);

  // Refresh window to show the box and new message
  wrefresh(win);

  // Increment the message_index
  (*message_index)++;
}

int main() {
  // Logfile
  FILE *fp;

  // Open the logfile to either create or delete the content
  fp = fopen("../Log/logfile_keyboard.txt", "w");
  if (fp == NULL) {  // Check for error
    perror("Error opening logfile");
    return -1;
  }
  fclose(fp);  // Close the file

  /* WATCHDOG */
  pid_t Watchdog_pid;             // For watchdog PID
  pid_t keyboard_pid = getpid();  // Recieve the process PID

  // Get the file locations
  char *fnames[NUM_PROCESSES] = PID_FILE_SP;

  // Open the keyboard process tmp file to write its PID
  FILE *pid_fp = fopen(fnames[KEYBOARD_SYM], "w");
  if (pid_fp == NULL) {  // Check for errors
    perror("Error opening keyboard tmp file");
    return -1;
  }
  fprintf(pid_fp, "%d", keyboard_pid);
  fclose(pid_fp);  // Close file

  log_message(fp, "Printed my pid on the file ", INT_TYPE, &keyboard_pid);

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
  if (fscanf(watchdog_fp, "%d", &Watchdog_pid) != 1) {
    printf("Error reading Watchdog PID from file.\n");
    fclose(watchdog_fp);
    return -1;
  }

  // Close the file
  fclose(watchdog_fp);
  log_message(fp, "Recieved the watchdog pid ", INT_TYPE, &Watchdog_pid);

  // Initialize the counter for signal sending
  int counter = 0;

  // Initialize variables for fifo
  int fd;
  // Create a fifo (named pipe) to send the value of the key pressed
  char *myfifo = "/tmp/myfifo";
  mkfifo(myfifo, 0666);
  char str[MAX_LEN];
  char format_string[MAX_LEN] = "%d";
  log_message(fp, "Created the fifo ", INT_TYPE, &myfifo);

  // Buffer for messages on the inspection window
  int message_index = 0;

  // Values for creating the winow
  WINDOW *inspection_win;
  int height, width;
  int startx, starty, titlex;
  int ch;

  // Start ncurses mode
  initscr();
  cbreak();
  noecho();
  nodelay(stdscr, TRUE);  // make getch() non blocking

  // Print messages on the screen
  printw("KEYBOARD\n");
  printw("Press K to restart\n");
  printw("Press Esc to exit\n");
  printw("\n");
  refresh();

  // Enable the keyboard
  keypad(stdscr, TRUE);
  log_message(fp, "Enabled the keyboard ", INT_TYPE, &myfifo);

  // Create the inspection window
  height = LINES * 0.6;
  width = COLS;
  starty = LINES - height;
  startx = 0;
  inspection_win = create_newwin(height, width, starty, startx);
  // Print the title
  titlex = (width - strlen("Inspection")) / 2;
  mvprintw(starty - 1, startx + titlex, "Inspection");
  wrefresh(inspection_win);
  wmove(inspection_win, starty - 1, startx - 1);  // Move the cursor
  refresh();
  log_message(fp, "Created the window ", INT_TYPE, &myfifo);
  // Open fifo for writing
  fd = open(myfifo, O_WRONLY);
  if (fd == -1) {
    perror("Opening fifo failed ");
    return -1;
  }
  log_message(fp, "Opened fifo for writing ", INT_TYPE, &myfifo);

  while (1) {
    log_message(fp, "In the while loop ", INT_TYPE, &Watchdog_pid);
    // Scan the user input
    ch = getch();
    refresh();

    // Check if the user has pressed a key
    if (ch != ERR) {
      log_message(fp, "Key pressed by the user ", INT_TYPE, &ch);

      // Format it to a string
      sprintf(str, format_string, ch);
      // Write the data to fifo
      if (write(fd, str, sizeof(str)) == -1) {
        perror("Keyboard: error writing to fifo");
      }

      usleep(WAIT_TIME);  // Wait

      // Check if esc button has been pressed and exit game
      if (ch == ESCAPE) {
        print_message(inspection_win, "Exiting the program...\n",
                      &message_index, width, height);
        sleep(1);  // Wait
        break;     // Exit loop
      }
      // Other key was pressed
      else {
        // Update the inspection window message based on the user input
        switch (ch) {
          // Move left up 'w'
          case KEY_LEFT_UP:
            print_message(inspection_win, "Moving up-left\n", &message_index,
                          width, height);
            break;
          // Move left 's'
          case KEY_LEFT_s:
            print_message(inspection_win, "Moving left\n", &message_index,
                          width, height);
            break;
          // Move left down 'x'
          case KEY_LEFT_DOWN:
            print_message(inspection_win, "Moving down-left\n", &message_index,
                          width, height);
            break;
          // Move up 'e'
          case KEY_UP_e:
            print_message(inspection_win, "Moving up\n", &message_index, width,
                          height);
            break;
          // Move down 'c'
          case KEY_DOWN_c:
            print_message(inspection_win, "Moving down\n", &message_index,
                          width, height);
            break;
          // Move right down 'v'
          case KEY_RIGHT_DOWN:
            print_message(inspection_win, "Moving down-right\n", &message_index,
                          width, height);
            break;
          // Move right 'f'
          case KEY_RIGHT_f:
            print_message(inspection_win, "Moving right\n", &message_index,
                          width, height);
            break;
          // Move up right 'r'
          case KEY_RIGHT_UP:
            print_message(inspection_win, "Moving right-up\n", &message_index,
                          width, height);
            break;
          case KEY_STOP:
            print_message(inspection_win, "Break\n", &message_index, width,
                          height);
            break;
          case RESTART:
            print_message(inspection_win, "Restart game\n", &message_index,
                          width, height);
            break;
        }
      }
    }
    log_message(fp, "Key not pressed by the user ", INT_TYPE, &ch);
    // Refresh the screen
    refresh();

    // Send a signal after certain iterations have passed
    if (counter == PROCESS_SIGNAL_INTERVAL) {
      log_message(fp, "Sending a signal to watchdog ", INT_TYPE, &Watchdog_pid);
      // send signal to watchdog every process signal interval
      if (kill(Watchdog_pid, SIGUSR1) < 0) {
        perror("kill");
      }
      log_message(fp, "Sent a signal to watchdog ", INT_TYPE, &Watchdog_pid);
      // Set counter to zero (start over)
      counter = 0;
    } else {
      // Increment the counter
      counter++;
    }

    // Wait for a certain time
    usleep(WAIT_TIME);
  }
  // End ncurses mode
  endwin();
  // Close fd
  close(fd);

  // End the keyboard program
  return 0;
}