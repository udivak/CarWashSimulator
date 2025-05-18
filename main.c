#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include "constants.h"
#include "structures.h"
#include "logging.h"
#include "ipc_utils.h"
#include "signal_handler.h"
#include "simulation_logic.h"

// --- Global Variables (for main process signal handling) ---
time_t simulation_start_time_global_monotonic_sec;
long simulation_start_time_global_monotonic_nsec;

// IPC global variables
extern int shm_id;
extern shared_data_t *shm_ptr;
extern int sem_id;
int log_pipe_fd[2];

// Signal handling global variable
extern volatile sig_atomic_t simulation_should_end;


int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s numOfMachine avg_arrive_time avg_wash_time run_time\n", argv[0]);
        fprintf(stderr, "Example: %s 3 2 5 60\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_stations = atoi(argv[1]);
    float avg_arrive_time = atof(argv[2]);
    float avg_wash_time = atof(argv[3]);
    int run_time_seconds = atoi(argv[4]);

    if (num_stations <= 0 || num_stations > MAX_STATIONS) {
        fprintf(stderr, "Error: Number of stations must be between 1 and %d.\n", MAX_STATIONS);
        return EXIT_FAILURE;
    }
    if (avg_arrive_time <= 0 || avg_wash_time <= 0 || run_time_seconds <= 0) {
        fprintf(stderr, "Error: Time values must be positive.\n");
        return EXIT_FAILURE;
    }

    float avg_arrive_time_lambda = 1.0f / avg_arrive_time;

    srand(time(NULL) ^ getpid());

    struct timespec start_ts_monotonic;
    clock_gettime(CLOCK_MONOTONIC, &start_ts_monotonic);
    simulation_start_time_global_monotonic_sec = start_ts_monotonic.tv_sec;
    simulation_start_time_global_monotonic_nsec = start_ts_monotonic.tv_nsec;

    if (pipe(log_pipe_fd) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // 0 for arrival_num when main process logs something generic before cars exist
    print_log(0, 0, "Car Wash Simulation Starting...");
    printf("Parameters: Stations=%d, AvgArrivalTime=%.2fs (Lambda=%.3f), AvgWashTime=%.2fs, SimulationTime=%ds\n",
           num_stations, avg_arrive_time, avg_arrive_time_lambda, avg_wash_time, run_time_seconds);
    fflush(stdout);

    initialize_ipc(num_stations);
    shm_ptr->main_simulation_start_time_sec = start_ts_monotonic.tv_sec;
    shm_ptr->main_simulation_start_time_nsec = start_ts_monotonic.tv_nsec;
    shm_ptr->simulation_active = 1;
    shm_ptr->sigint_triggered = 0;
    shm_ptr->car_arrival_counter = 1;                               // Start arrival numbers from 1
    shm_ptr->main_pid = getpid();


    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error: Cannot handle SIGINT");
        cleanup_ipc(1);
        return EXIT_FAILURE;
    }

    main_process_logic(num_stations, avg_arrive_time_lambda, avg_wash_time, run_time_seconds);

    cleanup_ipc(1);

    print_log(0, 0, "Simulation ended.");
    return EXIT_SUCCESS;
}