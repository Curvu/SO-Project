#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include "lib/functions.h"

/* Shared memory */
int shmid;
Mem_struct * mem;
sem_t * shared_memory_sem;

struct sigaction act;

void cleanup() {
  shmdt(mem);
}

void ctrlc_handler(int signo) {
  cleanup();
  exit(0);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("You must do like this example: ./user_console 32\n");
    exit(0);
  }

  int user;
  verifyParam(argv[1], &user, 1);

  #ifdef DEBUG
    printf("Hello, user %d!\n", user);
  #endif

  // TODO: check if user already exists

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

  /* Signal Handler */
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask); // Block all signals during handler
  act.sa_handler = ctrlc_handler;
  sigaction(SIGINT, &act, NULL);

  /* Main */
  char command[MAX], id[STR], key[STR];
  int min, max;
  while(1) {
    scanf(" %s", command);

    if (strcmp(command, "add_alert") == 0) {
      scanf(" %[^ ] %[^ ] %d %d", id, key, &min, &max);
      if (!(verifyID(id) && verifyKey(key))) {
        printf(">> Some invalid Parameter!!\n");
        continue;
      }

      #ifdef DEBUG
        printf("id: %s, key: %s, min: %d, max: %d \n", id, key, min, max);
      #endif

      // TODO: check if still have space for a new alert, and add it (dont understand this because [id] [key])
    } else if (strcmp(command, "remove_alert") == 0) {
      scanf(" %[^ ]", id);

      #ifdef DEBUG
        printf("id: %s \n", id);
      #endif

      // TODO: check if id is valid, remove alert
    } else if (strcmp(command, "list_alerts") == 0) {
      printf("this is the alert list.\n");
      // TODO: list alerts
    } else if (strcmp(command, "sensors") == 0) {
      sem_wait(shared_memory_sem);
      for (int i = 0; i < mem->max_sensors; i++) {
        if (!compareSensor(&NULL_SENSOR, &mem->sensors[i])) {
          printf("Sensor %s, Key %s, Min %d, Max %d, Interval %d\n", mem->sensors[i].id, mem->sensors[i].key, mem->sensors[i].min, mem->sensors[i].max, mem->sensors[i].inter);
        }
      }
      sem_post(shared_memory_sem);
    } else if (strcmp(command, "stats") == 0) {
      printf("list of stats\n");
      // TODO: list stats
    } else if (strcmp(command, "reset") == 0) {
      printf("restting test\n");
      // TODO: reset stats
    } else if (strcmp(command, "exit") == 0) {
      printf("bye bye!\n");
      // TODO: exit
      break;
    } else printf("Invalid command\n");
  }

  // TODO: free everything

  return 0;
}
