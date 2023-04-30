#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include "lib/functions.h"

Sensor sensor;
int value, count = 0;
int fifo;
char buffer[MAX];

struct sigaction act;

void cleanup() {
  /* Send message to server requesting to remove sensor */
  sprintf(buffer, "<%s#%s", sensor.id, sensor.key);
  if (write(fifo, buffer, strlen(buffer)) == -1) {
    perror("Error registering sensor");
    close(fifo);
    exit(EXIT_FAILURE);
  }

  /* Close FIFO */
  close(fifo);
  exit(0);
}

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

  verifyParam(argv[1], sensor.id, 0);
  verifyParam(argv[2], &sensor.inter, 1);
  verifyParam(argv[3], sensor.key, 0);
  verifyParam(argv[4], &sensor.min, 1);
  verifyParam(argv[5], &sensor.max, 1);

  if (sensor.min >= sensor.max) {
    printf("The minimum value must be less than the maximum value!\n");
    exit(EXIT_FAILURE);
  }

  srand(time(NULL));

  /* Signals */
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask); // Block all signals during handler
  act.sa_handler = ctrlz_handler;
  sigaction(SIGTSTP, &act, NULL);
  act.sa_handler = ctrlc_handler;
  sigaction(SIGINT, &act, NULL);

  /* Open FIFO */
  if ((fifo = open(SENSOR_FIFO, O_WRONLY)) == -1) {
    perror("Error opening FIFO");
    exit(EXIT_FAILURE);
  }

  /* Register FIFO */
  sprintf(buffer, ">%s#%s#%d#%d#%d", sensor.id, sensor.key, sensor.min, sensor.max, sensor.inter);
  if (write(fifo, buffer, strlen(buffer)) == -1) {
    perror("Error registering sensor");
    close(fifo);
    exit(EXIT_FAILURE);
  }

  /* Send Data */
  struct timespec req, remaining;
  req.tv_sec = sensor.inter;
  req.tv_nsec = 0;
  while(1) {
    if (nanosleep(&req, &remaining) == -1) req = remaining;
    else {
      value = (rand() % (sensor.max - sensor.min + 1)) + sensor.min;
      sprintf(buffer, "%s#%s#%d", sensor.id, sensor.key, value); // ID_sensor#Key#Value
      #ifdef DEBUG
        printf("%s\n", buffer);
      #endif

      if (write(fifo, buffer, strlen(buffer)) == -1) {
        perror("Error writing to FIFO");
        close(fifo);
        exit(EXIT_FAILURE);
      }

      count++;
      req.tv_sec = sensor.inter; // reset the sleep time
    }
  }

  return 0;
}
