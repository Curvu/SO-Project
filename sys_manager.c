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

#define MUTEX_FILE "/mutex"
#define LOG_FILE "log.txt"

int QUEUE_SZ, N_WORKERS, MAX_KEYS, MAX_SENSORS, MAX_ALERTS;
FILE *fp;

/* Shared memory */
typedef struct {
	//TODO
} Mem_struct;

int shmid;
Mem_struct * mem;

/* Mutex for write_log */
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
// TODO: mutex para ver se alguem ainda está a trabalhar

/* Signal Actions */
struct sigaction act;

/* Workers */
sem_t * sem_workers;
pid_t * processes; 

/* Thread Sensor Reader */
pthread_t sensor_reader;

/* Thread Console Reader */
pthread_t console_reader;
int *write_pos, *read_pos;

/* Thread Dispatcher */
pthread_t dispatcher;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t disp_mutex = PTHREAD_MUTEX_INITIALIZER;

void write_log(FILE *fp, char * content) {
  pthread_mutex_lock(&log_mutex);
  char hour[7], buffer[MAX];
  get_hour(hour);
  sprintf(buffer, "%s %s", hour, content);
  fprintf(fp, "%s\n", buffer);
  fflush(fp);
  printf("%s\n", buffer);
  pthread_mutex_unlock(&log_mutex);
}

void worker(int num) {
  char buffer[MAX];
  sprintf(buffer, "WORKER %d READY", num);
  write_log(fp, buffer);

  while(1) {
    sem_wait(sem_workers);
    // Do your thing
    sem_post(sem_workers);
    pthread_cond_signal(&cond);
  }

  exit(0);
}

void alert_watcher() {
  // ve os alertas e o que foi gerado pelo sensor
  // envia mensagem
  printf("ALERT WATCHER READY\n");
  exit(0);
}

void * sensor_reader_func(void * param) {
  write_log(fp, "THREAD SENSOR_READER CREATED");
  // Do your thing

  pthread_exit(NULL);
}

void * console_reader_func(void * param) {
  write_log(fp, "THREAD CONSOLE_READER CREATED");
  // Do your thing

  pthread_exit(NULL);
}

void * dispatcher_func(void * param) { // LEMBRAR QUE È PRECISO SINCRONIZAR COM OS WORKERS QUANDO NENHUM ESTÀ DISPONIVEL
  write_log(fp, "THREAD DISPATCHER CREATED");

  // Do your thing
  while (1) {
    // check if there is a worker available
    if (sem_trywait(sem_workers) == 0) {
      sem_post(sem_workers);
      // if there is, send the job to the worker

      // if there isn't, wait for a worker to be available
    } else pthread_cond_wait(&cond, &disp_mutex);
  }


  pthread_exit(NULL);
}

void cleanup() {
  /* Kill all processes */
  for (int i = 0; i < N_WORKERS; i++) {
    #ifdef DEBUG
      printf("DEBUG >> %d worker being deleted!\n", processes[i]);
    #endif 
    kill(processes[i], SIGKILL);
  }
  printf("DEBUG >> all workers deleted!\n");


  /* Remove Semaphores */
  sem_unlink(MUTEX_FILE);
  sem_close(sem_workers);

  /* Remove Threads */
  pthread_cancel(sensor_reader);
  pthread_cancel(console_reader);
  pthread_cancel(dispatcher);
  pthread_mutex_destroy(&disp_mutex);
  pthread_mutex_destroy(&log_mutex);
  pthread_cond_destroy(&cond);

  /* Remove shared memory */
  shmctl(shmid, IPC_RMID, NULL); // remove the shared memory
  shmdt(mem); // dettach from the shared memory

  #ifdef DEBUG
    printf("DEBUG >> leaving!\n");
  #endif
  write_log(fp, "HOME_IOT SIMULATOR CLOSING");
  fclose(fp);
}

void sigint_handler(int sig) {
  // TODO: finish me
  cleanup();
  exit(0);
}

