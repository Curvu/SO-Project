#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "lib/functions.h"

int count = 0;

struct sigaction act;

void cleanup() {}

void ctrlz_handler(int signo) {
  printf("\n>> Messages sent so far: %d\n", count);
}

void ctrlc_handler(int signo) {
  printf("\n>> Messages sent: %d\n", count);
  cleanup();
  exit(0);
}

int main(int argc, char **argv) { //$ sensor <identifier> <intervalo> <key> <valor min> <valor maximo>
  if (argc != 6) {
    printf("You must do like this example: ./sensor SENS1 3 HOUSETEMP 10 100\n");
    exit(0);
  }

  #ifdef DEBUG
    printf("Starting sensor!\n");
  #endif

  Sensor sensor;
  verifyParam(argv[1], sensor.id, 0);
  verifyParam(argv[2], &sensor.inter, 1);
  verifyParam(argv[3], sensor.key, 0);
  verifyParam(argv[4], &sensor.min, 1);
  verifyParam(argv[5], &sensor.max, 1);

  /* Signals */
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask); // Block all signals during handler
  act.sa_handler = ctrlz_handler;
  sigaction(SIGTSTP, &act, NULL);
  act.sa_handler = ctrlc_handler;
  sigaction(SIGINT, &act, NULL);

  srand(time(NULL));
  int value;

  int init = time(NULL);
  while(1) {
    if (time(NULL) - init >= sensor.inter) { // if time passed is equal or greater than interval
      init = time(NULL);
      value = (rand() % (sensor.max - sensor.min + 1)) + sensor.min;
      
      #ifdef DEBUG
        printf("%s#%s#%d\n", sensor.id, sensor.key, value); // ID_sensor#Key#Value
      #endif

      count++;
    }
  }

  return 0;
}
