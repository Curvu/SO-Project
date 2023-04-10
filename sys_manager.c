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


/* ----------------------------- */
/*           Variables           */
/* ----------------------------- */
#define LOG_FILE "log.txt"
#define MUTEX_SHM "/mutex_shared_memory"
#define MUTEX_WORKER "/mutex_worker"
#define MUTEX_LOGGER "/mutex_logger"
#define MUTEX_STILL_WORKING "/mutex_still_working"
int QUEUE_SZ, N_WORKERS, MAX_KEYS, MAX_SENSORS, MAX_ALERTS;
FILE *fp;

/* Shared memory */
typedef struct {
  int workers, available_workers, exit;
  Sensor *sensors;
  Alert *alerts;
  int jobs; //! jobs to be done! (only for debugging)
} Mem_struct;

Mem_struct * shm;
int shmid;

/* Signal Actions */
struct sigaction act; // main

/* Workers */
sem_t *BLOCK_LOGGER, *BLOCK_WORKER, *BLOCK_SHM, *STILL_WORKING;
pid_t * processes; 

/* Thread Sensor Reader, Console Reader, Dispatcher */
pthread_t sensor_reader, console_reader, dispatcher;

/* Mutexes */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //! will be used in pipes


/* ----------------------------- */
/*           Write Log           */
/* ----------------------------- */
void write_log(FILE *fp, char * content) {
  sem_wait(BLOCK_LOGGER);
  char buffer[MAX], *time_buff = get_hour();
  sprintf(buffer, "%s %s", time_buff, content);
  fprintf(fp, "%s\n", buffer);
  fflush(fp);
  printf("%s\n", buffer);
  free(time_buff);
  sem_post(BLOCK_LOGGER);
}


