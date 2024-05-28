#include "header.h"

Shared_Argument *shared_args;
pthread_mutex_t mutex;
int simulation_running = 1;

void generate_liquid_bottles(Liquid_medicine *liquid) {
    srand(getpid()^time(NULL));
    int min_level = shared_args->MIN_LIQUID_LEVEL - 3;
    int max_level = shared_args->MAX_LIQUID_LEVEL + 3;

    liquid->level = rand() % (max_level - min_level + 1) + min_level;
    liquid->has_normal_color = (rand() % (shared_args->COLOR_MAX_THRESHOLD + 1) >= shared_args->COLOR_MIN_THRESHOLD) ? 1 : 0;
    liquid->is_sealed = (rand() % (shared_args->SEALED_MAX_THRESHOLD + 1) >= shared_args->SEALED_MIN_THRESHOLD) ? 1 : 0;
    liquid->has_correct_label = (rand() % (shared_args->LABEL_MAX_THRESHOLD + 1) >= shared_args->LABEL_MIN_THRESHOLD) ? 1 : 0;
    liquid->expiry_date_printed = (rand() % (shared_args->EXPIRY_MAX_THRESHOLD + 1) >= shared_args->EXPIRY_MIN_THRESHOLD) ? 1 : 0;
    liquid->expiry_date_correct = (rand() % (shared_args->EXPIRY_MAX_THRESHOLD + 1) >= shared_args->EXPIRY_MIN_THRESHOLD) ? 1 : 0;
}

int check_validity(Liquid_medicine *liquid) {
    if (liquid->level < shared_args->MIN_LIQUID_LEVEL || liquid->level > shared_args->MAX_LIQUID_LEVEL) return 0;
    if (!liquid->is_sealed) return 0;
    if (!liquid->has_normal_color) return 0;
    if (!liquid->expiry_date_correct) return 0;
    if (!liquid->expiry_date_printed) return 0;
    if (!liquid->has_correct_label) return 0;
    return 1;
}

int find_number_of_bottles_inspected(SharedData *shared) {
    int counter = 0;
    for (int i = 0; i < shared_args->MAX_LIQUID_BOTTLES_PER_LINE; i++) {
        if (shared->inspected_index[i] == 1) {
            counter++;
        }
    }
    return counter;
}

void* production_function(void *args) {
    srand(getpid()^time(NULL));
    SharedData *shared = (SharedData *)args;
    shared->produced_liquid_medicine = 0;
    int delay = rand() % (shared_args->MAX_DELAY - shared_args->MIN_DELAY + 1) + shared_args->MIN_DELAY;

    for (int i = 0; i < shared_args->MAX_LIQUID_BOTTLES_PER_LINE && shared_args->SIMULATION_DURATION * 60 >= difftime(time(NULL), shared->starting_time); i++) {
        sleep(delay);
        pthread_mutex_lock(&mutex);
        generate_liquid_bottles(&shared->liquid[i]);
        shared->produced_liquid_medicine++;
        shared->produced_index[i] = 1;
        pthread_mutex_unlock(&mutex);
        printf("%d liquid medicine generated successfully!\n", i);
    }
    kill(getpid(), SIGINT);
    return NULL;
}

void* inspectors_function(void *args) {
    srand(getpid()^time(NULL));
    SharedData *shared = (SharedData *)args;
    while (simulation_running && shared_args->SIMULATION_DURATION * 60 >= difftime(time(NULL), shared->starting_time)) {
        for (int i = 0; i < shared_args->MAX_LIQUID_BOTTLES_PER_LINE; i++) {
            if (shared->produced_index[i] == 1) {
                int delay = rand() % (shared_args->MAX_DELAY_FOR_INSPECTORS - shared_args->MIN_DELAY_FOR_INSPECTORS + 1) + shared_args->MIN_DELAY_FOR_INSPECTORS;
                sleep(delay);
                printf("Level: %d, Sealed: %d, Color: %d, Date: %d, Label: %d\n", shared->liquid[i].level, shared->liquid[i].is_sealed, shared->liquid[i].has_normal_color, shared->liquid[i].expiry_date_correct, shared->liquid[i].has_correct_label);
                pthread_mutex_lock(&mutex);
                int is_valid = check_validity(&shared->liquid[i]);
                if (is_valid == 1) {
                    shared->inspected_index[i] = 1;
                    shared->produced_index[i] = 0;
                    shared->produced_liquid_medicine--;
                    printf("Line %d inspected liquid item %d\n", shared->line_index, i);
                } else {
                    shared->produced_index[i] = 0;
                    printf("Line %d has produced an invalid liquid item!\n", shared->line_index);
                }
                pthread_mutex_unlock(&mutex);
            }
        }
    }
    return NULL;
}

