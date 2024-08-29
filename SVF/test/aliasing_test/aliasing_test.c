#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void read(int *t, FILE *f);

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
    read(buff1,f);
    read(buff2,f);

    fclose(f);
  
    TIME_F(t1,t2)


}

void read(int *t, FILE *f) {
    int c = 100;
    int i = 0;
    while(c > 0x20){
        c = fgetc(f);
        t[i] = c;
        i++;
    }

}