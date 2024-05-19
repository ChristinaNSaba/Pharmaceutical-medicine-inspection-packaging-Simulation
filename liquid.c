#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <sys/ipc.h>
#include <signal.h>
#include <pthread.h>

pthread_mutex_t mutex;

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("error\n");
    }
    pthread_t inspectors, packagers, production;
    pthread_mutex_init(&mutex, NULL);
    if(pthread_create(&inspectors, NULL, &inspectors_function, NULL) != 0){
        printf("error in pthread create!\n");
        return 1;
    }
    if(pthread_create(&packagers, NULL, &packagers_function, NULL) != 0){
        printf("error in pthread create!\n");
        return 2;
    }
    if(pthread_create(&production, NULL, &production_function, NULL) != 0){
        printf("error in pthread create!\n");
        return 3;
    }
    if(pthread_join(inspectors, NULL) != 0){
        printf("error in pthread join!\n");
        return 4;
    }
    if(pthread_join(packagers, NULL) != 0){
        printf("error in pthread join!\n");
        return 5;
    }
    if(pthread_join(production, NULL) != 0){
        printf("error in pthread join!\n");
        return 6;
    }
    pthread_mutex_destroy(&mutex);
    return 0;
}
