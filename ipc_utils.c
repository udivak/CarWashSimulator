#include "ipc_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>

int shm_id = -1;
shared_data_t *shm_ptr = (void *)-1;
int sem_id = -1;

extern void print_log(pid_t car_id, unsigned long arrival_num, const char *message_format);

void initialize_ipc(int num_stations) {
    shm_id = shmget(SHM_KEY, sizeof(shared_data_t), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget error");
        exit(EXIT_FAILURE);
    }

    shm_ptr = (shared_data_t *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat error");
        shmctl(shm_id, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    memset(shm_ptr, 0, sizeof(shared_data_t));
    shm_ptr->queue_head = 0;
    shm_ptr->queue_tail = 0;
    shm_ptr->cars_currently_in_queue = 0;
    shm_ptr->total_cars_washed = 0;
    shm_ptr->total_waiting_time_seconds = 0.0;
    shm_ptr->car_arrival_counter = 1;
    shm_ptr->simulation_active = 1;
    shm_ptr->sigint_triggered = 0;

    sem_id = semget(SEM_KEY, 3, IPC_CREAT | IPC_EXCL | 0666);
    if (sem_id < 0) {
        if (errno == EEXIST) {
            fprintf(stderr, "Semaphores with key 0x%x (decimal %d) might already exist. Attempting to remove and recreate.\n", SEM_KEY, SEM_KEY);
            int old_sem_id = semget(SEM_KEY, 3, 0);
            if (old_sem_id != -1) {
                 if (semctl(old_sem_id, 0, IPC_RMID) == -1) {
                     perror("Failed to remove existing semaphores. Please use 'ipcrm -S <key_in_dec>' or 'ipcrm -s <semid>' manually.");
                     shmdt(shm_ptr); shmctl(shm_id, IPC_RMID, NULL); exit(EXIT_FAILURE);
                 }
                 printf("Successfully removed pre-existing semaphores.\n");
            } else {
                fprintf(stderr, "Warning: semget indicated EEXIST but could not get existing semaphore ID for removal. Proceeding to create new.\n");
            }
            sem_id = semget(SEM_KEY, 3, IPC_CREAT | IPC_EXCL | 0666);
            if (sem_id < 0) {
                perror("semget (re-create after removal attempt) error");
                shmdt(shm_ptr); shmctl(shm_id, IPC_RMID, NULL); exit(EXIT_FAILURE);
            }
        } else {
            perror("semget (create new) error");
            shmdt(shm_ptr); shmctl(shm_id, IPC_RMID, NULL); exit(EXIT_FAILURE);
        }
    }

    if (semctl(sem_id, MUTEX_SEM, SETVAL, 1) == -1 ||
        semctl(sem_id, EMPTY_STATIONS_SEM, SETVAL, num_stations) == -1 ||
        semctl(sem_id, CARS_IN_QUEUE_SEM, SETVAL, 0) == -1) {
        perror("semctl SETVAL error");
        cleanup_ipc(1); exit(EXIT_FAILURE);
    }
    print_log(0, 0, "IPC Initialized");
    printf("SHM ID=%d, SEM ID=%d\n", shm_id, sem_id);
    fflush(stdout);
}

void cleanup_ipc(int is_main_process) {
    if (shm_ptr != (void *)-1) {
        if (shmdt(shm_ptr) == -1) {
            if (shm_id != -1) perror("shmdt error");
        }
        shm_ptr = (void *)-1;
    }

    if (is_main_process) {
        print_log(0, 0, "Main process cleaning up IPC resources...");
        if (shm_id != -1) {
            if (shmctl(shm_id, IPC_RMID, NULL) == -1 && errno != EINVAL && errno != ENOENT) {
                perror("shmctl IPC_RMID error");
            }
            shm_id = -1;
        }
        if (sem_id != -1) {
            if (semctl(sem_id, 0, IPC_RMID) == -1 && errno != EINVAL && errno != ENOENT) {
                perror("semctl IPC_RMID error");
            }
            sem_id = -1;
        }
        print_log(0, 0, "IPC Cleanup by main process finished.");
    }
}