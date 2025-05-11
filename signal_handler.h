// signal_handler.h
#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include "structures.h"

extern volatile sig_atomic_t simulation_should_end;
extern time_t simulation_start_time_global_monotonic_sec;
extern long simulation_start_time_global_monotonic_nsec;
extern shared_data_t *shm_ptr;
extern int shm_id;

void sigint_handler(int signum);

#endif // SIGNAL_HANDLER_H