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
#include <errno.h>

#include "lib/functions.h"

#define LOG_FILE "log.txt"

int QUEUE_SZ, N_WORKERS, MAX_KEYS, MAX_SENSORS, MAX_ALERTS;
FILE *fp;
int available_workers = 0;

/* Shared memory */
int shmid;
Mem_struct * mem;
sem_t * shared_memory_sem;

/* Signal Actions */
struct sigaction act; // main

/* Workers */
sem_t semaphore, process_log, BLOCK_WORKER;
pid_t * processes; 

/* Thread Sensor Reader, Console Reader, Dispatcher */
pthread_t sensor_reader, console_reader, dispatcher;

/* Mutexs */
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t kill_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t disp_mutex = PTHREAD_MUTEX_INITIALIZER;

int needQuit(pthread_mutex_t * mutex) {
  int ret = pthread_mutex_trylock(mutex);
  if (ret == 0) {
    pthread_mutex_unlock(mutex);
    return 1;
  } else if (ret == EBUSY) return 0;
  return 1;
}

void write_log(FILE *fp, char * content) {
  pthread_mutex_lock(&log_mutex);
  sem_wait(&process_log);
  char hour[7], buffer[MAX];
  get_hour(hour);
  sprintf(buffer, "%s %s", hour, content);
  fprintf(fp, "%s\n", buffer);
  fflush(fp);
  printf("%s\n", buffer);
  sem_post(&process_log);
  pthread_mutex_unlock(&log_mutex);
}

void worker(int num) {
  char buffer[MAX];
  sprintf(buffer, "WORKER %d READY", num);
  write_log(fp, buffer);

  sem_wait(&BLOCK_WORKER);
  available_workers++;
  sem_post(&BLOCK_WORKER);

  while(1) {
    sem_wait(&semaphore);
    sem_wait(&BLOCK_WORKER);
    available_workers--;
    sem_post(&BLOCK_WORKER);

    // Do your thing
    // here

    sem_post(&semaphore);
    sem_wait(&BLOCK_WORKER);
    available_workers++;
    sem_post(&BLOCK_WORKER);
  }

  exit(0);
}

void alert_watcher() {
  write_log(fp, "ALERT WATCHER READY");
  // ve os alertas e o que foi gerado pelo sensor
  // envia mensagem
  while(1);
  exit(0);
}

void * sensor_reader_func(void * param) {
  write_log(fp, "THREAD SENSOR_READER CREATED");
  mem->sens = 1;

  // Do your thing
  while(needQuit(&kill_mutex)) {
    // read from sensors
    // send to dispatcher
  }
  pthread_exit(NULL);
}

void * console_reader_func(void * param) {
  write_log(fp, "THREAD CONSOLE_READER CREATED");
  mem->cons = 1;

  // Do your thing
  while(needQuit(&kill_mutex)) {
    // read from console
    // send to dispatcher
  }
  pthread_exit(NULL);
}


void * dispatcher_func(void * param) {
  write_log(fp, "THREAD DISPATCHER CREATED");
  mem->disp = 1;

  // Do your thing
  while (needQuit(&kill_mutex)) {
    if (available_workers > 0) { // check if there is a worker available
      
    }
  }
  pthread_exit(NULL);
}

void cleanup() {
  /* Kill all processes */
  for (int i = 0; i < N_WORKERS + 1; i++) {
    #ifdef DEBUG
      printf("DEBUG >> %d worker being deleted!\n", processes[i]);
    #endif 
    kill(processes[i], SIGKILL); // TODO: check if process is still doing something
  }

  for (int i = 0; i < N_WORKERS + 1; i++) wait(NULL);

  /* Remove Semaphores */
  sem_destroy(&semaphore);
  sem_destroy(&process_log);
  sem_destroy(&BLOCK_WORKER);
  sem_close(shared_memory_sem);
  sem_unlink(SEM_FILE);

  /* Remove Threads */
  pthread_mutex_unlock(&kill_mutex);

  pthread_join(sensor_reader, NULL);
  pthread_join(console_reader, NULL);
  pthread_join(dispatcher, NULL);

  pthread_mutex_destroy(&kill_mutex);
  pthread_mutex_destroy(&disp_mutex);
  pthread_mutex_destroy(&log_mutex);
  pthread_cond_destroy(&cond);

  /* Remove shared memory */
  shmctl(shmid, IPC_RMID, NULL); // remove the shared memory
  shmdt(mem); // dettach from the shared memory

  /* Close log file */
  write_log(fp, "HOME_IOT SIMULATOR CLOSING");
  fclose(fp);
  exit(0);
}

void sigint_handler(int sig) {
  printf("\n");
  if (sig == SIGINT) {
    write_log(fp, "SIGINT RECEIVED");
    cleanup();
    exit(0);
  }
}

