#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

char *wrapper_scanf(char *fmt, char *buf){
  //scanf(fmt, buf);
  return buf;
}

char *wrapper_gets(char *buf){
  //gets(buf);
  return buf;
}

// No wrapper for fgetc because the input variable is explicit
// int wrapper_fgetc(FILE *stream){
//   return fgetc(stream);
// }