/* ----------------------------- */
/*       Process Functions       */
/* ----------------------------- */
void worker(int num) {
  char buffer[MAX];
  sprintf(buffer, "WORKER %d READY", num);

  sem_wait(BLOCK_SHM);
  shm->available_workers++;
  sem_post(BLOCK_SHM);

  while(1) {
    // Worker is ready to receive job
    write_log(fp, buffer);
    
    // Wait for job
    sem_wait(BLOCK_WORKER);
  
    //* start job
    sem_wait(BLOCK_SHM);
    if (shm->available_workers == shm->workers) sem_wait(STILL_WORKING); // is the first worker to start
    shm->available_workers--;
    sem_post(BLOCK_SHM);

    //! Do your thing here
    #ifdef DEBUG
      printf("DEBUG >> Worker %d doing job!\n", num); fflush(stdout);
      srand(time(NULL));
      sleep(rand() % 5);
    #endif

    //* finish job
    sem_wait(BLOCK_SHM);
    shm->available_workers++;
    if (shm->available_workers == shm->workers) sem_post(STILL_WORKING); // is the last worker to finish
    sem_post(BLOCK_SHM);
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


/* ----------------------------- */
/*        Thread Functions       */
/* ----------------------------- */
void * sensor_reader_func(void * param) {
  write_log(fp, "THREAD SENSOR_READER CREATED");
  while(shm->exit == 0) {
    // read from sensors
    // send to dispatcher
  }

  #ifdef DEBUG
    printf("DEBUG >> Sensor Reader exiting!\n");
  #endif
  pthread_exit(NULL);
}

void * console_reader_func(void * param) {
  write_log(fp, "THREAD CONSOLE_READER CREATED");
  while(shm->exit == 0) {
    // read from console
    // send to dispatcher
  }
  #ifdef DEBUG
    printf("DEBUG >> Console Reader exiting!\n");
  #endif
  pthread_exit(NULL);
}

void * dispatcher_func(void * param) {
  write_log(fp, "THREAD DISPATCHER CREATED");
  while (shm->exit == 0) {
    //! if receive job from sensor_reader or console_reader do your thing:
    if (shm->jobs > 0) {
      if (shm->available_workers > 0) { // check if there is a worker available
        shm->jobs--;
        //! send job to worker
        #ifdef DEBUG
          printf("DEBUG >> Sending job to worker!\n");
        #endif

        //* signal worker to start job
        sem_post(BLOCK_WORKER);
      }
    }
  }
  #ifdef DEBUG
    printf("DEBUG >> Dispatcher exiting!\n");
  #endif
  pthread_exit(NULL);
}


/* ----------------------------- */
/*            Cleanup            */
/* ----------------------------- */
void cleanup() {
  /* Remove Threads */
  shm->exit = 1;
  pthread_join(sensor_reader, NULL);
  pthread_join(console_reader, NULL);
  pthread_join(dispatcher, NULL);

  /* Kill all processes */
  sem_wait(STILL_WORKING); // wait for all workers to finish their job
  for (int i = 0; i < N_WORKERS + 1; i++) {
    #ifdef DEBUG
      printf("DEBUG >> %d worker being deleted!\n", processes[i]);
    #endif
    if (processes[i] > 0) kill(processes[i], SIGKILL); // TODO: check if process is still doing something
  }
  for (int i = 0; i < N_WORKERS + 1; i++) wait(NULL);

  #ifdef DEBUG
    printf("DEBUG >> All threads and processes deleted!\n");
  #endif

  /* Destroy mutex */
  pthread_mutex_destroy(&mutex);

  /* Remove shared memory */
  if (shmid > 0) {
    shmctl(shmid, IPC_RMID, NULL); // remove the shared memory
    shmdt(shm); // dettach from the shared memory
  }

  /* Free memory */
  free(processes);

  /* Close log file */
  if (fp) {
    write_log(fp, "HOME_IOT SIMULATOR CLOSING");
    fclose(fp);
  }

  /* Remove Semaphores */
  sem_close(BLOCK_WORKER);
  sem_unlink(MUTEX_WORKER);
  sem_close(BLOCK_LOGGER);
  sem_unlink(MUTEX_LOGGER);
  sem_close(BLOCK_SHM);
  sem_unlink(MUTEX_SHM);
  sem_close(STILL_WORKING);
  sem_unlink(MUTEX_STILL_WORKING);

  exit(0);
}


/* ----------------------------- */
/*         Error Handlers        */
/* ----------------------------- */
void handle_error(char * content) {
  printf("ERROR >> %s\n", content);
  exit(EXIT_FAILURE);
}

void handle_error_log(FILE *fp, char * content) {
  write_log(fp, content);
  cleanup();
  exit(EXIT_FAILURE);
}


/* ----------------------------- */
/*        Signal Handlers        */
/* ----------------------------- */
void sigint_handler(int sig) { // ctrl + c
  printf("\n");
  if (sig == SIGINT) {
    write_log(fp, "SIGINT RECEIVED");
    cleanup();
    exit(0);
  }
}

void sigtstp_handler(int sig) { // ctrl + z
  printf("\n");
  if (sig == SIGTSTP) {
    write_log(fp, "SIGTSTP RECEIVED");
    #ifdef DEBUG
      printf("DEBUG >> %d workers available 🧌\n", shm->available_workers); fflush(stdout);
    #endif
  }
}

void sigquit_handler(int sig) { // ctrl + backslash
  printf("\n");
  if (sig == SIGQUIT) {
    sem_wait(BLOCK_SHM);
    shm->jobs++;
    sem_post(BLOCK_SHM);
    printf("DEBUG >> Added 1 job to queue 🧵\n"); fflush(stdout);
  }
}


/* ----------------------------- */
/*             Main              */
/* ----------------------------- */
int main(int argc, char **argv) {
  if (argc != 2) handle_error("INVALID NUMBER OF ARGUMENTS");
  if ((fp = fopen(LOG_FILE, "a+")) == NULL) handle_error("OPENING LOG FILE");

  /* Handling file */
  FILE *cfg = fopen(argv[1], "r");
  if (cfg == NULL) handle_error("OPENING CONFIG FILE");

  /* Read Config */
  fscanf(cfg, "%d\n%d\n%d\n%d\n%d", &QUEUE_SZ, &N_WORKERS, &MAX_KEYS, &MAX_SENSORS, &MAX_ALERTS);
  if (QUEUE_SZ < 1 || N_WORKERS < 1 || MAX_KEYS < 1 || MAX_SENSORS < 1 || MAX_ALERTS < 0) handle_error("INVALID CONFIG FILE");
  fclose(cfg);

  /* Signal */
  act.sa_flags = 0;
  // block all signals during setup
  sigfillset(&act.sa_mask);
  sigdelset(&act.sa_mask, SIGINT);
  sigdelset(&act.sa_mask, SIGTSTP);
  #ifdef DEBUG
    sigdelset(&act.sa_mask, SIGQUIT); // ctrl + backslash
  #endif
  sigprocmask(SIG_SETMASK, &act.sa_mask, NULL); // this will block all signals

  act.sa_handler = SIG_IGN;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTSTP, &act, NULL);
  #ifdef DEBUG
    sigaction(SIGQUIT, &act, NULL); // ctrl + backslash
  #endif

  /* Semaphores */
  if ((BLOCK_SHM = sem_open(MUTEX_SHM, O_CREAT, 0666, 1)) == SEM_FAILED) handle_error("INITIALIZING BLOCK_SHM SEMAPHORE");          // block when someone is using the shared memory
  if ((BLOCK_WORKER = sem_open(MUTEX_WORKER, O_CREAT, 0666, 0)) == SEM_FAILED) handle_error("INITIALIZING BLOCK_WORKER SEMAPHORE"); // block workers when there are no jobs
  if ((BLOCK_LOGGER = sem_open(MUTEX_LOGGER, O_CREAT, 0666, 1)) == SEM_FAILED) handle_error("INITIALIZING BLOCK_LOGGER SEMAPHORE"); // block when someone is using the log file
  if ((STILL_WORKING = sem_open(MUTEX_STILL_WORKING, O_CREAT, 0666, 1)) == SEM_FAILED) handle_error("INITIALIZING STILL_WORKING SEMAPHORE"); // block when there are still workers working

  /* Main */
  write_log(fp, "HOME_IOT SIMULATOR STARTING");

  /* Shared Memory */
  if ((shmid = shmget(IPC_PRIVATE, sizeof(Mem_struct*), 0666 | IPC_CREAT | IPC_EXCL)) == -1) handle_error_log(fp, "ERROR >> CREATING SHARED MEMORY");
  if ((shm = (Mem_struct *) shmat(shmid, NULL, 0)) == (Mem_struct *) -1) handle_error_log(fp, "ERROR >> ATTACHING SHARED MEMORY");

  /* Initialize Shared Memory */
  shm->workers = N_WORKERS;
  shm->available_workers = 0;
  shm->exit = 0;
  shm->jobs = 0; //! DEBUG -> delete me later
  shm->sensors = (Sensor *) malloc(MAX_SENSORS * sizeof(Sensor));
  for (int i = 0; i < MAX_SENSORS; i++) shm->sensors[i] = NULL_SENSOR;
  shm->alerts = (Alert *) malloc(MAX_ALERTS * sizeof(Alert));
  for (int i = 0; i < MAX_ALERTS; i++) shm->alerts[i] = NULL_ALERT;

  #ifdef DEBUG
    printf("DEBUG >> shmid = %d\n", shmid);
  #endif

  /* Sensor Reader, Console Reader and Dispatcher */
  if (pthread_create(&sensor_reader, NULL, sensor_reader_func, NULL) != 0) handle_error_log(fp, "ERROR >> CREATING SENSOR_READER");
  if (pthread_create(&console_reader, NULL, console_reader_func, NULL) != 0) handle_error_log(fp, "ERROR >> CREATING CONSOLE_READER");
  if (pthread_create(&dispatcher, NULL, dispatcher_func, NULL) != 0) handle_error_log(fp, "ERROR >> CREATING DISPATCHER");

  /* Processes */
  if ((processes = (pid_t *) malloc((N_WORKERS + 1) * sizeof(pid_t))) == NULL) handle_error_log(fp, "ERROR >> ALLOCATING MEMORY");
  for (int i = 0; i < N_WORKERS+1; i++) {
    if ((processes[i] = fork()) == 0) {
      if (i != 0) worker(i); /* Workers */
      else alert_watcher(); /* Alerts Watcher */
      exit(0);
    } else if (processes[i] < 0) handle_error_log(fp, "ERROR CREATING PROCESS");
  }

  /* Re-enable signal */
  act.sa_handler = sigint_handler;
  sigaction(SIGINT, &act, NULL);
  act.sa_handler = sigtstp_handler;
  sigaction(SIGTSTP, &act, NULL);
  #ifdef DEBUG
    act.sa_handler = &sigquit_handler;
    sigaction(SIGQUIT, &act, NULL); // ctrl + backslash
  #endif

  while(1) pause(); // wait for SIGINT
}