int main(int argc, char **argv) {
	if (argc != 2) {
    printf("INVALID NUMBER OF ARGUMENTS\n");
    exit(0);
  }

  fp = fopen(LOG_FILE, "a+");
  if (fp == NULL) {
    printf("ERROR >> OPENING LOG FILE\n");
    exit(0);
  }

  /* Handling file */
	FILE *cfg = fopen(argv[1], "r");
	if (cfg == NULL) {
    printf("ERROR >> OPENING CONFIG FILE\n");
    fclose(cfg);
		exit(0);
	}

	/* Read Config */
	fscanf(cfg, "%d\n%d\n%d\n%d\n%d", &QUEUE_SZ, &N_WORKERS, &MAX_KEYS, &MAX_SENSORS, &MAX_ALERTS);
	if (QUEUE_SZ < 1 || N_WORKERS < 1 || MAX_KEYS < 1 || MAX_SENSORS < 1 || MAX_ALERTS < 0) {
    printf("ERROR >> IN CONFIG FILE\n");
    fclose(cfg);
		exit(0);
	}
  fclose(cfg);

  /* Signal */
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  // block all signals during setup
  act.sa_handler = SIG_IGN;
  sigaction(SIGINT, &act, NULL);

  /* Mutexes */
  pthread_mutex_lock(&kill_mutex);

  if (pthread_cond_init(&cond, NULL) != 0) {
    printf("ERROR >> CREATING CONDITION VARIABLE\n");
    exit(0);
  }

  /* Semaphores */
  if (sem_init(&process_log, 0, 1) != 0) {
    printf("ERROR >> CREATING PROCESS_LOG SEMAPHORE\n");
    exit(0);
  }

  if (sem_init(&semaphore, 0, N_WORKERS) != 0) {
    printf("ERROR >> CREATING WORKERS SEMAPHORE\n");
    exit(0);
  }

  if (sem_init(&BLOCK_WORKER, 0, 1) != 0) {
    printf("ERROR >> CREATING BLOCK_WORKER SEMAPHORE\n");
    exit(0);
  }

  if ((shared_memory_sem = sem_open(SEM_FILE, O_CREAT, 0666, 1)) == SEM_FAILED) {
    printf("ERROR >> CREATING SHARED MEMORY SEMAPHORE\n");
    exit(0);
  }

  /* Main */
  write_log(fp, "HOME_IOT SIMULATOR STARTING");

  if ((processes = (pid_t *) malloc((N_WORKERS + 1) * sizeof(pid_t))) == NULL) {
    printf("ERROR >> ALLOCATING MEMORY\n");
    exit(0);
  }

	/* Shared Memory */
	if ((shmid = shmget(SHM_KEY, sizeof(Mem_struct*), 0666 | IPC_CREAT | IPC_EXCL)) == -1) {
    printf("ERROR >> CREATING SHARED MEMORY\n");
    exit(0);
	}

  #ifdef DEBUG
    printf("DEBUG >> shmid = %d\n", shmid);
  #endif

	if ((mem = (Mem_struct *) shmat(shmid, NULL, 0)) == (Mem_struct *) -1) {
    printf("ERROR >> ATTACHING SHARED MEMORY\n");
		exit(0);
	}

  mem->cons = 0;
  mem->sens = 0;
  mem->disp = 0;
  mem->max_sensors = MAX_SENSORS;
  mem->max_alerts = MAX_ALERTS;
  // mem->sensors = (Sensor *) malloc(MAX_SENSORS * sizeof(Sensor));
  for (int i = 0; i < MAX_SENSORS; i++) mem->sensors[i] = NULL_SENSOR;


  /* Sensor Reader */
  if (pthread_create(&sensor_reader, NULL, sensor_reader_func, NULL) != 0) {
    printf("ERROR >> CREATING SENSOR_READER\n");
    exit(0);
  }

  /* Console Reader */
  if (pthread_create(&console_reader, NULL, console_reader_func, NULL) != 0) {
    printf("ERROR >> CREATING CONSOLE_READER\n");
    exit(0);
  }

  /* Dispatcher */
  if (pthread_create(&dispatcher, NULL, dispatcher_func, NULL) != 0) {
    printf("ERROR >> CREATING DISPATCHER\n");
    exit(0);
  }

  while (mem->sens == 0 || mem->cons == 0 || mem->disp == 0){}; // wait for threads to be ready

  /* Processes */
  for (int i = 0; i < N_WORKERS+1; i++) {
    if ((processes[i] = fork()) == 0) {
      if (i != 0) worker(i); /* Workers */
      else alert_watcher(); /* Alerts Watcher */
      exit(0);
    } else if (processes[i] < 0) {
      printf("ERROR CREATING WORKER\n");
      exit(0);
    }
  }

  /* Re-enable signal */
  act.sa_handler = sigint_handler;
  sigaction(SIGINT, &act, NULL);

  pause(); // wait for SIGINT
}
