#include "signal_handler.h"
#include <signal.h>
#include <stdio.h>
#include <unistd.h>


volatile sig_atomic_t simulation_should_end = 0; // Definition

// Declared in main.c
extern time_t simulation_start_time_global_monotonic_sec;
extern long simulation_start_time_global_monotonic_nsec;

// Declared in ipc_utils.h
extern shared_data_t *shm_ptr;
extern int shm_id;

extern void print_log(pid_t car_id, unsigned long arrival_num, const char *message_format);

void sigint_handler(int signum) {
    char msg[] = "\nSIGINT received by main process. Initiating graceful shutdown...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);

    simulation_should_end = 1;
    if (shm_ptr != (void *)-1 && shm_id != -1) {
        shm_ptr->simulation_active = 0;
        shm_ptr->sigint_triggered = 1;
    }
    (void) signum;      // Prevent unused variable warning
}