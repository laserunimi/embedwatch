#include <stdio.h>
#include <stdlib.h>    
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

//Timing Stuff
#include <time.h>
#include <sys/time.h>

void hook_exit(){ return; }

int main(int argc, char *argv[]){

	char buf[20] = {0};
    int c = 100;
    int i = 0;

    for(int j=0; j < 20; j++){
        c = fgetc(stdin);
        buf[i%15] = c;
        i++;
    }

    if(buf[0] =='a'){
        buf[15] = 'z';
    } else {
        buf[15] = 'x';
    }

    printf("%s\n", buf);

    hook_exit();
  
    return 0;
}
