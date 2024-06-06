#include "header.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/shm.h>
#include <time.h>

Shared_Argument *shared_args;
pthread_mutex_t mutex;
int simulation_running = 1;

void generate_pills(Pill_Container *pill) {
    pill->count = rand() % 30 + 1; // Generate pill count between 1 and 30
    pill->has_normal_color = (rand() % 2);
    pill->is_sealed = (rand() % 2);
    pill->has_correct_label = (rand() % 2);
    pill->expiry_date_printed = (rand() % 2);
    pill->expiry_date_correct = (rand() % 2);
    printf("Generated pills with count: %d\n", pill->count);
}

int check_validity1(Pill_Container *pill) {
    return pill->is_sealed && pill->has_normal_color && pill->has_correct_label &&
           pill->expiry_date_printed && pill->expiry_date_correct;
}

void* production_function1(void *args) {
    int local_line_index = *(int *)args;
    srand(time(NULL) ^ getpid());
    shared_args->produced_pills[local_line_index] = 0;
    int delay = rand() % (shared_args->MAX_DELAY - shared_args->MIN_DELAY + 1) + shared_args->MIN_DELAY;

    printf("Production function started for line %d\n", local_line_index);

    for (int i = 0; i < shared_args->MAX_PILL_CONTAINERS_PER_LINE && simulation_running; i++) {
        sleep(delay);
        pthread_mutex_lock(&mutex);
        generate_pills(&shared_args->pills[local_line_index][i]);
        shared_args->produced_pills[local_line_index]++;
        shared_args->produced_index[local_line_index][i] = 1;
        pthread_mutex_unlock(&mutex);
        printf("%d pill container prepared by line %d.\n", shared_args->produced_pills[local_line_index], local_line_index);
    }

    return NULL;
}

void* inspectors_function1(void *args) {
    int local_line_index = *(int *)args;
    srand(time(NULL) ^ getpid());
    printf("Inspector function started for line %d\n", local_line_index);

    while (simulation_running) {
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < shared_args->MAX_PILL_CONTAINERS_PER_LINE; i++) {
            if (shared_args->produced_index[local_line_index][i] == 1 && shared_args->inspected_index[local_line_index][i] == 0) {
                pthread_mutex_unlock(&mutex); // Unlock before sleep
                int delay = rand() % (shared_args->MAX_DELAY_FOR_INSPECTORS - shared_args->MIN_DELAY_FOR_INSPECTORS + 1) + shared_args->MIN_DELAY_FOR_INSPECTORS;
                sleep(delay);
                pthread_mutex_lock(&mutex); // Lock after sleep
                int is_valid = check_validity1(&shared_args->pills[local_line_index][i]);
                if (is_valid) {
                    shared_args->inspected_index[local_line_index][i] = 1;
                    shared_args->produced_index[local_line_index][i] = 0;
                    printf("Line %d inspected pill container %d\n", local_line_index, i);
                } else {
                    shared_args->produced_index[local_line_index][i] = 0;
                    printf("Line %d has produced an invalid pill container!\n", local_line_index);
                }
            }
        }
        pthread_mutex_unlock(&mutex);
        sleep(1);  // To prevent busy-waiting
    }
    return NULL;
}

void* packagers_function1(void *args) {
    int local_line_index = *(int *)args;
    srand(time(NULL) ^ getpid());
    shared_args->pills_packaged[local_line_index] = 0;

    printf("Packager function started for line %d\n", local_line_index);

    while (simulation_running) {
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < shared_args->MAX_PILL_CONTAINERS_PER_LINE; i++) {
            if (shared_args->inspected_index[local_line_index][i] == 1) {
                pthread_mutex_unlock(&mutex); // Unlock before sleep
                int delay = rand() % (shared_args->PACKAGING_DELAY_MAX - shared_args->PACKAGING_DELAY_MIN + 1) + shared_args->PACKAGING_DELAY_MIN;
                sleep(delay);
                pthread_mutex_lock(&mutex); // Lock after sleep
                shared_args->pills_packaged[local_line_index]++;
                shared_args->inspected_index[local_line_index][i] = 0;
                printf("Line %d Packaged a pill container\n", local_line_index);
            }
        }
        pthread_mutex_unlock(&mutex);
        sleep(1);  // To prevent busy-waiting
    }
    return NULL;
}

