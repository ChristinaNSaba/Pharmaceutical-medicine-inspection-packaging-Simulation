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
int line_index;
int production_line;

void generate_pills(Pill_Container *pill) {
    pill->count = rand() % 30 + 1; // Generate pill count between 1 and 30

    // Increase probability of generating valid pills
    pill->has_normal_color = (rand() % 10 < 8); // 80% chance to have normal color
    pill->is_sealed = (rand() % 10 < 9); // 90% chance to be sealed
    pill->has_correct_label = (rand() % 10 < 9); // 90% chance to have correct label
    pill->expiry_date_printed = (rand() % 10 < 8); // 80% chance to have expiry date printed
    pill->expiry_date_correct = (rand() % 10 < 8); // 80% chance to have correct expiry date

    printf("Generated pills with count: %d, sealed: %d, normal color: %d, correct label: %d, expiry printed: %d, expiry correct: %d\n",
           pill->count, pill->is_sealed, pill->has_normal_color, pill->has_correct_label,
           pill->expiry_date_printed, pill->expiry_date_correct);
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

int find_minimum_speed1() {
    int min_speed = shared_args->production_speed[0];
    int min_index = 0;

    for (int i = 1; i < shared_args->NUM_OF_PRODUCTION_LINES; i++) {
        if (shared_args->production_speed[i] < min_speed) {
            min_speed = shared_args->production_speed[i];
            min_index = i;
        }
    }
    return min_index;
}

void* manager_function1(void *args) {
    int local_line_index = *(int *)args;
    time_t current_time;

    printf("Manager function started for line %d\n", local_line_index);

    while (simulation_running) {
        time(&current_time);
        if (shared_args->SIMULATION_DURATION * 60 >= difftime(current_time, shared_args->starting_time[local_line_index])) {
            sleep(10);

            // Update production speed for the current line
            pthread_mutex_lock(&mutex);
            shared_args->production_speed[local_line_index] = shared_args->produced_pills[local_line_index] - shared_args->pills_packaged[local_line_index];
            pthread_mutex_unlock(&mutex);
            printf("Speed of production line %d is %d\n", local_line_index, shared_args->production_speed[local_line_index]);

            for (int i = 0; i < shared_args->NUM_OF_PRODUCTION_LINES; i++) {
                int min_speed_index = find_minimum_speed1();
                printf("Minimum speed of line %d is %d\n", min_speed_index, shared_args->production_speed[min_speed_index]);

                if (shared_args->production_speed[i] >= SWITCH_THRESHOLD) {
                    kill(shared_args->production_line_pid[i], SIGUSR1);
                    kill(shared_args->production_line_pid[min_speed_index], SIGUSR2);
                    printf("Worker from line %d switched to line %d\n", min_speed_index, i);
                    return NULL;
                }
            }
        } else {
            break;
        }
    }
    return NULL;
}

void signal_handler1(int signal) {
    if (signal == SIGINT) {
        simulation_running = 0;
    }
}

void create_new_thread1(int signal) {
    pthread_t new_packager_worker;
    if (pthread_create(&new_packager_worker, NULL, production_function1, &line_index) != 0) {
        printf("Error in pthread create (packager)!\n");
    }
}

void delete_thread1(int signal) {
    srand(getpid() ^ time(NULL));

    pthread_mutex_lock(&mutex);
    shared_args->NUM_OF_WORKERS[production_line]--;
    pthread_mutex_unlock(&mutex);
}

int main(int argc, char* argv[]) {
    srand(getpid() ^ time(NULL));
    if (argc != 3) {
        printf("Usage: %s <line_index>\n", argv[0]);
        return 1;
    }

   /* // Attach to shared memory
    int shared_memory_id = shmget(SHM_KEY, SHM_SIZE, 0666);
    if (shared_memory_id == -1) {
        perror("shmget");
        return 1;
    }

    shared_args = (Shared_Argument *)shmat(shared_memory_id, NULL, 0);
    if (shared_args == (void *)-1) {
        perror("shmat");
        return 1;
    }
*/
    signal(SIGINT, signal_handler1);
    signal(SIGUSR1, create_new_thread1);
    signal(SIGUSR2, delete_thread1);

    pthread_mutex_init(&mutex, NULL);

    line_index = atoi(argv[1]);
    production_line = atoi(argv[1]);
    shared_args->starting_time[production_line] = time(NULL);

    // Create multiple inspector threads
    pthread_t inspectors[shared_args->NUM_OF_WORKERS[production_line]];
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_create(&inspectors[i], NULL, inspectors_function1, &line_index) != 0) {
            printf("Error in pthread create (inspectors)!\n");
            return 1;
        }
    }

    // Create multiple packager threads
    pthread_t packagers[shared_args->NUM_OF_WORKERS[production_line]];
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_create(&packagers[i], NULL, packagers_function1, &line_index) != 0) {
            printf("Error in pthread create (packagers)!\n");
            return 2;
        }
    }

    // Create multiple production threads
    pthread_t production[shared_args->NUM_OF_WORKERS[production_line]];
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_create(&production[i], NULL, production_function1, &line_index) != 0) {
            printf("Error in pthread create (production)!\n");
            return 3;
        }
    }

    pthread_t manager;
    if (pthread_create(&manager, NULL, manager_function1, &line_index) != 0) {
        printf("Error in pthread create (manager)!\n");
        return 7;
    }

    // Join all production threads
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_join(production[i], NULL) != 0) {
            printf("Error in pthread join (production)!\n");
            return 4;
        }
    }

    // Join the manager thread
    if (pthread_join(manager, NULL) != 0) {
        printf("Error in pthread join (manager)!\n");
        return 4;
    }

    // Allow inspectors and packagers to finish
    sleep(2);

    // Join all inspector threads
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_join(inspectors[i], NULL) != 0) {
            printf("Error in pthread join (inspectors)!\n");
            return 5;
        }
    }

    // Join all packager threads
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_join(packagers[i], NULL) != 0) {
            printf("Error in pthread join (packagers)!\n");
            return 6;
        }
    }

    pthread_mutex_destroy(&mutex);
    return 0;
}

