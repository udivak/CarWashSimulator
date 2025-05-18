#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "structures.h"

extern time_t simulation_start_time_global_monotonic_sec;
extern long simulation_start_time_global_monotonic_nsec;
extern shared_data_t *shm_ptr;
extern int log_pipe_fd[];

void write_log_to_pipe(const char *log_msg) {
    write(log_pipe_fd[1], log_msg, strlen(log_msg));
}

double get_current_simulation_time_sec(time_t start_sec, long start_nsec) {
    struct timespec current_ts;
    if (clock_gettime(CLOCK_MONOTONIC, &current_ts) == -1) {
        perror("clock_gettime error");
        return 0.0;
    }
    return (double)(current_ts.tv_sec - start_sec) +
           (double)(current_ts.tv_nsec - start_nsec) / 1e9;
}
/*
void print_log(pid_t car_id, unsigned long arrival_num, const char *message_format) {
    double sim_time;
    if (shm_ptr != (void *)-1 && shm_ptr->main_simulation_start_time_sec != 0) {
        sim_time = get_current_simulation_time_sec(
            shm_ptr->main_simulation_start_time_sec,
            shm_ptr->main_simulation_start_time_nsec);
    } else {
        sim_time = get_current_simulation_time_sec(
            simulation_start_time_global_monotonic_sec,
            simulation_start_time_global_monotonic_nsec);
    }

    if (car_id == 0) { // Main process logging (doesn't have an arrival number for itself)
        printf("%s, Time: %.6f\n", message_format, sim_time);
    } else {    // Car process logging
        printf("Car: %d (Arrival Number: %lu), %s, Time: %.6f\n", car_id, arrival_num, message_format, sim_time);
    }
    fflush(stdout);
}*/

void print_log(pid_t car_pid, unsigned long arrival_num, const char *message_format) {
    double sim_time;
    // if shared memory is hooked up, use its start time; otherwise use the global
    if (shm_ptr != (void*)-1 && shm_ptr->main_simulation_start_time_sec != 0) {
        sim_time = get_current_simulation_time_sec(
            shm_ptr->main_simulation_start_time_sec,
            shm_ptr->main_simulation_start_time_nsec);
    } else {
        sim_time = get_current_simulation_time_sec(
            simulation_start_time_global_monotonic_sec,
            simulation_start_time_global_monotonic_nsec);
    }

    char buf[256];
    if (car_pid == 0) {
        snprintf(buf, sizeof(buf), "%s, Time: %.6f\n", message_format, sim_time);
    } else {
        snprintf(buf, sizeof(buf),
                 "Car: %d (Arrival Number: %lu), %s, Time: %.6f\n",
                 car_pid, arrival_num, message_format, sim_time);
    }

    // make sure we set shm_ptr->main_pid = getpid() *after* initialize_ipc in main()
    if (shm_ptr == (void*)-1 || getpid() == shm_ptr->main_pid) {
        printf("%s", buf);
        fflush(stdout);
    } else {
        write_log_to_pipe(buf);
    }
}


void perror_msg(const char* msg, pid_t car_id) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Car %d (PID): %s", car_id, msg);
    perror(buf);
}