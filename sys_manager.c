#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>

#include "lib/functions.h"

int QUEUE_SZ, N_WORKERS, MAX_KEYS, MAX_SENSORS, MAX_ALERTS;

#define MUTEX_FILE "/mutex"

/* Shared memory */
typedef struct {
	//TODO
} mem_struct;

int shmid;
mem_struct * mem;

// Workers
pid_t *workers; // array

sem_t * sem_workers;

pthread_t console_reader, sensor_reader, dispatcher;
int *write_pos, *read_pos;
// pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void worker(int num) {
	//TODO Fazer o worker e sincronizar com semáforos
  sem_wait(sem_workers);
  char buffer[MAX];
  sprintf(buffer, "WORKER %d READY", num);
  write_log(buffer);
  sem_post(sem_workers);

  while(1) { // provavelmente é para usar um cena para esperar atividade (não consome recursos)
    // Do your thing
  }

  exit(0);
}

void dispatcher_func() { // LEMBRAR QUE È PRECISO SINCRONIZAR COM OS WORKERS QUANDO NENHUM ESTÀ DISPONIVEL
  return;
}

void cleanup() {
  /* Kill workers */
  for (int i = 0; i < (N_WORKERS); i++) kill(workers[i], SIGKILL);

  /* Remove shared memory */
  shmdt(mem); // dettach from the shared memory
  shmctl(shmid, IPC_RMID, NULL); // remove the shared memory

  /* Remove Semaphores */
  sem_close(sem_workers);
  sem_unlink(MUTEX_FILE);
}

int main(int argc, char **argv) {
	if (argc != 2) {
    printf("INVALID NUMBER OF ARGUMENTS\n");
	  exit(0);
  }

  write_log("HOME_IOT SIMULATOR STARTING");

	/* Handling file */
	FILE *cfg = fopen(argv[1], "r");
	if (cfg == NULL) {
    write_log("ERROR OPENING CONFIG FILE");
    fclose(cfg);
		exit(0);
	}

	/* Read Config */
	fscanf(cfg, "%d\n%d\n%d\n%d\n%d", &QUEUE_SZ, &N_WORKERS, &MAX_KEYS, &MAX_SENSORS, &MAX_ALERTS);
	if (QUEUE_SZ < 1 || N_WORKERS < 1 || MAX_KEYS < 1 || MAX_SENSORS < 1 || MAX_ALERTS < 0) {
    write_log("ERROR IN CONFIG FILE");
    fclose(cfg);
		exit(0);
	}
  fclose(cfg);

	/* Creating Shared Memory */
	if ((shmid = shmget(IPC_PRIVATE, sizeof(mem_struct), IPC_CREAT | 0700)) == -1){
    write_log("ERROR CREATING SHARED MEMORY");
	  exit(0);
	}

	if ((mem = (mem_struct *) shmat(shmid, NULL, 0)) == (mem_struct *) -1) {
    write_log("ERROR ATTACHING SHARED MEMORY");
		exit(0);
	}

  /* Creating Semaphores */
  if ((sem_workers = sem_open(MUTEX_FILE, O_CREAT, 0700, 1)) == SEM_FAILED) {
    write_log("ERROR CREATING SEMAPHORE");
    exit(0);
  }

  /* Creating Workers */
  workers = malloc(N_WORKERS * sizeof(pid_t));
  if (workers == NULL) {
    write_log("ERROR WHILE CREATING WORKERS");
    exit(0);
  }
  for (int i = 0; i < N_WORKERS; i++) {
    if (fork() == 0) {
      worker(i + 1);
    }
  }

  /* Creating Dispatcher */

	/* Creating Alerts Watcher */
	if (fork() == 0) {
		//TODO
	}

	// Criação das threads
	// TODO Ainda não possuimos as funções das threads
	// if (pthread_create(&console_reader, NULL, console_reader, NULL) != 0){
	// 	printf("Error creating console reader thread\n");
	// 	return 1;
	// }
	// if (pthread_create(&sensor_reader, NULL, sensor_reader, NULL) != 0){
	// 	printf("Error creating sensor reader thread\n");
	// 	return 1;
	// }
	//if (pthread_create(&dispatcher, NULL, *dispatcher_func, NULL) != 0) {
	//	printf("Error creating dispatcher thread\n");
	//	return 1;
	//}

	// Espera pelas threads
	// pthread_join(console_reader, NULL);
	// pthread_join(sensor_reader, NULL);
	pthread_join(dispatcher, NULL);
}
