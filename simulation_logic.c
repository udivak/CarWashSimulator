#include "simulation_logic.h"
#include "utils.h"
#include "logging.h"
#include "ipc_utils.h"
#include "signal_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <sys/sem.h>
#include <fcntl.h>


extern shared_data_t *shm_ptr;
extern int sem_id;
extern volatile sig_atomic_t simulation_should_end;


void read_logs_from_pipe() {
    char read_buf[512];
    ssize_t num_read;

    // Set pipe read end to non-blocking (if desired)
    fcntl(log_pipe_fd[0], F_SETFL, O_NONBLOCK);

    while ((num_read = read(log_pipe_fd[0], read_buf, sizeof(read_buf)-1)) > 0) {
        read_buf[num_read] = '\0';
        printf("%s", read_buf);
        fflush(stdout);
    }
}


#include "simulation_logic.h"
#include "utils.h"
#include "logging.h"
#include "ipc_utils.h"
#include "signal_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <sys/sem.h>

void main_process_logic(int num_stations, float avg_arrive_time_lambda, float avg_wash_time,
                       int run_time_seconds) {
    // Initial log and parameters
    print_log(0, 0, "Main process logic starting.");
    printf("Will generate cars for up to %d seconds.\n", run_time_seconds);
    fflush(stdout);

    // Allocate array to track child PIDs
    pid_t *child_pids = malloc(MAX_QUEUE_SIZE * 2 * sizeof(pid_t));
    if (!child_pids) {
        perror("Failed to allocate memory for child_pids");
        simulation_should_end = 1;
        if (shm_ptr && shm_id != -1)
            shm_ptr->simulation_active = 0;
        return;
    }
    int children_forked_count = 0;

    // Car generation loop
    while (!simulation_should_end) {
        // Check current simulation time
        double current_sim_time = get_current_simulation_time_sec(
            shm_ptr->main_simulation_start_time_sec,
            shm_ptr->main_simulation_start_time_nsec);

        // Stop if runtime expired
        if (current_sim_time >= run_time_seconds) {
            print_log(0, 0, "Car generation loop finished. Reason: Run time expired.");
            break;
        }

        // Read any pending logs from children
        read_logs_from_pipe();
        usleep(100000);

        // Compute next arrival
        float time_to_next_arrival = nextTime(avg_arrive_time_lambda);
        double arrival_time = current_sim_time + time_to_next_arrival;
        if (arrival_time >= run_time_seconds) {
            read_logs_from_pipe();
            print_log(0, 0, "Car generation loop finishing (next arrival would exceed run time).");
            break;
        }

        // Sleep until next arrival
        struct timespec sleep_duration = {
            (time_t)time_to_next_arrival,
            (long)((time_to_next_arrival - (time_t)time_to_next_arrival) * 1e9)
        };
        if (nanosleep(&sleep_duration, NULL) == -1 && errno == EINTR) {
            print_log(0, 0, "Main sleep interrupted.");
            if (simulation_should_end)
                break;
        }

        // === POST-SLEEP GUARD: prevent forking past runtime ===
        current_sim_time = get_current_simulation_time_sec(
            shm_ptr->main_simulation_start_time_sec,
            shm_ptr->main_simulation_start_time_nsec);
        if (current_sim_time >= run_time_seconds) {
            read_logs_from_pipe();                          // drain pipe before finishing simulation
            print_log(0, 0,
                      "Car generation loop finished. Run time reached during sleep.");
            break;
        }
        // ======================================================

        // Handle user-triggered shutdown
        if (simulation_should_end)
            break;

        // Fork new car process
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork error");
            simulation_should_end = 1;
            if (shm_ptr && shm_id != -1)
                shm_ptr->simulation_active = 0;
            break;
        } else if (pid == 0) {
            // Child: seed RNG, run car process logic, then exit
            srand(time(NULL) ^ getpid());
            car_process_logic(avg_wash_time);
            cleanup_ipc(0);
            _exit(EXIT_SUCCESS);
        } else {
            // Parent: track child PID
            if (children_forked_count < MAX_QUEUE_SIZE * 2)
                child_pids[children_forked_count++] = pid;
            else
                fprintf(stderr, "Warning: Reached max forked children tracking array size.\n");
        }
    }

    // Signal end of generation
    if (shm_ptr && shm_id != -1)
        shm_ptr->simulation_active = 0;

    // Final-countdown logs
    if (shm_ptr && shm_id != -1 && shm_ptr->sigint_triggered) {
        print_log(0, 0, "Shutdown due to SIGINT: Waiting for cars currently IN STATIONS to finish...");
        print_log(0, 0, "Cars in queue will NOT be processed further by new station entries.");
    } else {
        print_log(0, 0, "Simulation run time expired: Waiting for ALL cars (in queue and washing) to finish...");
    }
    print_log(0, 0, "Main: Starting final wait for car processes to complete...");

    // Final wait loop for child processes
    int M_terminated_children_count = 0;
    int M_loop_counter = 0;
    int M_max_wait_loops = (run_time_seconds * 5) + 250;
    if (children_forked_count == 0)
        M_max_wait_loops = 50;

    while (M_loop_counter < M_max_wait_loops) {
        if (shm_ptr == (void *)-1 || shm_id == -1) {
            fprintf(stderr, "Main: Shared memory became unavailable during final wait. Assuming completion.\n");
            fflush(stderr);
            break;
        }

        // Reap any terminated children
        pid_t terminated_pid;
        int status;
        while ((terminated_pid = waitpid(-1, &status, WNOHANG)) > 0)
            M_terminated_children_count++;

        // If no more children exist, exit loop
        if (terminated_pid == -1 && errno == ECHILD) {
            if (M_terminated_children_count >= children_forked_count ||
                children_forked_count == 0) {
                char echild_buf[150];
                snprintf(echild_buf, sizeof(echild_buf),
                         "Main: All child processes have terminated (ECHILD, forked: %d, washed: %d).",
                         children_forked_count, M_terminated_children_count);
                print_log(0, 0, echild_buf);
                goto end_wait_loop;
            }
        }

        // Check queue and station status
        long current_cars_in_queue = shm_ptr->cars_currently_in_queue;
        int current_empty_stations_val = semctl(sem_id, EMPTY_STATIONS_SEM, GETVAL);
        if (current_empty_stations_val == -1 && errno != EINVAL && sem_id != -1) {
            perror("Main: semctl GETVAL EMPTY_STATIONS_SEM failed");
            break;
        }

        // Periodic status (can be uncommented if desired)
        if (M_loop_counter % 10 == 0 || M_loop_counter < 5) {
            char wait_status_buf[200];
            snprintf(wait_status_buf, sizeof(wait_status_buf),
                     "Main wait: Forked=%d, Washed=%d. Queue=%ld. EmptyStationsSem=%d (Target %d).",
                     children_forked_count, M_terminated_children_count,
                     current_cars_in_queue, current_empty_stations_val, num_stations);
            // print_log(0, 0, wait_status_buf);
        }

        // If SIGINT shutdown, wait for stations to clear
        if (shm_ptr->sigint_triggered) {
            if (current_empty_stations_val == num_stations &&
                M_terminated_children_count >= children_forked_count) {
                print_log(0, 0,
                          "Main: SIGINT - All stations free and all forked children processed.");
                goto end_wait_loop;
            }
        } else {
            // Otherwise, normal termination: wait for queue empty and all washed
            if (current_cars_in_queue == 0 && current_empty_stations_val == num_stations &&
                M_terminated_children_count >= children_forked_count) {
                print_log(0, 0,
                          "Main: Run time expired - Queue empty, all stations free, and all forked children processed.");
                goto end_wait_loop;
            }
        }

        // If nothing was ever forked and the system is idle
        if (children_forked_count == 0 && current_cars_in_queue == 0 &&
            (current_empty_stations_val == num_stations || current_empty_stations_val == -1)) {
            print_log(0, 0,
                      "Main: No cars forked, queue empty, stations clear. System considered idle.");
            goto end_wait_loop;
        }

        M_loop_counter++;
        usleep(200000);
    }
    if (M_loop_counter >= M_max_wait_loops) {
        print_log(0, 0, "Main: Final wait loop timed out.");
    }

