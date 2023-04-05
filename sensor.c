#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "lib/functions.h"

int count = 0;

void ctrlz_handler(int signo) {
  printf("\n>> Messages sent so far: %d\n", count);
}

int main(int argc, char **argv) { //$ sensor <identifier> <intervalo> <key> <valor min> <valor maximo>
  if (argc != 6) {
    printf("You must do like this example: ./sensor SENS1 3 HOUSETEMP 10 100\n");
    exit(0);
  }

  #ifdef DEBUG
    printf("Starting sensor!\n");
  #endif /* DEBUG */

  char id[STR], key[STR];
  int inter, min, max;
  verifyParam(argv[1], id, 0);
  verifyParam(argv[2], &inter, 1);
  verifyParam(argv[3], key, 0);
  verifyParam(argv[4], &min, 1);
  verifyParam(argv[5], &max, 1);

  struct sigaction act;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask); // Block all signals during handler
  act.sa_handler = ctrlz_handler;
  sigaction(SIGTSTP, &act, NULL);

  srand(time(NULL));
  int value;

  int init = time(NULL);
  while(1) {
    if (time(NULL) - init >= inter) { // if time passed is equal or greater than interval
      init = time(NULL);
      value = (rand() % (max - min + 1)) + min;
      
      #ifdef DEBUG
        printf("%s#%s#%d\n", id, key, value); // ID_sensor#Key#Value
      #endif /* DEBUG */

      count++;
    }
  }

  return 0;
}