void* manager_function1(void *args) {
    int local_line_index = *(int *)args;

    printf("Manager function started for line %d\n", local_line_index);

    while (simulation_running) {
        sleep(10);

        // Update production speed for the current line
        pthread_mutex_lock(&mutex);
        shared_args->production_speed[local_line_index] = shared_args->produced_pills[local_line_index] - shared_args->pills_packaged[local_line_index];
        pthread_mutex_unlock(&mutex);
        printf("Speed of production line %d is %d\n", local_line_index, shared_args->production_speed[local_line_index]);
    }

    return NULL;
}

void signal_handler1(int signal) {
    if (signal == SIGINT) {
        simulation_running = 0;
    }
}
/*void create_new_thread1(int signal) {

    pthread_t new_packager_worker;
    if (pthread_create(&new_packager_worker, NULL, production_function, &local_line_index) != 0) {
        printf("Error in pthread create (packager)!\n");
    }
}

void delete_thread1(int signal) {
    srand(getpid() ^ time(NULL));

    int selected_thread_index = -1;
    int flag = -1;

    pthread_mutex_lock(&mutex);

    shared_args->NUM_OF_WORKERS[production_line] --;

    pthread_mutex_unlock(&mutex);
}
*/
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <number_of_lines>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_lines = atoi(argv[1]);
    if (num_lines <= 0) {
        fprintf(stderr, "Invalid number of lines: %d\n", num_lines);
        exit(EXIT_FAILURE);
    }

    key_t shm_key = ftok("header.h", 65);
    int shm_id = shmget(shm_key, sizeof(Shared_Argument), 0666 | IPC_CREAT);
    shared_args = (Shared_Argument *)shmat(shm_id, NULL, 0);

    // Initialize shared memory variables
    shared_args->MAX_PILL_CONTAINERS_PER_LINE = 100;
    shared_args->MAX_DELAY = 5;
    shared_args->MIN_DELAY = 1;
    shared_args->MAX_DELAY_FOR_INSPECTORS = 3;
    shared_args->MIN_DELAY_FOR_INSPECTORS = 1;
    shared_args->PACKAGING_DELAY_MAX = 5;
    shared_args->PACKAGING_DELAY_MIN = 1;
    shared_args->SIMULATION_DURATION = 60; // For example, run for 60 seconds

    pthread_mutex_init(&mutex, NULL);

    pthread_t production_threads[num_lines];
    pthread_t inspector_threads[num_lines];
    pthread_t packager_threads[num_lines];
    pthread_t manager_threads[num_lines];
    int line_indices[num_lines];

    signal(SIGINT, signal_handler1);

    time_t current_time;
    time(&current_time);

    for (int i = 0; i < num_lines; i++) {
        line_indices[i] = i;
        shared_args->starting_time[i] = current_time;
        pthread_create(&production_threads[i], NULL, production_function1, &line_indices[i]);
        pthread_create(&inspector_threads[i], NULL, inspectors_function1, &line_indices[i]);
        pthread_create(&packager_threads[i], NULL, packagers_function1, &line_indices[i]);
        pthread_create(&manager_threads[i], NULL, manager_function1, &line_indices[i]);
    }

    for (int i = 0; i < num_lines; i++) {
        pthread_join(production_threads[i], NULL);
        pthread_join(inspector_threads[i], NULL);
        pthread_join(packager_threads[i], NULL);
        pthread_join(manager_threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex);
    shmdt(shared_args);
    shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}

