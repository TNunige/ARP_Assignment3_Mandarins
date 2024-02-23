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
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "Include/constants.h"

/* GLOBAL VARIABLES*/
pid_t sp_pids[NUM_PROCESSES];  // Array to store Process IDs
// Array of struct timeval to store the timestamp of the last update from each
// process
struct timeval prev_ts[NUM_PROCESSES];
// Array to track whether data has been received from each process
int process_data_recieved[NUM_PROCESSES] = {0, 0, 0, 0, 0};
char *process_names[NUM_PROCESSES] = PROCESS_NAMES;  // Names for the logfile

// Logs time update to file
void log_receipt(pid_t process_id, char *process_name, struct timeval tv) {
  FILE *lf_fp = fopen("../Log/logfile_watchdog.txt", "a");
  if (lf_fp == NULL) {  // Check for errors
    perror("Error opening logfile file");
    return;
  }
  fprintf(lf_fp, "Recieved signal from %s [%d]: %ld %ld\n", process_name,
          process_id, tv.tv_sec, tv.tv_usec);
  fclose(lf_fp);  // Close the logfile
}

// Updates the process data received and previous time
void process_update_handler(int sig, siginfo_t *info, void *context) {
  for (int i = 0; i < NUM_PROCESSES; i++) {
    if (info->si_pid == sp_pids[i]) {
      process_data_recieved[i] = 1;  // Set data recieved to 1
      gettimeofday(&prev_ts[i], NULL);
      // Log the signal and process
      log_receipt(sp_pids[i], process_names[i], prev_ts[i]);
    }
  }
}

// Gets elapsed time in seconds between two timevals
double get_elapsed_time_s(struct timeval current, struct timeval previous) {
  return (double)(current.tv_sec - previous.tv_sec) +
         (double)(current.tv_usec - previous.tv_usec) / 1000000;
}

// Terminates all watched processes
void terminate_all_watched_processes() {
  for (int i = 0; i < NUM_PROCESSES; i++) {
    if (kill(sp_pids[i], SIGKILL) < 0) {
      perror("kill");
    }
    printf("Killed process %s\n", process_names[i]);
  }
}

// Main loop
int main() {
  // Clear contents of the logfile
  FILE *lf_fp = fopen("../Log/logfile_watchdog.txt", "w");
  fclose(lf_fp);

  // Get its PID
  pid_t Watchdog_pid = getpid();
  // Publish the watchdog pid
  FILE *watchdog_fp = fopen(PID_FILE_PW, "w");
  if (watchdog_fp == NULL) {  // Check for errors
    perror("Error opening Watchdog tmp file");
    return -1;
  }
  fprintf(watchdog_fp, "%d", Watchdog_pid);
  fclose(watchdog_fp);  // Close file

  lf_fp = fopen("../Log/logfile_watchdog.txt", "a");
  fprintf(lf_fp, "Watchdog pid %d\n", Watchdog_pid);
  fclose(lf_fp);

  // Reading in pids for other processes
  FILE *pid_fp = NULL;
  struct stat sbuf;

  char *fnames[NUM_PROCESSES] = PID_FILE_SP;
  for (int i = 0; i < NUM_PROCESSES; i++) {
    // Check if the file size is bigger than 0
    if (stat(fnames[i], &sbuf) == -1) {
      perror("error-stat");
      return -1;
    }
    // Wait until the file has data
    while (sbuf.st_size <= 0) {
      if (stat(fnames[i], &sbuf) == -1) {
        perror("error-stat");
        return -1;
      }
      usleep(50000);
    }

    pid_fp = fopen(fnames[i], "r");
    // Save the PID to sp_pids
    fscanf(pid_fp, "%d", &sp_pids[i]);
    // Close file
    fclose(pid_fp);

    lf_fp = fopen("../Log/logfile_watchdog.txt", "a");
    fprintf(lf_fp, "Process %d pid %d\n", i, sp_pids[i]);
    fclose(lf_fp);
  }

  // Set up sigaction for receiving signals from processes
  struct sigaction p_action;
  p_action.sa_flags = SA_SIGINFO;
  // Define sigaction as a function process_update_handler
  p_action.sa_sigaction = process_update_handler;
  if (sigaction(SIGUSR1, &p_action, NULL) < 0) {  // Check for errors
    perror("sigaction");
  }

  // Get a start time to track total run time
  struct timeval process_start_time;
  gettimeofday(&process_start_time, NULL);
  struct timeval read_time;
  double elapsed;  // Elapsed time variable

  while (1) {
    // Loop over all monitored processes
    for (int i = 0; i < NUM_PROCESSES; i++) {
      // Check if signal has come from the process
      if (process_data_recieved[i]) {
        gettimeofday(&read_time, NULL);
        // Calculate the elapsed time
        elapsed = get_elapsed_time_s(read_time, prev_ts[i]);

        // Check if there is a timeout
        if (elapsed > PROCESS_TIMEOUT_S) {
          // Terminate all processes
          terminate_all_watched_processes();
          printf("Terminated all\n");

          // Log termination and total elapsed time
          struct timeval termination_time;
          gettimeofday(&termination_time, NULL);
          elapsed = get_elapsed_time_s(termination_time, process_start_time);
          lf_fp = fopen("../Log/logfile_watchdog.txt", "a");
          fprintf(lf_fp, "Terminated after running for %05.2f seconds\n",
                  elapsed);
          fclose(lf_fp);

          // Exit the watchdog process
          return 0;
        }
      }
    }

    // Wait for a certain time
    usleep(WAIT_TIME);
  }

  // Exit the watchdog process
  return 0;
}