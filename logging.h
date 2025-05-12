#ifndef LOGGING_H
#define LOGGING_H

#include <sys/types.h>
#include <time.h>

double get_current_simulation_time_sec(time_t start_sec, long start_nsec);
void print_log(pid_t car_id, unsigned long arrival_num, const char *message_format);
void perror_msg(const char* msg, pid_t car_id);

#endif