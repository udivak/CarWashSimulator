#include "utils.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

float nextTime(float rateParameter) {
    if (rateParameter <= 0) {
        fprintf(stderr, "Error: rateParameter must be positive for nextTime().\n");
        return 1.0f;
    }
    return -logf(1.0f - (float)rand() / (RAND_MAX + 1.0f)) / rateParameter;
}