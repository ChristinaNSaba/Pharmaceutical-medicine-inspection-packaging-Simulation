#include "header.h"

Shared_Argument *shared_args;
pthread_mutex_t mutex;
int simulation_running = 1;
int bottles_packaged;
int produced_liquid_medicine;
int production_line;
int line_index;
int produced_index[1000];
int inspected_index[1000];
Liquid_medicine liquid[1000];

void generate_liquid_bottles(Liquid_medicine *liquid) {
    srand(getpid() ^ time(NULL));
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

int find_number_of_bottles_inspected() {
    int counter = 0;
    for (int i = 0; i < shared_args->MAX_LIQUID_BOTTLES_PER_LINE; i++) {
        if (inspected_index[i] == 1) {
            counter++;
        }
    }
    return counter;
}

int find_minimum_speed() {
    int min = shared_args->production_speed[0];
    int min_index = 0;
    for (int i = 1; i < shared_args->NUM_OF_PRODUCTION_LINES; i++) {
        if (shared_args->production_speed[i] < min) {
            min = shared_args->production_speed[i];
            min_index = i;
        }
    }
    return min_index;
}

void* production_function(void *args) {
    int local_line_index = *(int *)args;
    srand(getpid() ^ time(NULL));
    produced_liquid_medicine = 0;
    int delay = rand() % (shared_args->MAX_DELAY - shared_args->MIN_DELAY + 1) + shared_args->MIN_DELAY;

    for (int i = 0; i < shared_args->MAX_LIQUID_BOTTLES_PER_LINE && shared_args->SIMULATION_DURATION * 60 >= difftime(time(NULL), shared_args->starting_time[local_line_index]); i++) {
        sleep(delay);
        pthread_mutex_lock(&mutex);
        generate_liquid_bottles(&liquid[i]);
        produced_liquid_medicine++;
        produced_index[i] = 1;
        pthread_mutex_unlock(&mutex);
        printf("%d liquid medicine generated successfully!\n", i);
    }
    return NULL;
}

void* inspectors_function(void *args) {
    int local_line_index = *(int *)args;
    srand(getpid() ^ time(NULL));
    while (simulation_running && shared_args->SIMULATION_DURATION * 60 >= difftime(time(NULL), shared_args->starting_time[local_line_index])) {
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < shared_args->MAX_LIQUID_BOTTLES_PER_LINE; i++) {
            if (produced_index[i] == 1) {
                int delay = rand() % (shared_args->MAX_DELAY_FOR_INSPECTORS - shared_args->MIN_DELAY_FOR_INSPECTORS + 1) + shared_args->MIN_DELAY_FOR_INSPECTORS;
                pthread_mutex_unlock(&mutex); // Unlock before sleep
                sleep(delay);
                pthread_mutex_lock(&mutex); // Lock after sleep
                printf("Level: %d, Sealed: %d, Color: %d, Date: %d, Label: %d\n", liquid[i].level, liquid[i].is_sealed, liquid[i].has_normal_color, liquid[i].expiry_date_correct, liquid[i].has_correct_label);
                int is_valid = check_validity(&liquid[i]);
                if (is_valid == 1) {
                    inspected_index[i] = 1;
                    produced_index[i] = 0;
                    printf("Line %d inspected liquid item %d\n", local_line_index, i);
                } else {
                    produced_index[i] = 0;
                    printf("Line %d has produced an invalid liquid item!\n", local_line_index);
                }
            }
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void* packagers_function(void *args) {
    int local_line_index = *(int *)args;
    srand(getpid() ^ time(NULL));
    bottles_packaged = 0;
    while (simulation_running && shared_args->SIMULATION_DURATION * 60 >= difftime(time(NULL), shared_args->starting_time[local_line_index])) {
        pthread_mutex_lock(&mutex);
        int bottles_inspected = find_number_of_bottles_inspected();
        if (bottles_inspected >= 1) {
            int delay = rand() % (shared_args->PACKAGING_DELAY_MAX - shared_args->PACKAGING_DELAY_MIN + 1) + shared_args->PACKAGING_DELAY_MIN;
            pthread_mutex_unlock(&mutex); // Unlock before sleep
            sleep(delay);
            pthread_mutex_lock(&mutex); // Lock after sleep
            bottles_packaged += 1;
            printf("Line %d Packaged a bottle and added folded prescription\n", local_line_index);
            shared_args->production_speed[local_line_index] = produced_liquid_medicine - bottles_packaged;
            int flag = 0;
            for (int i = 0; i < shared_args->MAX_LIQUID_BOTTLES_PER_LINE && flag == 0; i++) {
                if (inspected_index[i] == 1) {
                    inspected_index[i] = 0;
                    flag = 1;
                }
            }
        }
        pthread_mutex_unlock(&mutex);
        sleep(1);  // To prevent busy-waiting
    }
    return NULL;
}

void signal_handler(int signal) {
    if (signal == SIGINT) {
        simulation_running = 0;
    }
}

void* manager_function(void *args) {
    while (simulation_running && shared_args->SIMULATION_DURATION * 60 >= difftime(time(NULL), shared_args->starting_time[line_index])) {
        for (int i = 0; i < shared_args->NUM_OF_PRODUCTION_LINES; i++) {
            int min_speed_index = find_minimum_speed();
            if (shared_args->production_speed[i] >= SWITCH_THRESHOLD ) {
                kill(shared_args->production_line_pid[i], SIGUSR1);
                kill(shared_args->production_line_pid[min_speed_index], SIGUSR2);
            }
        }
    }
    return NULL;
}

void create_new_thread(int signal) {

    pthread_t new_packager_worker;
    if (pthread_create(&new_packager_worker, NULL, production_function, &line_index) != 0) {
        printf("Error in pthread create (packager)!\n");
    }
}

void delete_thread(int signal) {
    srand(getpid() ^ time(NULL));

    int selected_thread_index = -1;
    int flag = -1;

    pthread_mutex_lock(&mutex);

    shared_args->NUM_OF_WORKERS[production_line] --;

    pthread_mutex_unlock(&mutex);
}

int main(int argc, char* argv[]) {
    srand(getpid() ^ time(NULL));
    if (argc != 3) {
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
    signal(SIGUSR1, create_new_thread);
    signal(SIGUSR2, delete_thread);

    pthread_mutex_init(&mutex, NULL);

    line_index = atoi(argv[1]);
    production_line = atoi(argv[1]);
    shared_args->starting_time[production_line] = time(NULL);
    // Create multiple inspector threads
    pthread_t inspectors[shared_args->NUM_OF_WORKERS[production_line]];
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_create(&inspectors[i], NULL, inspectors_function, &line_index) != 0) {
            printf("Error in pthread create (inspectors)!\n");
            return 1;
        }
    }

    // Create multiple packager threads
    pthread_t packagers[shared_args->NUM_OF_WORKERS[production_line]];
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_create(&packagers[i], NULL, packagers_function, &line_index) != 0) {
            printf("Error in pthread create (packagers)!\n");
            return 2;
        }
    }

    // Create multiple production threads
    pthread_t production[shared_args->NUM_OF_WORKERS[production_line]];
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_create(&production[i], NULL, production_function, &line_index) != 0) {
            printf("Error in pthread create (production)!\n");
            return 3;
        }
    }

    pthread_t manager;
    if (pthread_create(&manager, NULL, manager_function, NULL) != 0) {
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