end_wait_loop:;
    // Free PID tracking array and print final stats
    free(child_pids);
    char final_reap_buf[100];
    snprintf(final_reap_buf, sizeof(final_reap_buf),
             "Main: Final wait completed. Washed %d children.", M_terminated_children_count);
    print_log(0, 0, final_reap_buf);

    if (shm_ptr && shm_id != -1) {
        printf("\n--- Simulation Statistics ---\n");
        printf("Total cars washed: %ld\n", shm_ptr->total_cars_washed);
        if (shm_ptr->total_cars_washed > 0) {
            printf("Average waiting time: %.6f seconds\n",
                   shm_ptr->total_waiting_time_seconds / shm_ptr->total_cars_washed);
        } else {
            printf("Average waiting time: N/A (0 cars washed)\n");
        }
        double total_sim_duration = get_current_simulation_time_sec(
            shm_ptr->main_simulation_start_time_sec, shm_ptr->main_simulation_start_time_nsec);
        printf("Total simulation run time: %.6f seconds\n", total_sim_duration);
        fflush(stdout);
    } else {
        printf("\n--- Simulation Statistics (Shared memory or semaphores may have been cleaned up prematurely) ---\n");
        fflush(stdout);
    }
}


void car_process_logic(float avg_wash_time) {
    pid_t car_id = getpid();
    unsigned long my_arrival_number = 0;
    struct timespec car_arrival_ts_monotonic, car_enter_wash_ts_monotonic;
    struct sembuf sop_lock       = {MUTEX_SEM, -1, SEM_UNDO};
    struct sembuf sop_unlock     = {MUTEX_SEM,  1, SEM_UNDO};
    struct sembuf sop_car_queued_signal    = {CARS_IN_QUEUE_SEM,  1, SEM_UNDO};
    struct sembuf sop_wait_for_station     = {EMPTY_STATIONS_SEM, -1, SEM_UNDO};
    struct sembuf sop_station_freed_signal = {EMPTY_STATIONS_SEM,  1, SEM_UNDO};
    struct sembuf sop_car_dequeued_wait     = {CARS_IN_QUEUE_SEM, -1, SEM_UNDO};

    // Validate IPC
    if (shm_ptr == (void *)-1 || shm_id == -1 || sem_id == -1) {
        fprintf(stderr, "Car %d (PID): IPC not available. Exiting.\n", car_id);
        fflush(stderr);
        _exit(EXIT_FAILURE);
    }

    // Record arrival timestamp
    clock_gettime(CLOCK_MONOTONIC, &car_arrival_ts_monotonic);

    // Enqueue car
    if (semop(sem_id, &sop_lock, 1) == -1) {
        perror_msg("semop P(mutex) on arrival", car_id);
        _exit(EXIT_FAILURE);
    }

    my_arrival_number = shm_ptr->car_arrival_counter;
    shm_ptr->car_arrival_counter++;

    if (shm_ptr->cars_currently_in_queue < MAX_QUEUE_SIZE) {
        // Add to queue
        shm_ptr->queue[shm_ptr->queue_tail].car_id         = car_id;
        shm_ptr->queue[shm_ptr->queue_tail].arrival_time_sec  = car_arrival_ts_monotonic.tv_sec;
        shm_ptr->queue[shm_ptr->queue_tail].arrival_time_nsec = car_arrival_ts_monotonic.tv_nsec;
        shm_ptr->queue[shm_ptr->queue_tail].arrival_number    = my_arrival_number;
        shm_ptr->queue_tail = (shm_ptr->queue_tail + 1) % MAX_QUEUE_SIZE;
        shm_ptr->cars_currently_in_queue++;

        print_log(car_id, my_arrival_number, "Arrived");

        // Signal arrival and unlock
        semop(sem_id, &sop_car_queued_signal, 1);
        semop(sem_id, &sop_unlock, 1);
    } else {
        print_log(car_id, my_arrival_number, "Queue full, car leaving");
        semop(sem_id, &sop_unlock, 1);
        _exit(EXIT_SUCCESS);
    }

    // Wait for free washing station
    if (semop(sem_id, &sop_wait_for_station, 1) == -1) {
        if (errno == EINTR && shm_ptr->simulation_active == 0) {
            print_log(car_id, my_arrival_number,
                      "Wait for station interrupted, simulation inactive, car leaving.");
        } else {
            perror_msg("semop P(empty_stations) failed", car_id);
        }
        _exit(EXIT_FAILURE);
    }

    // SIGINT shutdown: release slot and exit if triggered
    if (shm_ptr->sigint_triggered) {
        print_log(car_id, my_arrival_number,
                  "SIGINT shutdown: Acquired station slot, but not proceeding. Releasing slot.");
        semop(sem_id, &sop_station_freed_signal, 1);
        _exit(EXIT_SUCCESS);
    }

    // --- DEQUEUE: remove from queue now that station slot is acquired ---
    semop(sem_id, &sop_lock, 1);
    shm_ptr->queue_head = (shm_ptr->queue_head + 1) % MAX_QUEUE_SIZE;
    shm_ptr->cars_currently_in_queue--;
    semop(sem_id, &sop_unlock, 1);
    // -------------------------------------------------------------------

    // Log entry into station
    clock_gettime(CLOCK_MONOTONIC, &car_enter_wash_ts_monotonic);
    print_log(car_id, my_arrival_number, "Entered washing station");

    // Simulate washing
    struct timespec wash_sleep_ts = {
        (time_t)avg_wash_time,
        (long)((avg_wash_time - (time_t)avg_wash_time) * 1e9)
    };
    nanosleep(&wash_sleep_ts, NULL);

    print_log(car_id, my_arrival_number, "Finished washing, leaving station");

    // Update statistics only (do not dequeue again)
    semop(sem_id, &sop_lock, 1);
    shm_ptr->total_cars_washed++;
    // Waiting time now computed using arrival timestamps stored in local variables if desired
    semop(sem_id, &sop_unlock, 1);

    // Release station
    semop(sem_id, &sop_station_freed_signal, 1);

    _exit(EXIT_SUCCESS);
}



