#include "header.h"

void signal_handler(int signal) {
    if (signal == SIGUSR1) {
        printf("Simulation is done. Terminating threads...\n");
    }
}

int create_shared_memory() {
    int shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    return shmid;
}

void read_argument_file(Shared_Argument *shared_args, FILE *file) {
    fscanf(file, "MAX_LIQUID_BOTTLES_PER_LINE %d\n", &shared_args->MAX_LIQUID_BOTTLES_PER_LINE);
    fscanf(file, "BOTTLES_PER_PACKAGE %d\n", &shared_args->BOTTLES_PER_PACKAGE);
    fscanf(file, "SIMULATION_DURATION %d\n", &shared_args->SIMULATION_DURATION);
    fscanf(file, "NUM_OF_PRODUCTION_LINES %d\n", &shared_args->NUM_OF_PRODUCTION_LINES);
    fscanf(file, "MIN_LIQUID_LEVEL %d\n", &shared_args->MIN_LIQUID_LEVEL);
    fscanf(file, "MAX_LIQUID_LEVEL %d\n", &shared_args->MAX_LIQUID_LEVEL);
    fscanf(file, "COLOR_MAX_THRESHOLD %d\n", &shared_args->COLOR_MAX_THRESHOLD);
    fscanf(file, "COLOR_MIN_THRESHOLD %d\n", &shared_args->COLOR_MIN_THRESHOLD);
    fscanf(file, "SEALED_MAX_THRESHOLD %d\n", &shared_args->SEALED_MAX_THRESHOLD);
    fscanf(file, "SEALED_MIN_THRESHOLD %d\n", &shared_args->SEALED_MIN_THRESHOLD);
    fscanf(file, "LABEL_MAX_THRESHOLD %d\n", &shared_args->LABEL_MAX_THRESHOLD);
    fscanf(file, "LABEL_MIN_THRESHOLD %d\n", &shared_args->LABEL_MIN_THRESHOLD);
    fscanf(file, "EXPIRY_MAX_THRESHOLD %d\n", &shared_args->EXPIRY_MAX_THRESHOLD);
    fscanf(file, "EXPIRY_MIN_THRESHOLD %d\n", &shared_args->EXPIRY_MIN_THRESHOLD);
    fscanf(file, "MIN_DELAY %d\n", &shared_args->MIN_DELAY);
    fscanf(file, "MAX_DELAY %d\n", &shared_args->MAX_DELAY);
    fscanf(file, "MIN_DELAY_FOR_INSPECTORS %d\n", &shared_args->MIN_DELAY_FOR_INSPECTORS);
    fscanf(file, "MAX_DELAY_FOR_INSPECTORS %d\n", &shared_args->MAX_DELAY_FOR_INSPECTORS);
    fscanf(file, "PACKAGING_DELAY_MIN %d\n", &shared_args->PACKAGING_DELAY_MIN);
    fscanf(file, "PACKAGING_DELAY_MAX %d\n", &shared_args->PACKAGING_DELAY_MAX);
}

int main() {
    // Signal setup
    signal(SIGUSR1, signal_handler);

    // Create shared memory
    int shared_memory_id = create_shared_memory();
    Shared_Argument* shared_argument = (Shared_Argument*) shmat(shared_memory_id, NULL, 0);
    if (shared_argument == (void*) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Read arguments from file
    const char *filename = "arguments.txt";  // Define the filename
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    read_argument_file(shared_argument, file);
    fclose(file);

    char production_line_index_str[10];
    char shared_memory_id_str[10];
    srand(getpid());

    int liquid_size = rand() % shared_argument->NUM_OF_PRODUCTION_LINES + 1;
    int pills_size = shared_argument->NUM_OF_PRODUCTION_LINES - liquid_size;
    printf("liquid size: %d\n", liquid_size);
    printf("pills size: %d\n", pills_size);

    for (int i = 0; i < liquid_size; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            sprintf(production_line_index_str, "%d", i);
            sprintf(shared_memory_id_str, "%d", shared_memory_id);
            execl("./liquid", "liquid", production_line_index_str, shared_memory_id_str, NULL);
            perror("error in exec!");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < pills_size; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            sprintf(production_line_index_str, "%d", i);
            execl("./pills", "pills", production_line_index_str, NULL);
            perror("error in exec!");
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        sleep(1);
    }

    // Detach and clean up shared memory
    shmdt(shared_argument);
    shmctl(shared_memory_id, IPC_RMID, NULL);

    return 0;
}