void* packagers_function(void *args) {
    srand(getpid()^time(NULL));
    SharedData *shared = (SharedData *)args;
    int bottles_packaged = 0;
    while (simulation_running && shared_args->SIMULATION_DURATION * 60 >= difftime(time(NULL), shared->starting_time)) {
        pthread_mutex_lock(&mutex);
        int bottles_inspected = find_number_of_bottles_inspected(shared);
        if (bottles_inspected >= 1) {
            int delay = rand() % (shared_args->PACKAGING_DELAY_MAX - shared_args->PACKAGING_DELAY_MIN + 1) + shared_args->PACKAGING_DELAY_MIN;
            sleep(delay);
            bottles_packaged+=1;
            printf("Line %d Packaged a bottles and added folded prescription\n", shared->line_index);
            int flag = 0;
            for (int i = 0; i < shared_args->MAX_LIQUID_BOTTLES_PER_LINE && flag == 0; i++) {
                if (shared->inspected_index[i] == 1) {
                    shared->inspected_index[i] = 0;
                    flag = 1;
                }
            }
        }
        pthread_mutex_unlock(&mutex);
        sleep(1);  // To prevent busy-waiting
    }
    kill(getppid(), SIGUSR1);
    return NULL;
}

void signal_handler(int signal) {
    if (signal == SIGINT) {
        simulation_running = 0;
    }
}

//void* manager_function(void *args) {
//    SharedData *shared_lines = (SharedData *)args;
//    int line_count = shared_args->NUM_OF_PRODUCTION_LINES;
//
//    while (simulation_running) {
//        sleep(5);  // Check every 5 seconds
//
//        // Find the slowest and fastest lines
//        int slowest_line = -1, fastest_line = -1;
//        int min_production = INT_MAX, max_production = 0;
//
//        pthread_mutex_lock(&mutex);
//        for (int i = 0; i < line_count; i++) {
//            if (shared_lines[i].produced_liquid_medicine < min_production) {
//                min_production = shared_lines[i].produced_liquid_medicine;
//                slowest_line = i;
//            }
//            if (shared_lines[i].produced_liquid_medicine > max_production) {
//                max_production = shared_lines[i].produced_liquid_medicine;
//                fastest_line = i;
//            }
//        }
//
//        // Check if the difference exceeds the threshold
//        if (fastest_line != -1 && slowest_line != -1 && (max_production - min_production > shared_args->PRODUCTION_THRESHOLD)) {
//            printf("Adjusting: slowing down line %d and speeding up line %d\n", fastest_line, slowest_line);
//            // Adjust delays (simplified example, real adjustment would need finer control)
//            for (int i = 0; i < shared_args->MAX_LIQUID_BOTTLES_PER_LINE; i++) {
//                shared_lines[fastest_line].produced_index[i] = 0; // Simulating slowing down
//                shared_lines[slowest_line].produced_index[i] = 1; // Simulating speeding up
//            }
//        }
//        pthread_mutex_unlock(&mutex);
//    }
//    return NULL;
//}

int main(int argc, char* argv[]) {
    srand(getpid() ^ time(NULL));
    if (argc != 2) {
        printf("Usage: %s <line_index>\n", argv[0]);
        return 1;
    }

    // Attach to shared memory
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

    signal(SIGINT, signal_handler);

    pthread_mutex_init(&mutex, NULL);

    SharedData shared;
    memset(&shared, 0, sizeof(SharedData)); 
    shared.line_index = atoi(argv[1]);  
    shared.starting_time = time(NULL);

    // Create multiple inspector threads
    pthread_t inspectors[shared_args->NUM_INSPECTORS];
    for (int i = 0; i < shared_args->NUM_INSPECTORS; i++) {
        if (pthread_create(&inspectors[i], NULL, inspectors_function, &shared) != 0) {
            printf("Error in pthread create (inspectors)!\n");
            return 1;
        }
    }

    // Create multiple packager threads
    pthread_t packagers[shared_args->NUM_PACKAGERS];
    for (int i = 0; i < shared_args->NUM_PACKAGERS; i++) {
        if (pthread_create(&packagers[i], NULL, packagers_function, &shared) != 0) {
            printf("Error in pthread create (packagers)!\n");
            return 2;
        }
    }

    // Create a production thread
    pthread_t production;
    if (pthread_create(&production, NULL, production_function, &shared) != 0) {
        printf("Error in pthread create (production)!\n");
        return 3;
    }

    if (pthread_join(production, NULL) != 0) {
        printf("Error in pthread join (production)!\n");
        return 4;
    }

    // Allow inspectors and packagers to finish
    sleep(2);

    for (int i = 0; i < shared_args->NUM_INSPECTORS; i++) {
        if (pthread_join(inspectors[i], NULL) != 0) {
            printf("Error in pthread join (inspectors)!\n");
            return 5;
        }
    }

    for (int i = 0; i < shared_args->NUM_PACKAGERS; i++) {
        if (pthread_join(packagers[i], NULL) != 0) {
            printf("Error in pthread join (packagers)!\n");
            return 6;
        }
    }

    pthread_mutex_destroy(&mutex);
    return 0;
}
