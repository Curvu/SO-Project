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
#define MUTEX_LOG_FILE "/log_mutex"

/* Shared memory */
typedef struct {
	//TODO
} mem_struct;

int shmid;
mem_struct * mem;

sem_t * sem_log; // binary

/* Workers */
pid_t * workers; // array
sem_t * sem_workers;

/* Thread Sensor Reader */
pthread_t sensor_reader;

/* Thread Console Reader */
pthread_t console_reader;
int *write_pos, *read_pos;

/* Thread Dispatcher */
pthread_t dispatcher;
pthread_mutex_t disp_mutex = PTHREAD_MUTEX_INITIALIZER;

void block_write_log(char *content) {
  sem_wait(sem_log);
  write_log(content);
  sem_post(sem_log);
}

void worker(int num) {
  char buffer[MAX];
  sprintf(buffer, "WORKER %d READY", num);
  block_write_log(buffer);

  while(1) {
    // wait for a job
    
    sem_wait(sem_workers);
    // Do your thing
    sem_post(sem_workers);
  }

  exit(0);
}

void alert_watcher() {
  // ve os alertas e o que foi gerado pelo sensor
  // envia mensagem
  exit(0);
}

void * sensor_reader_func(void * param) {
  block_write_log("THREAD SENSOR_READER CREATED");

  // Do your thing

  pthread_exit(NULL);
}

void * console_reader_func(void * param) {
  block_write_log("THREAD CONSOLE_READER CREATED");

  // Do your thing

  pthread_exit(NULL);
}

void * dispatcher_func(void * param) { // LEMBRAR QUE È PRECISO SINCRONIZAR COM OS WORKERS QUANDO NENHUM ESTÀ DISPONIVEL
  block_write_log("THREAD DISPATCHER CREATED");

  // Do your thing

  pthread_exit(NULL);
}

void cleanup() {
  /* Kill workers */
  for (int i = 0; i < (N_WORKERS); i++) kill(workers[i], SIGKILL);

  /* Remove shared memory */
  shmdt(mem); // dettach from the shared memory
  shmctl(shmid, IPC_RMID, NULL); // remove the shared memory

  /* Remove Semaphores */
  sem_unlink(MUTEX_FILE);
  sem_close(sem_workers);
  sem_unlink(MUTEX_LOG_FILE);
  sem_close(sem_log);

  /* Remove Threads */
  pthread_cancel(sensor_reader);
  pthread_cancel(console_reader);
  pthread_cancel(dispatcher);

  block_write_log("HOME_IOT SIMULATOR CLOSING");
  exit(0);
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
  if ((sem_workers = sem_open(MUTEX_FILE, O_CREAT, 0700, N_WORKERS)) == SEM_FAILED) {
    write_log("ERROR CREATING WORKERS SEMAPHORE");
    exit(0);
  }

  if ((sem_log = sem_open(MUTEX_LOG_FILE, O_CREAT, 0700, 1)) == SEM_FAILED) {
    write_log("ERROR CREATING LOG SEMAPHORE");
    exit(0);
  }

  /* Sensor Reader */
  if (pthread_create(&sensor_reader, NULL, sensor_reader_func, NULL) != 0) {
    block_write_log("ERROR CREATING SENSOR_READER");
    exit(0);
  }

  /* Console Reader */
  if (pthread_create(&console_reader, NULL, console_reader_func, NULL) != 0) {
    block_write_log("ERROR CREATING CONSOLE_READER");
    exit(0);
  }

  /* Dispatcher */
  if (pthread_create(&dispatcher, NULL, dispatcher_func, NULL) != 0) {
    block_write_log("ERROR CREATING DISPATCHER");
    exit(0);
  }

  /* Creating Workers */
  workers = malloc(N_WORKERS * sizeof(pid_t));
  if (workers == NULL) {
    block_write_log("ERROR WHILE CREATING WORKERS");
    exit(0);
  }
  for (int i = 0; i < N_WORKERS; i++) {
    if (fork() == 0) worker(i + 1);
  }

  /* Creating Alerts Watcher */
  if (fork() == 0) alert_watcher();

	// Espera pelas threads
	pthread_join(console_reader, NULL);
	pthread_join(sensor_reader, NULL);
	pthread_join(dispatcher, NULL);

  return 0;
}
