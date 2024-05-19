#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <sys/ipc.h>
#include <signal.h>

#define NUM_OF_PRODUCTION_LINES 5

int main(){

    //signal here

    char production_line_index_str[10];
    srand(getpid());
    int liquid_size = rand()%NUM_OF_PRODUCTION_LINES + 1;
    int pills_size = NUM_OF_PRODUCTION_LINES - liquid;
    for(int i=0; i< liquid_size; i++)
    {
        pid_t pid = fork();
        if(pid == 0){
            sprintf(production_line_index_str, "%d", i);
            execl("./liquid", "liquid", production_line_index_str, NULL);
            perror("error in exec!");
        }
    }
    for(int i=0; i<pills_size; i++){
        pid_t pid = fork();
        if(pid == 0){
            sprintf(production_line_index_str, "%d", i);
            execl("./pills", "pills", production_line_index_str, NULL);
            perror("error in exec!");
        }
    }

    while(1){
        sleep(1);
    }
    return 0;
}
