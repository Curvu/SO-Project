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
#include <sys/stat.h>
#include <sys/select.h>
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
  int workers, available_workers;
  Sensor *sensors;
  Alert *alerts;
} Mem_struct;

Mem_struct * shm;
int shmid;

/* Signal Actions */
struct sigaction act; // main

/* Workers */
sem_t *BLOCK_LOGGER, *BLOCK_WORKER, *BLOCK_SHM, *STILL_WORKING;
pid_t *processes; 

/* Thread Sensor Reader */
pthread_t sensor_reader;
int fd_sensor;

/* Thread Console Reader */
pthread_t console_reader;
int fd_user;

/* Thread Dispatcher */
pthread_t dispatcher;


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
/*            Cleanup            */
/* ----------------------------- */
void cleanup() {
  /* Remove Threads */
  pthread_kill(sensor_reader, SIGUSR1);
  pthread_kill(console_reader, SIGUSR1);
  pthread_kill(dispatcher, SIGUSR1);
  pthread_join(sensor_reader, NULL);
  pthread_join(console_reader, NULL);
  pthread_join(dispatcher, NULL);

  /* Kill all processes */
  sem_wait(STILL_WORKING); // wait for all workers to finish their job
  for (int i = 0; i < N_WORKERS + 1; i++) {
    #ifdef DEBUG
      printf("DEBUG >> %d worker being deleted!\n", processes[i]);
    #endif
    if (processes[i] > 0) kill(processes[i], SIGKILL);
  }
  for (int i = 0; i < N_WORKERS + 1; i++) wait(NULL);

  #ifdef DEBUG
    printf("DEBUG >> All threads and processes deleted!\n");
  #endif

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

  /* Remove FIFO's */ //TODO: FINISH ME
  unlink(SENSOR_FIFO);
  unlink(USER_FIFO);

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
}

void alert_watcher() {
  write_log(fp, "ALERT WATCHER READY");
  // ve os alertas e o que foi gerado pelo sensor
  // envia mensagem
  while(1);
}


/* ----------------------------- */
/*        Thread Functions       */
/* ----------------------------- */
void * sensor_reader_func(void * param) {
  write_log(fp, "THREAD SENSOR_READER CREATED");

  // open pipe
  if ((fd_sensor = open(SENSOR_FIFO, O_RDWR)) < 0) handle_error_log(fp, "open sensor fifo");

  char buffer[MAX];
  int size;
  fd_set set;

  while(1) {
    FD_ZERO(&set);
    FD_SET(fd_sensor, &set);
    
    if (select(fd_sensor + 1, &set, NULL, NULL, NULL) < 0) handle_error_log(fp, "select sensor fifo");
    if ((size = read(fd_sensor, buffer, MAX)) < 0) handle_error_log(fp, "read sensor fifo");
    buffer[size] = '\0';

    #ifdef DEBUG
      printf("DEBUG >> Sensor Reader read: %s\n", buffer);
    #endif

    if (buffer[0] == '>') { /* Register Sensor */
      Sensor s;
      sscanf(buffer, " >%[^#]#%[^#]#%d#%d#%d", s.id, s.key, &s.min, &s.max, &s.inter);
      for (int i = 0; i < MAX_SENSORS; i++) {
        if (compareSensors(&shm->sensors[i], &NULL_SENSOR)) {
          cpySensor(&shm->sensors[i], &s);
          #ifdef DEBUG
            printf("DEBUG >> SENSOR CREATED: {%s, %s, %d, %d, %d}\n", shm->sensors[i].id, shm->sensors[i].key, shm->sensors[i].min, shm->sensors[i].max, shm->sensors[i].inter);
          #endif
          break;
        }
      }
    } else if (buffer[0] == '<') { /* Unregister Sensor */
      char id[STR], key[STR];
      sscanf(buffer, " <%[^#]#%[^#]", id, key);
      for (int i = 0; i < MAX_SENSORS; i++) {
        if (checkSensor(&shm->sensors[i], id, key)) {
          cpySensor(&shm->sensors[i], &NULL_SENSOR);
          #ifdef DEBUG
            printf("DEBUG >> SENSOR REMOVED: {%s, %s, %d, %d, %d}\n", shm->sensors[i].id, shm->sensors[i].key, shm->sensors[i].min, shm->sensors[i].max, shm->sensors[i].inter);
          #endif
          break;
        }
      }
    } else { /* Send data to internal queue */
      char id[STR], key[STR];
      sscanf(buffer, " %[^#]#%[^#]#%*d", id, key);
      int flag = 0;
      for (int i = 0; i < MAX_SENSORS; i++) {
        if (checkSensor(&shm->sensors[i], id, key)) {
          flag = 1;
          break;
        }
      }

      if (flag) {
        //! send to dispatcher (internal queue)
        #ifdef DEBUG
          printf("DEBUG >> Sending job to dispatcher! 😼\n");
        #endif
      }
    }
  }
}

