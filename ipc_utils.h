#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include "structures.h"

extern int shm_id;
extern shared_data_t *shm_ptr;
extern int sem_id;

void initialize_ipc(int num_stations);
void cleanup_ipc(int is_main_process);

#endif