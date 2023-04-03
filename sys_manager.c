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
} Mem_struct;

int shmid;
Mem_struct * mem;

/* Workers */
sem_t * sem_workers;

/* Thread Sensor Reader */
pthread_t sensor_reader;

/* Thread Console Reader */
pthread_t console_reader;
int *write_pos, *read_pos;

/* Thread Dispatcher */
pthread_t dispatcher;
pthread_mutex_t disp_mutex = PTHREAD_MUTEX_INITIALIZER;

void worker(int num) {
  char buffer[MAX];
  sprintf(buffer, "WORKER %d READY", num);
  write_log(buffer);

  while(1) {
    // wait for a job
    
    sem_wait(sem_workers);
    // Do your thing
    sem_post(sem_workers);
    exit(0);
  }
}

void alert_watcher() {
  // ve os alertas e o que foi gerado pelo sensor
  // envia mensagem
  exit(0);
}

void * sensor_reader_func(void * param) {
  write_log("THREAD SENSOR_READER CREATED");

  // Do your thing

  pthread_exit(NULL);
}

void * console_reader_func(void * param) {
  write_log("THREAD CONSOLE_READER CREATED");

  // Do your thing

  pthread_exit(NULL);
}

void * dispatcher_func(void * param) { // LEMBRAR QUE È PRECISO SINCRONIZAR COM OS WORKERS QUANDO NENHUM ESTÀ DISPONIVEL
  write_log("THREAD DISPATCHER CREATED");

  // Do your thing

  pthread_exit(NULL);
}

void cleanup(pid_t * processes) {
  #ifdef DEBUG
    printf("DEBUG >> Killing all processes!\n");
  #endif
  /* Kill all processes */
  for (int i = 0; i < N_WORKERS; i++) kill(processes[i], SIGKILL);

  #ifdef DEBUG
    printf("DEBUG >> Deleting semaphores!\n");
  #endif
  /* Remove Semaphores */
  sem_unlink(MUTEX_FILE);
  sem_close(sem_workers);

  #ifdef DEBUG
    printf("DEBUG >> Deleting threads!\n");
  #endif
  /* Remove Threads */
  pthread_cancel(sensor_reader);
  pthread_cancel(console_reader);
  pthread_cancel(dispatcher);

  #ifdef DEBUG
    printf("DEBUG >> Deleting shared memory!\n");
  #endif
  /* Remove shared memory */
  shmctl(shmid, IPC_RMID, NULL); // remove the shared memory
  shmdt(mem); // dettach from the shared memory

  #ifdef DEBUG
    printf("DEBUG >> leaving!\n");
  #endif
  write_log("HOME_IOT SIMULATOR CLOSING");
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

  #ifdef DEBUG
    printf("DEBUG >> Creating shared memory!\n");
  #endif

	/* Creating Shared Memory */
	if ((shmid = shmget(IPC_PRIVATE, sizeof(Mem_struct*), 0666 | IPC_CREAT)) == -1){
    write_log("ERROR CREATING SHARED MEMORY");
	  exit(0);
	}

  #ifdef DEBUG
    printf("DEBUG >> %d\n", shmid);
  #endif

	if ((mem = (Mem_struct *) shmat(shmid, NULL, 0)) == (Mem_struct *) -1) {
    write_log("ERROR ATTACHING SHARED MEMORY");
		exit(0);
	}

  #ifdef DEBUG
    printf("DEBUG >> Creating semaphores!\n");
  #endif

  /* Creating Semaphores */
  if ((sem_workers = sem_open(MUTEX_FILE, O_CREAT, 0700, N_WORKERS)) == SEM_FAILED) {
    write_log("ERROR CREATING WORKERS SEMAPHORE");
    exit(0);
  }

  #ifdef DEBUG
    printf("DEBUG >> Creating threads!\n");
  #endif

  /* Sensor Reader */
  if (pthread_create(&sensor_reader, NULL, sensor_reader_func, NULL) != 0) {
    write_log("ERROR CREATING SENSOR_READER");
    exit(0);
  }

  /* Console Reader */
  if (pthread_create(&console_reader, NULL, console_reader_func, NULL) != 0) {
    write_log("ERROR CREATING CONSOLE_READER");
    exit(0);
  }

  /* Dispatcher */
  if (pthread_create(&dispatcher, NULL, dispatcher_func, NULL) != 0) {
    write_log("ERROR CREATING DISPATCHER");
    exit(0);
  }

  #ifdef DEBUG
    printf("DEBUG >> Creating processes!\n");
  #endif

  /* Creating Workers */
  pid_t processes[N_WORKERS + 1];
  for (int i = 0; i < N_WORKERS; i++) {
    pid_t pid = fork();
    if (pid == -1) {
      write_log("ERROR CREATING WORKER");
      exit(0);
    } else if (pid == 0) {
      processes[i] = getpid();
      worker(i);
      exit(0);
    } else processes[i] = pid;
  }

  /* Creating Alerts Watcher */
  pid_t pid = fork();
  if (pid == -1) {
    write_log("ERROR CREATING ALERTS WATCHER");
    exit(0);
  } else if (pid == 0) {
    processes[N_WORKERS] = getpid();
    alert_watcher();
    exit(0);
  } else processes[N_WORKERS] = pid;

	// Espera pelas threads
	pthread_join(console_reader, NULL);
	pthread_join(sensor_reader, NULL);
	pthread_join(dispatcher, NULL);

  #ifdef DEBUG
    printf("DEBUG >> Cleaning up!\n");
  #endif
  cleanup(processes);

  return 0;
}