void * console_reader_func(void * param) {
  write_log(fp, "THREAD CONSOLE_READER CREATED");

  // open pipe
  if ((fd_user = open(USER_FIFO, O_RDWR)) < 0) handle_error_log(fp, "open user fifo");

  Message msg;
  int size;
  fd_set set;

  while(1) {
    FD_ZERO(&set);
    FD_SET(fd_user, &set);

    if (select(fd_user + 1, &set, NULL, NULL, NULL) < 0) handle_error_log(fp, "select user fifo");
    if ((size = read(fd_user, &msg, sizeof(Message))) < 0) handle_error_log(fp, "read user fifo");
    
    if (msg.command == 1) { // add alert
      for (int i = 0; i < MAX_ALERTS; i++) {
        
      }
    } else if (msg.command == 2) { // remove alert

    } else if (msg.command == 3) { // list alerts

    } else if (msg.command == 4) { // sensors

    } else if (msg.command == 5) { // stats 

    } else if (msg.command == 6) { // reset

    }
  }
}

void * dispatcher_func(void * param) {
  write_log(fp, "THREAD DISPATCHER CREATED");
  while (1) {
    //! if receive job from sensor_reader or console_reader do your thing:
    if (0) {
      if (shm->available_workers > 0) { // check if there is a worker available
        //! send job to worker
        #ifdef DEBUG
          printf("DEBUG >> Sending job to worker!\n");
        #endif

        //* signal worker to start job
        sem_post(BLOCK_WORKER); // TODO: change to conditional variable
      }
    }
  }
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

void sigusr1_handler(int sig) { // to close threads
  if (sig == SIGUSR1) {
    close(fd_sensor);
    close(fd_user);
    pthread_exit(NULL);
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

  /* Signal (block all signals during setup) */
  act.sa_flags = 0;
  sigfillset(&act.sa_mask);
  sigdelset(&act.sa_mask, SIGINT);
  sigdelset(&act.sa_mask, SIGTSTP);
  sigdelset(&act.sa_mask, SIGUSR1);
  sigprocmask(SIG_SETMASK, &act.sa_mask, NULL); // this will block all signals

  /* Signal Handlers - for now both are blocked */
  act.sa_handler = SIG_IGN;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTSTP, &act, NULL);
  sigaction(SIGUSR1, &act, NULL); // to close threads

  /* Semaphores */
  if ((BLOCK_SHM = sem_open(MUTEX_SHM, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) handle_error("INITIALIZING BLOCK_SHM SEMAPHORE");          // block when someone is using the shared memory
  if ((BLOCK_WORKER = sem_open(MUTEX_WORKER, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) handle_error("INITIALIZING BLOCK_WORKER SEMAPHORE"); // block workers when there are no jobs
  if ((BLOCK_LOGGER = sem_open(MUTEX_LOGGER, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) handle_error("INITIALIZING BLOCK_LOGGER SEMAPHORE"); // block when someone is using the log file
  if ((STILL_WORKING = sem_open(MUTEX_STILL_WORKING, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) handle_error("INITIALIZING STILL_WORKING SEMAPHORE"); // block when there are still workers working

  /* Create FIFO's */
  if (mkfifo(SENSOR_FIFO, 0666) == -1) handle_error("CREATING SENSOR FIFO");
  if (mkfifo(USER_FIFO, 0666) == -1) handle_error("CREATING USER FIFO");

  /* Main */
  write_log(fp, "HOME_IOT SIMULATOR STARTING");

  /* Shared Memory */
  size_t mem_struct_size = sizeof(Mem_struct);
  size_t sensors_size = MAX_SENSORS*sizeof(Sensor);
  size_t alerts_size = MAX_ALERTS*sizeof(Alert);
  if ((shmid = shmget(IPC_PRIVATE, mem_struct_size + sensors_size + alerts_size, 0666 | IPC_CREAT | IPC_EXCL)) == -1) handle_error_log(fp, "ERROR >> CREATING SHARED MEMORY");
  if ((shm = (Mem_struct *) shmat(shmid, NULL, 0)) == (Mem_struct *) -1) handle_error_log(fp, "ERROR >> ATTACHING SHARED MEMORY");

  /* Initialize Shared Memory */
  shm->workers = N_WORKERS;
  shm->available_workers = 0;
  shm->sensors = (Sensor *)((void *)shm + mem_struct_size); 
  shm->alerts = (Alert *)((void *)shm + mem_struct_size + sensors_size);
  for (int i = 0; i < MAX_SENSORS; i++) shm->sensors[i] = NULL_SENSOR;
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

  /* Re-enable signals */
  act.sa_handler = sigint_handler;
  sigaction(SIGINT, &act, NULL);
  act.sa_handler = sigtstp_handler;
  sigaction(SIGTSTP, &act, NULL);
  /* Signal for threads */
  act.sa_handler = sigusr1_handler;
  sigaction(SIGUSR1, &act, NULL);

  while(1) pause(); // wait for SIGINT
}
