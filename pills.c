#include "header.h"

Shared_Argument *shared_args;
pthread_mutex_t mutex;
int simulation_running = 1;
int pills_packaged;
int produced_pills;
int production_line;
int line_index;
int produced_index[1000];
int inspected_index[1000];
Pill pills[1000];

void generate_pills(Pill *pill) {
    srand(getpid() ^ time(NULL));
    pill->count = rand() % 30 + 1; // Generate pill count between 1 and 30
    pill->has_normal_color = (rand() % (shared_args->COLOR_MAX_THRESHOLD + 1) >= shared_args->COLOR_MIN_THRESHOLD) ? 1 : 0;
    pill->is_sealed = (rand() % (shared_args->SEALED_MAX_THRESHOLD + 1) >= shared_args->SEALED_MIN_THRESHOLD) ? 1 : 0;
    pill->has_correct_label = (rand() % (shared_args->LABEL_MAX_THRESHOLD + 1) >= shared_args->LABEL_MIN_THRESHOLD) ? 1 : 0;
    pill->expiry_date_printed = (rand() % (shared_args->EXPIRY_MAX_THRESHOLD + 1) >= shared_args->EXPIRY_MIN_THRESHOLD) ? 1 : 0;
    pill->expiry_date_correct = (rand() % (shared_args->EXPIRY_MAX_THRESHOLD + 1) >= shared_args->EXPIRY_MIN_THRESHOLD) ? 1 : 0;
}

int check_validity(Pill *pill) {
    if (pill->is_sealed && pill->has_normal_color && pill->has_correct_label &&
        pill->expiry_date_printed && pill->expiry_date_correct) {
        return 1;
    }
    return 0;
}

int find_number_of_pills_inspected() {
    int counter = 0;
    for (int i = 0; i < shared_args->MAX_PILL_CONTAINERS_PER_LINE; i++) {
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
    SharedData *shared = (SharedData *)args;
    int delay = rand() % (shared_args->MAX_DELAY - shared_args->MIN_DELAY + 1) + shared_args->MIN_DELAY;
    while (simulation_running && shared_args->SIMULATION_DURATION * 60 >= difftime(time(NULL), shared->starting_time)) {
        sleep(delay);
        pthread_mutex_lock(&mutex);
        generate_pills(&shared->pills[shared->produced_pills % shared_args->MAX_PILL_CONTAINERS_PER_LINE]);
        shared->produced_index[shared->produced_pills % shared_args->MAX_PILL_CONTAINERS_PER_LINE] = 1;
        shared->produced_pills++;
        pthread_mutex_unlock(&mutex);
        printf("%d pill container prepared.\n", shared->produced_pills);
    }
    kill(getpid(), SIGINT);
    return NULL;
}

void* inspectors_function(void *args) {
    SharedData *shared = (SharedData *)args;
    while (simulation_running && shared_args->SIMULATION_DURATION * 60 >= difftime(time(NULL), shared->starting_time)) {
        for (int i = 0; i < shared_args->MAX_PILL_CONTAINERS_PER_LINE; i++) {
            if (shared->produced_index[i] == 1 && inspected_index[i] == 0) {
                pthread_mutex_lock(&mutex);
                int delay = rand() % (shared_args->MAX_DELAY_FOR_INSPECTORS - shared_args->MIN_DELAY_FOR_INSPECTORS + 1) + shared_args->MIN_DELAY_FOR_INSPECTORS;
                pthread_mutex_unlock(&mutex); // Unlock before sleep
                sleep(delay);
                pthread_mutex_lock(&mutex); // Lock after sleep
                int is_valid = check_validity(&shared->pills[i]);
                if (is_valid == 1) {
                    inspected_index[i] = 1;
                    printf("Line %d inspected pill container %d\n", line_index, i);
                } else {
                    printf("Line %d has produced an invalid pill container!\n", line_index);
                }
                pthread_mutex_unlock(&mutex);
            }
        }
    }
    return NULL;
}

void* packagers_function(void *args) {
    SharedData *shared = (SharedData *)args;
    while (simulation_running && shared_args->SIMULATION_DURATION * 60 >= difftime(time(NULL), shared->starting_time)) {
        int bottles_inspected = find_number_of_pills_inspected();
        if (bottles_inspected >= 1) {
            int delay = rand() % (shared_args->PACKAGING_DELAY_MAX - shared_args->PACKAGING_DELAY_MIN + 1) + shared_args->PACKAGING_DELAY_MIN;
            sleep(delay);
            pills_packaged += 1;
            printf("Line %d Packaged a pill container\n", line_index);
            for (int i = 0; i < shared_args->MAX_PILL_CONTAINERS_PER_LINE; i++) {
                if (inspected_index[i] == 1) {
                    inspected_index[i] = 0;
                    break;
                }
            }
        }
    }
    return NULL;
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
void delete_thread(int signal) {
    srand(getpid() ^ time(NULL));

    int selected_thread_index = -1;
    int flag = -1;

    pthread_mutex_lock(&mutex);

    shared_args->NUM_OF_WORKERS[production_line] --;

    pthread_mutex_unlock(&mutex);
}

void signal_handler(int signal) {
    if (signal == SIGINT) {
        simulation_running = 0;
    }
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

    pthread_mutex_init(&mutex, NULL);

    line_index = atoi(argv[1]);
    production_line = atoi(argv[1]);
    SharedData shared_data;
    shared_data.produced_pills = 0;
    shared_data.starting_time = time(NULL);

    // Create multiple inspector threads
    pthread_t inspectors[shared_args->NUM_OF_WORKERS[production_line]];
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_create(&inspectors[i], NULL, inspectors_function, &shared_data) != 0) {
            printf("Error in pthread create (inspectors)!\n");
            return 1;
        }
    }

    // Create multiple packager threads
    pthread_t packagers[shared_args->NUM_OF_WORKERS[production_line]];
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_create(&packagers[i], NULL, packagers_function, &shared_data) != 0) {
            printf("Error in pthread create (packagers)!\n");
            return 2;
        }
    }

    // Create multiple production threads
    pthread_t production[shared_args->NUM_OF_WORKERS[production_line]];
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_create(&production[i], NULL, production_function, &shared_data) != 0) {
            printf("Error in pthread create (production)!\n");
            return 3;
        }
    }

    // Join all production threads
    for (int i = 0; i < shared_args->NUM_OF_WORKERS[production_line]; i++) {
        if (pthread_join(production[i], NULL) != 0) {
            printf("Error in pthread join (production)!\n");
            return 4;
        }
    }

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

