#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include "lib/functions.h"

int count = 0;

/* Shared memory */
int shmid;
Mem_struct * mem;
sem_t * shared_memory_sem;

struct sigaction act;

void cleanup() {
  shmdt(mem);
}

void ctrlz_handler(int signo) {
  printf("\n>> Messages sent so far: %d\n", count);
}

void ctrlc_handler(int signo) {
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

  /* Semaphore */
  if ((shared_memory_sem = sem_open(SEM_FILE, 0)) == SEM_FAILED) {
    printf("ERROR >> SHARED MEMORY SEMAPHORE\n");
    exit(0);
  }

  /* Shared Memory */
	if ((shmid = shmget(SHM_KEY, sizeof(Mem_struct*), 0666)) == -1) {
    printf("ERROR >> SHARED MEMORY NOT FOUND\n");
    exit(0);
	}

  #ifdef DEBUG
    printf("DEBUG >> shmid = %d\n", shmid);
  #endif

	if ((mem = (Mem_struct *) shmat(shmid, NULL, 0)) == (Mem_struct *) -1) {
    printf("ERROR >> ATTACHING SHARED MEMORY\n");
		exit(0);
	}

  #ifdef DEBUG
    printf("Shared Memory (max_sensors): %d\n", mem->max_sensors);
  #endif

  /* Add sensor to shared memory */
  sem_wait(shared_memory_sem);
  for (int i = 0; i < mem->max_sensors; i++) {
    if (compareSensor(&NULL_SENSOR, &mem->sensors[i])) {
      mem->sensors[i] = sensor;
      break;
    } else if (compareSensor(&sensor, &mem->sensors[i])) {
      printf("Sensor %s already exists!\n", sensor.id);
      sem_post(shared_memory_sem);
      cleanup();
      exit(0);
    }
  }
  sem_post(shared_memory_sem);

  #ifdef DEBUG
    printf("DEBUG >> Sensor added to shared memory\n");
  #endif

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
