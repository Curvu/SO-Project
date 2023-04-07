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
typedef struct {
  int sens, cons, disp;
  Sensor *sensors;
  Alert *alerts;
} Mem_struct;

int shmid;
Mem_struct * mem;
sem_t BLOCK_SHM;

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
  char buffer[MAX], *time_buff = get_hour();
  sprintf(buffer, "%s %s", time_buff, content);
  fprintf(fp, "%s\n", buffer);
  fflush(fp);
  printf("%s\n", buffer);
  free(time_buff);
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
  sem_wait(&BLOCK_SHM);
  mem->sens = 1;
  sem_post(&BLOCK_SHM);

  // Do your thing
  while(needQuit(&kill_mutex)) {
    // read from sensors
    // send to dispatcher
  }
  pthread_exit(NULL);
}

void * console_reader_func(void * param) {
  write_log(fp, "THREAD CONSOLE_READER CREATED");
  sem_wait(&BLOCK_SHM);
  mem->cons = 1;
  sem_post(&BLOCK_SHM);

  // Do your thing
  while(needQuit(&kill_mutex)) {
    // read from console
    // send to dispatcher
  }
  pthread_exit(NULL);
}

void * dispatcher_func(void * param) {
  write_log(fp, "THREAD DISPATCHER CREATED");
  sem_wait(&BLOCK_SHM);
  mem->disp = 1;
  sem_post(&BLOCK_SHM);

  // Do your thing
  while (needQuit(&kill_mutex)) {
    sem_wait(&semaphore);
    if (available_workers > 0) { // check if there is a worker available
      // send job to worker
      sem_post(&semaphore);
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
    if (processes[i] > 0) kill(processes[i], SIGKILL); // TODO: check if process is still doing something
  }
  for (int i = 0; i < N_WORKERS + 1; i++) wait(NULL);

  /* Remove Semaphores */
  sem_destroy(&semaphore);
  sem_destroy(&process_log);
  sem_destroy(&BLOCK_WORKER);
  sem_destroy(&BLOCK_SHM);

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
  if (shmid > 0) {
    shmctl(shmid, IPC_RMID, NULL); // remove the shared memory
    shmdt(mem); // dettach from the shared memory
  }

  /* Close log file */
  if (fp) {
    write_log(fp, "HOME_IOT SIMULATOR CLOSING");
    fclose(fp);
  }
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

void sigtstp_handler(int sig) {
  printf("\n");
  if (sig == SIGTSTP) {
    write_log(fp, "SIGTSTP RECEIVED");
    #ifdef DEBUG
      printf("DEBUG >> %d workers available 🧌\n", available_workers);
    #endif
  }
}

int main(int argc, char **argv) {
	if (argc != 2) {
    printf("INVALID NUMBER OF ARGUMENTS\n");
    exit(EXIT_FAILURE);
  }

  fp = fopen(LOG_FILE, "a+");
  if (fp == NULL) {
    printf("ERROR >> OPENING LOG FILE\n");
    exit(EXIT_FAILURE);
  }

  /* Handling file */
  FILE *cfg = fopen(argv[1], "r");
  if (cfg == NULL) {
    printf("ERROR >> OPENING CONFIG FILE\n");
    fclose(cfg);
    exit(EXIT_FAILURE);
  }

  /* Read Config */
  fscanf(cfg, "%d\n%d\n%d\n%d\n%d", &QUEUE_SZ, &N_WORKERS, &MAX_KEYS, &MAX_SENSORS, &MAX_ALERTS);
  if (QUEUE_SZ < 1 || N_WORKERS < 1 || MAX_KEYS < 1 || MAX_SENSORS < 1 || MAX_ALERTS < 0) {
    printf("ERROR >> IN CONFIG FILE\n");
    fclose(cfg);
    exit(EXIT_FAILURE);
  }
  fclose(cfg);

  /* Signal */
  act.sa_flags = 0;
  // block all signals during setup
  sigfillset(&act.sa_mask);
  sigdelset(&act.sa_mask, SIGINT);
  sigdelset(&act.sa_mask, SIGTSTP);
  sigprocmask(SIG_SETMASK, &act.sa_mask, NULL); // this will block all signals

  act.sa_handler = SIG_IGN;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTSTP, &act, NULL);

  /* Mutexes */
  pthread_mutex_lock(&kill_mutex);

  if (pthread_cond_init(&cond, NULL) != 0) {
    printf("ERROR >> CREATING CONDITION VARIABLE\n");
    exit(EXIT_FAILURE);
  }

  /* Semaphores */
  if (sem_init(&process_log, 0, 1) != 0) {
    printf("ERROR >> CREATING PROCESS_LOG SEMAPHORE\n");
    exit(EXIT_FAILURE);
  }

  if (sem_init(&semaphore, 0, N_WORKERS) != 0) {
    printf("ERROR >> CREATING WORKERS SEMAPHORE\n");
    exit(EXIT_FAILURE);
  }

  if (sem_init(&BLOCK_WORKER, 0, 1) != 0) {
    printf("ERROR >> CREATING BLOCK_WORKER SEMAPHORE\n");
    exit(EXIT_FAILURE);
  }

  if ((sem_init(&BLOCK_SHM, 0, 1)) != 0) {
    printf("ERROR >> CREATING BLOCK_SHM SEMAPHORE\n");
    exit(EXIT_FAILURE);
  }

  /* Main */
  write_log(fp, "HOME_IOT SIMULATOR STARTING");

  if ((processes = (pid_t *) malloc((N_WORKERS + 1) * sizeof(pid_t))) == NULL) {
    write_log(fp, "ERROR >> ALLOCATING MEMORY");
    cleanup();
    exit(EXIT_FAILURE);
  }

	/* Shared Memory */
	if ((shmid = shmget(IPC_PRIVATE, sizeof(Mem_struct*), 0666 | IPC_CREAT | IPC_EXCL)) == -1) {
    write_log(fp, "ERROR >> CREATING SHARED MEMORY");
    cleanup();
    exit(EXIT_FAILURE);
	}

  #ifdef DEBUG
    printf("DEBUG >> shmid = %d\n", shmid);
  #endif

	if ((mem = (Mem_struct *) shmat(shmid, NULL, 0)) == (Mem_struct *) -1) {
    write_log(fp, "ERROR >> ATTACHING SHARED MEMORY");
    cleanup();
		exit(EXIT_FAILURE);
	}

  sem_wait(&BLOCK_SHM);
  mem->cons = 0;
  mem->sens = 0;
  mem->disp = 0;
  mem->sensors = (Sensor *) malloc(MAX_SENSORS * sizeof(Sensor));
  for (int i = 0; i < MAX_SENSORS; i++) mem->sensors[i] = NULL_SENSOR;
  mem->alerts = (Alert *) malloc(MAX_ALERTS * sizeof(Alert));
  for (int i = 0; i < MAX_ALERTS; i++) mem->alerts[i] = NULL_ALERT;
  sem_post(&BLOCK_SHM);

  /* Sensor Reader */
  if (pthread_create(&sensor_reader, NULL, sensor_reader_func, NULL) != 0) {
    write_log(fp, "ERROR >> CREATING SENSOR_READER");
    cleanup();
    exit(EXIT_FAILURE);
  }

  /* Console Reader */
  if (pthread_create(&console_reader, NULL, console_reader_func, NULL) != 0) {
    write_log(fp, "ERROR >> CREATING CONSOLE_READER");
    cleanup();
    exit(EXIT_FAILURE);
  }

  /* Dispatcher */
  if (pthread_create(&dispatcher, NULL, dispatcher_func, NULL) != 0) {
    write_log(fp, "ERROR >> CREATING DISPATCHER");
    cleanup();
    exit(EXIT_FAILURE);
  }

  while (mem->sens == 0 || mem->cons == 0 || mem->disp == 0){}; // wait for threads to be ready

  /* Processes */
  for (int i = 0; i < N_WORKERS+1; i++) {
    if ((processes[i] = fork()) == 0) {
      if (i != 0) worker(i); /* Workers */
      else alert_watcher(); /* Alerts Watcher */
      exit(0);
    } else if (processes[i] < 0) {
      write_log(fp, "ERROR CREATING PROCESS");
      cleanup();
      exit(EXIT_FAILURE);
    }
  }

  /* Re-enable signal */
  act.sa_handler = sigint_handler;
  sigaction(SIGINT, &act, NULL);
  act.sa_handler = sigtstp_handler;
  sigaction(SIGTSTP, &act, NULL);

  while(1) pause(); // wait for SIGINT
}