int main(int argc, char **argv) {
	if (argc != 2) {
    printf("INVALID NUMBER OF ARGUMENTS\n");
    exit(0);
  }

  fp = fopen(LOG_FILE, "a+");
  if (fp == NULL) {
    printf("ERROR OPENING LOG FILE\n");
    exit(0);
  }

  // Log mutex
  if (pthread_mutex_init(&log_mutex, NULL) != 0) {
    write_log(fp, "ERROR CREATING LOG MUTEX");
    exit(0);
  }

  write_log(fp, "HOME_IOT SIMULATOR STARTING");

	/* Handling file */
	FILE *cfg = fopen(argv[1], "r");
	if (cfg == NULL) {
    write_log(fp, "ERROR OPENING CONFIG FILE");
    fclose(cfg);
		exit(0);
	}

	/* Read Config */
	fscanf(cfg, "%d\n%d\n%d\n%d\n%d", &QUEUE_SZ, &N_WORKERS, &MAX_KEYS, &MAX_SENSORS, &MAX_ALERTS);
	if (QUEUE_SZ < 1 || N_WORKERS < 1 || MAX_KEYS < 1 || MAX_SENSORS < 1 || MAX_ALERTS < 0) {
    write_log(fp, "ERROR IN CONFIG FILE");
    fclose(cfg);
		exit(0);
	}
  fclose(cfg);

  if ((processes = (pid_t *) malloc((N_WORKERS + 1) * sizeof(pid_t))) == NULL) {
    write_log(fp, "ERROR ALLOCATING MEMORY");
    exit(0);
  }

  #ifdef DEBUG
    printf("DEBUG >> Creating shared memory!\n");
  #endif

	/* Creating Shared Memory */
	if ((shmid = shmget(IPC_PRIVATE, sizeof(Mem_struct*), 0666 | IPC_CREAT)) == -1){
    write_log(fp, "ERROR CREATING SHARED MEMORY");
    exit(0);
	}

  #ifdef DEBUG
    printf("DEBUG >> shmid = %d\n", shmid);
  #endif

	if ((mem = (Mem_struct *) shmat(shmid, NULL, 0)) == (Mem_struct *) -1) {
    write_log(fp, "ERROR ATTACHING SHARED MEMORY");
		exit(0);
	}

  #ifdef DEBUG
    printf("DEBUG >> Creating semaphores!\n");
  #endif

  /* Creating Semaphores */
  if ((sem_workers = sem_open(MUTEX_FILE, O_CREAT, 0700, N_WORKERS)) == SEM_FAILED) {
    write_log(fp, "ERROR CREATING WORKERS SEMAPHORE");
    exit(0);
  }

  // THREADS!!
  #ifdef DEBUG
    printf("DEBUG >> Creating threads!\n");
  #endif

  /* Sensor Reader */
  if (pthread_create(&sensor_reader, NULL, sensor_reader_func, NULL) != 0) {
    write_log(fp, "ERROR CREATING SENSOR_READER");
    exit(0);
  }

  /* Console Reader */
  if (pthread_create(&console_reader, NULL, console_reader_func, NULL) != 0) {
    write_log(fp, "ERROR CREATING CONSOLE_READER");
    exit(0);
  }

  /* Dispatcher */
  if (pthread_create(&dispatcher, NULL, dispatcher_func, NULL) != 0) {
    write_log(fp, "ERROR CREATING DISPATCHER");
    exit(0);
  }

  #ifdef DEBUG
    printf("DEBUG >> Creating workers!\n");
  #endif

  /* Workers */
  for (int i = 0; i < N_WORKERS; i++) {
    if ((processes[i] = fork()) == 0) {
      worker(i + 1);
      exit(0);
    } else if (processes[i] < 0) {
      write_log(fp, "ERROR CREATING WORKER");
      exit(0);
    }
  }

  #ifdef DEBUG
    printf("DEBUG >> Creating alerts watcher!\n");
  #endif

  /* Alerts Watcher */
  if ((processes[N_WORKERS] = fork()) == 0) {
    alert_watcher();
    exit(0);
  } else if (processes[N_WORKERS] < 0) {
    write_log(fp, "ERROR CREATING ALERTS WATCHER");
    exit(0);
  }

  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  act.sa_handler = sigint_handler;
  sigaction(SIGINT, &act, NULL);
  
  pthread_join(sensor_reader, NULL);
  pthread_join(console_reader, NULL);
  pthread_join(dispatcher, NULL);
  for (int i = 0; i < N_WORKERS; i++) waitpid(processes[i], NULL, 0);
  cleanup();
  
  return 0;
}
