#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <sys/types.h>
#include "constants.h"

typedef struct {
    pid_t car_id;
    time_t arrival_time_sec;
    long arrival_time_nsec;
    unsigned long arrival_number;
} car_in_queue_t;

typedef struct {
    // Queue
    car_in_queue_t queue[MAX_QUEUE_SIZE];
    int queue_head;
    int queue_tail;
    int cars_currently_in_queue;

    // Statistics
    long total_cars_washed;
    double total_waiting_time_seconds;
    unsigned long car_arrival_counter;

    // Simulation Control
    time_t main_simulation_start_time_sec;
    long main_simulation_start_time_nsec;
    int simulation_active;
    int sigint_triggered;

    // Parent Process PID
    pid_t main_pid;

} shared_data_t;

#endif