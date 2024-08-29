#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//Timing Stuff
#include <time.h>
#include <sys/time.h>
#define TIME_S(tv_s) {\
                gettimeofday(&tv_s,NULL);\
            }
    
#define TIME_F(tv_s,tv_f) {\
                gettimeofday(&tv_f,NULL);\
                long seconds = tv_f.tv_sec - tv_s.tv_sec;\
                long microseconds = tv_f.tv_usec - tv_s.tv_usec;\
                double elapsed = seconds + microseconds*1e-6;\
                printf("Elapsed: %.6f\n", elapsed);\
                }

void hook_exit(){ return; }

void read(int *t, FILE *f);

void is_valid(int *t, FILE *f){
    if(t[0] == 0)
        read(t,f);
    
}

int main(int argc, char **argv){
    struct  timeval t1,t2;
    TIME_S(t1)

    FILE *f = fopen("svr", "rb");
    if (f == NULL)
        exit(0);

    int buff1[10];
    int buff2[100];
    
    int *t;
    
    /*if(rand() % 2)
        t = buff1;
    else
        t = buff2;*/
    is_valid(buff1,f);
    is_valid(buff2,f);

    TIME_F(t1,t2);
    hook_exit();
}

void read(int *t, FILE *f) {
    int c = 100;
    int i = 0;
    while(c > 0x20){
        c = fgetc(f);
        t[i] = c;
        i++;
    }

    fclose(f);
}