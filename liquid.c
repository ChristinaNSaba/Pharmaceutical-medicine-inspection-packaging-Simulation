#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <sys/ipc.h>
#include <signal.h>
#include <pthread.h>
#define MAX_LIQUID_BOTTLES_PER_LINE 100
#define MIN_LIQUID_LEVEL 50
#define MAX_LIQUID_LEVEL 100
#define COLOR_MAX_THRESHOLD 100
#define COLOR_MIN_THRESHOLD 5
#define SEALED_MAX_THRESHOLD 200
#define SEALED_MIN_THRESHOLD 8
#define LABEL_MAX_THRESHOLD 100
#define LABEL_MIN_THRESHOLD 7
#define EXPIRY_MAX_THRESHOLD 100
#define EXPIRY_MIN_THRESHOLD 5
#define MIN_DELAY 5
#define MAX_DELAY 10

pthread_mutex_t mutex;

typedef struct Liquid_medicine{
    int level;
    int has_normal_color;
    int is_sealed;
    int has_correct_label;
    int expiry_date_printed;
    int expiry_date_correct;
    int is_valid;
}Liquid_medicine;

Liquid_medicine liquid[MAX_LIQUID_BOTTLES_PER_LINE];
int produced_liquid_medicine;
int produced_index[MAX_LIQUID_BOTTLES_PER_LINE] = {0};
int inspected_index[MAX_LIQUID_BOTTLES_PER_LINE] = {0};

void generate_liquid_bottles(Liquid_medicine liquid){

    //generate level
    int min_level = MIN_LIQUID_LEVEL - 3;
    int max_level = MAX_LIQUID_LEVEL + 3;

    int liquid_level = rand() % (max_level - min_level + 1) + min_level;
    liquid.level = liquid_level;

    //generate liquid color
    int invalid_color = rand() %(COLOR_MAX_THRESHOLD + 1);
    if(invalid_color < COLOR_MIN_THRESHOLD){
        liquid.has_normal_color = 0; // 0 stands for false and 1 stands for 0
    }
    else{
        liquid.has_normal_color = 1; // true
    }

    //sealed the product
    int invalid_sealed = rand() %(SEALED_MAX_THRESHOLD + 1);
    if(invalid_sealed < SEALED_MIN_THRESHOLD){
        liquid.is_sealed = 0; //false
    }
    else{
        liquid.is_sealed = 1; //true
    }

    //generate label
    int invalid_label = rand()%(LABEL_MAX_THRESHOLD + 1);
    if(invalid_label < LABEL_MIN_THRESHOLD){
        liquid.has_correct_label = 0; //false
    }
    else{
        liquid.has_correct_label = 1; //true
    }

    //generate expiry date
    int missed_expiry_date = rand() % (EXPIRY_MAX_THRESHOLD + 1);
    if(missed_expiry_date < EXPIRY_MIN_THRESHOLD){
        liquid.expiry_date_printed = 0; //false
    }
    else{
        liquid.expiry_date_printed = 1; //true
    }
    int invalid_expiry_date = rand() % (EXPIRY_MAX_THRESHOLD + 1);
    if(invalid_expiry_date < EXPIRY_MIN_THRESHOLD){
        liquid.expiry_date_correct = 0; //false
    }
    else{
        liquid.expiry_date_correct = 1; //true
    }

    printf("liquid level: %d\n", liquid.expiry_date_correct);
}

int check_validity(Liquid_medicine liquid){
    //0 => false
    //1 => true

    if(liquid.level < MIN_LIQUID_LEVEL && liquid.level > MAX_LIQUID_LEVEL){
        return 0;
    }
    if(liquid.is_sealed == 0){
        return 0;
    }
    if(liquid.has_normal_color == 0){
        return 0;
    }
    if(liquid.expiry_date_correct == 0){
        return 0;
    }
    if(liquid.expiry_date_printed == 0){
        return 0;
    }
    if(liquid.has_correct_label == 0){
        return 0;
    }
    return 1;
}

void* production_function(void *args){
    produced_liquid_medicine = 0;
    //speed of production for each line
    int delay = rand() % (MAX_DELAY - MIN_DELAY + 1) + MIN_DELAY;
    for(int i=0; i<MAX_LIQUID_BOTTLES_PER_LINE; i++){
        sleep(delay);
        generate_liquid_bottles(liquid[i]);
        produced_liquid_medicine += 1;
        produced_index[i] = 1;
        printf("%d liquid medicine generated successfully!\n", i);
        printf("produced counter: %d\n", produced_liquid_medicine);
    }
}

void* packagers_function(void *args){

}

void* inspectors_function(void *args){
    while(produced_liquid_medicine == 0 ){
        sleep(0.5);
    }
    while(produced_liquid_medicine > 0){
        for(int i=0; i<MAX_LIQUID_BOTTLES_PER_LINE; i++){
            if(produced_index[i] == 1){
                pthread_mutex_lock(&mutex);
                int is_valid = check_validity(liquid[i]);
                if(is_valid){
                    inspected_index[i] = 1;
                    produced_index[i] = 0;
                    produced_liquid_medicine --;
                    printf("inspector function: index: %d\n", i);
                }
                else{
                    produced_index[i] = 0;
                }
                pthread_mutex_unlock(&mutex);
            }
        }
    }
}

int main(int argc, char* argv[]){
    srand(time(NULL) ^ getpid());
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
