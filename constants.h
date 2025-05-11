// constants.h
#ifndef CONSTANTS_H
#define CONSTANTS_H

#define MAX_STATIONS 10
#define MAX_QUEUE_SIZE 100

#define SHM_KEY ((key_t)0x1234)
#define SEM_KEY ((key_t)0x5678)

#define MUTEX_SEM 0
#define EMPTY_STATIONS_SEM 1
#define CARS_IN_QUEUE_SEM 2

#endif // CONSTANTS_H