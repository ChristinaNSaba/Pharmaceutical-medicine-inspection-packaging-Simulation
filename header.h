

#ifndef HEADER_H
#define HEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define SHM_KEY 1234
#define SHM_SIZE sizeof(Shared_Argument)
#define SWITCH_THRESHOLD 10
typedef struct {
    int NUM_OF_PRODUCTION_LINES;
    int MAX_LIQUID_BOTTLES_PER_LINE;
    int MAX_PILL_CONTAINERS_PER_LINE;
    int BOTTLES_PER_PACKAGE;
    int PILL_CONTAINERS_PER_PACKAGE;
    int SIMULATION_DURATION;
    int MIN_LIQUID_LEVEL;
    int MAX_LIQUID_LEVEL;
    int COLOR_MAX_THRESHOLD;
    int COLOR_MIN_THRESHOLD;
    int SEALED_MAX_THRESHOLD;
    int SEALED_MIN_THRESHOLD;
    int LABEL_MAX_THRESHOLD;
    int LABEL_MIN_THRESHOLD;
    int EXPIRY_MAX_THRESHOLD;
    int EXPIRY_MIN_THRESHOLD;
    int MIN_DELAY;
    int MAX_DELAY;
    int MIN_DELAY_FOR_INSPECTORS;
    int MAX_DELAY_FOR_INSPECTORS;
    int PACKAGING_DELAY_MIN;
    int PACKAGING_DELAY_MAX;
} Shared_Argument;

extern Shared_Argument *shared_args;
extern pthread_mutex_t mutex;
extern int simulation_running;

typedef struct {
    int level;
    int has_normal_color;
    int is_sealed;
    int has_correct_label;
    int expiry_date_printed;
    int expiry_date_correct;
    int is_valid;
} Liquid_medicine;

typedef struct {
    int count;
    int has_normal_color;
    int is_sealed;
    int has_correct_label;
    int has_correct_size;
    int expiry_date_printed;
    int expiry_date_correct;
    int is_valid;
} Pill;

typedef struct {
    Liquid_medicine liquid[100];
    Pill pills[100];
    int produced_liquid_medicine;
    int produced_pills;
    int produced_index_pills[200];
    int inspected_index_pills[200];
    int line_index_pills;
    int produced_index[200];
    int inspected_index[200];
    int line_index;
    time_t starting_time;
} SharedData;

#endif // HEADER_H

