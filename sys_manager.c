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
#include <sys/msg.h>
#include "lib/functions.h"


/* ----------------------------- */
/*           Variables           */
/* ----------------------------- */
#define LOG_FILE "log.txt"
#define MUTEX_SHM "/mutex_shared_memory"
#define MUTEX_WORKER "/mutex_worker"
#define MUTEX_LOGGER "/mutex_logger"
#define MUTEX_STILL_WORKING "/mutex_still_working"
#define MUTEX_QUEUE "/mutex_queue"
int QUEUE_SZ, N_WORKERS, MAX_KEYS, MAX_SENSORS, MAX_ALERTS;
FILE *fp;

/* Shared Memory */
typedef struct {
  int workers, available_workers;
  Sensor *sensors;
  Alert *alerts;
  Stat *stats;
} Mem_struct;

Mem_struct * shm;
int shmid;

/* Signal Actions */
struct sigaction act; // main

/* Workers */
sem_t *BLOCK_LOGGER, *BLOCK_WORKER, *BLOCK_SHM, *STILL_WORKING, *BLOCK_QUEUE;
pid_t *processes; 

/* Thread Sensor Reader */
pthread_t sensor_reader;
int fd_sensor;

/* Thread Console Reader */
pthread_t console_reader;
int fd_user;

/* Thread Dispatcher */
pthread_t dispatcher;

/* Internal Queue */
typedef struct {
  int type; // 0 is from sensor and 1 from user 
  char content[MAX];
  Command cmd;
} Job;

typedef struct Jobs {
  Job job;
  struct Jobs * next;
} Jobs;

typedef struct {
  int n; // current number of elements
  Jobs *user; // User Console Thread
  Jobs *sensor; // Sensor Reader Thread
} InternalQueue;

InternalQueue * queue;
int mqid; // message queue id

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
  sem_close(BLOCK_QUEUE);
  sem_unlink(MUTEX_QUEUE);

  /* Remove FIFO's */
  unlink(SENSOR_FIFO);
  unlink(USER_FIFO);

  /* Remove Message Queue */
  msgctl(mqid, IPC_RMID, NULL);

  /* Exit */
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
  char send[] = "ERROR >> ";
  strcat(send, content);
  write_log(fp, send);
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

  Job job;
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
    if (job.type == 0) {
      // from sensor
      // do something
    } else {
      // from user
      // do something
    }

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
/*        Queue Functions        */
/* ----------------------------- */
void create_job(Jobs* jobs, Job job) {
  Jobs *new = malloc(sizeof(Jobs));
  if (new == NULL) handle_error_log(fp, "CREATING NEW JOB");
  new->job = job;

  if (jobs->next == NULL) { // empty list - add to the beginning
    jobs->next = new;
    new->next = NULL;
  } else { // non-empty list - add to the end
    Jobs *aux = jobs->next;
    while(aux->next != NULL) aux = aux->next;
    aux->next = new;
    new->next = NULL;
  }
  queue->n++;
}

Job * pop_job(Jobs* jobs) { // get first job and remove it from the queue
  if (jobs->next == NULL) return NULL; // empty list
  Jobs *aux = jobs->next;
  jobs->next = aux->next;
  queue->n--;
  return &(aux->job);
}

/* ----------------------------- */
/*        Thread Functions       */
/* ----------------------------- */
void * sensor_reader_func(void * param) {
  write_log(fp, "THREAD SENSOR_READER CREATED");

  // open pipe
  if ((fd_sensor = open(SENSOR_FIFO, O_RDWR)) < 0) handle_error_log(fp, "open sensor fifo");

  int size;
  Job job;
  job.type = 0;

  fd_set set;
  while(1) {
    FD_ZERO(&set);
    FD_SET(fd_sensor, &set);

    if (select(fd_sensor + 1, &set, NULL, NULL, NULL) < 0) handle_error_log(fp, "select sensor fifo");
    if ((size = read(fd_sensor, job.content, MAX)) < 0) handle_error_log(fp, "read sensor fifo");
    job.content[size] = '\0';

    #ifdef DEBUG
      printf("DEBUG >> Sensor Reader read: %s\n", job.content);
    #endif

    /* Send job to queue */
    sem_wait(BLOCK_QUEUE);
    if (queue->n < QUEUE_SZ) create_job(queue->sensor, job);
    else write_log(fp, "QUEUE IS FULL");
    sem_post(BLOCK_QUEUE);

    // if (buffer[0] == '>') { /* Register Sensor */
    //   Sensor s;
    //   sscanf(buffer, " >%[^#]#%[^#]#%d#%d#%d", s.id, s.key, &s.min, &s.max, &s.inter);

    //   sem_wait(BLOCK_SHM);
    //   if ((i = searchSensor(&shm->sensors, &NULL_SENSOR, MAX_SENSORS, 0)) != -1) {
    //     cpySensor(&shm->sensors[i], &s);
    //     sprintf(str, "CREATED SENSOR {%s, %s, %d, %d, %d}", shm->sensors[i].id, shm->sensors[i].key, shm->sensors[i].min, shm->sensors[i].max, shm->sensors[i].inter);
    //   } else strcpy(str, "SENSOR LIST IS FULL");
    //   sem_post(BLOCK_SHM);
    //   write_log(fp, str);
    // } else { /* Send data to internal queue */
    //   // send to dispatcher (internal queue) the content of buffer so that it can be processed by the workers

    //   Sensor s = NULL_SENSOR;
    //   int val;
    //   sscanf(buffer, " %[^#]#%[^#]#%d", s.id, s.key, &val);

    //   sem_wait(BLOCK_SHM);
    //   if((k = searchKey(&shm->stats, s.key, MAX_KEYS)) == -1) { /* Register Key in stats */
    //     if ((k = searchKey(&shm->stats, "", MAX_KEYS)) != -1) strcpy(shm->stats[k].key, s.key);
    //     else {
    //       strcpy(str, "KEY LIST IS FULL");
    //       sem_post(BLOCK_SHM);
    //       continue;
    //     }
    //   }

    //   if ((i = searchSensor(&shm->sensors, &s, MAX_SENSORS, 1)) != -1) {
    //     //! send to dispatcher (internal queue)
    //     #ifdef DEBUG
    //       printf("DEBUG >> Sending job to dispatcher! 😼\n");
    //     #endif
    //     shm->stats[k].last = val; // and maybe notify alerts_watcher
    //   } // (it's like sensor doesn't exist)
    //   sem_post(BLOCK_SHM);
    // }
  }
}

void * console_reader_func(void * param) {
  write_log(fp, "THREAD CONSOLE_READER CREATED");

  // open pipe
  if ((fd_user = open(USER_FIFO, O_RDWR)) < 0) handle_error_log(fp, "open user fifo");

  Command cmd;
  int size;
  fd_set set;
  Job job;
  job.type = 1;


  while(1) {
    FD_ZERO(&set);
    FD_SET(fd_user, &set);

    if (select(fd_user + 1, &set, NULL, NULL, NULL) < 0) handle_error_log(fp, "select user fifo");
    if ((size = read(fd_user, &cmd, sizeof(Command))) < 0) handle_error_log(fp, "read user fifo");
    job.cmd = cmd;

    /* Send job to queue */
    sem_wait(BLOCK_QUEUE);
    if (queue->n < QUEUE_SZ) create_job(queue->user, job);
    else write_log(fp, "QUEUE IS FULL");
    sem_post(BLOCK_QUEUE);

    // msg.type = cmd.user;
    // if (cmd.command == 1) { // add alert
    //   sem_wait(BLOCK_SHM);
    //   if ((i = searchAlert(&shm->alerts, &NULL_ALERT, MAX_ALERTS, 0)) != -1) {
    //     cpyAlert(&shm->alerts[i], &cmd.alert);
    //     sprintf(msg.response, "CREATED ALERT: {%s, %s, %d, %d}", shm->alerts[i].id, shm->alerts[i].key, shm->alerts[i].min, shm->alerts[i].max);
    //   } else strcpy(msg.response, "NO MORE SPACE FOR NEW ALERTS");
    //   sem_post(BLOCK_SHM);
    //   write_log(fp, msg.response);
    // } else if (cmd.command == 2) { // remove alert
    //   sem_wait(BLOCK_SHM);
    //   if ((searchAlert(&shm->alerts, &cmd.alert, MAX_ALERTS, 1)) != -1) {
    //     cpyAlert(&shm->alerts[i], &NULL_ALERT);
    //     strcpy(msg.response, "OK\n");
    //   } else strcpy(msg.response, "COULD NOT FIND ALERT\n");
    //   sem_post(BLOCK_SHM);
    // } else if (cmd.command == 3) { // list alerts
    //   sprintf(msg.response, "%-32s %-32s %-5s %-5s\n", "ID", "KEY", "MIN", "MAX");
    //   sem_wait(BLOCK_SHM);
    //   for (int j = 0; j < MAX_ALERTS; j++) {
    //     if (!compareAlerts(&shm->alerts[j], &NULL_ALERT)) { // its not null
    //       char holder[MAX];
    //       sprintf(holder, "%-32s %-32s %-5d %-5d\n", shm->alerts[j].id, shm->alerts[j].key, shm->alerts[j].min, shm->alerts[j].max);
    //       strcat(msg.response, holder);
    //     }
    //   }
    //   sem_post(BLOCK_SHM);
    // } else if (cmd.command == 4) { // sensors
    //   strcpy(msg.response, "ID\n");
    //   sem_wait(BLOCK_SHM);
    //   for (int j = 0; j < MAX_SENSORS; j++) {
    //     if (!compareSensors(&shm->sensors[j], &NULL_SENSOR)) {
    //       char holder[STR];
    //       sprintf(holder, "%s\n", shm->sensors[j].id);
    //       strcat(msg.response, holder);
    //     }
    //   }
    //   sem_post(BLOCK_SHM);
    // } else if (cmd.command == 5) { // stats
    //   sem_wait(BLOCK_SHM);
    //   sprintf(msg.response, "%-32s %-5s %-5s %-5s %-5s %-5s\n", "Key", "Last", "Min", "Max", "Avg", "Count");
    //   for (int j = 0; j < MAX_KEYS; j++) {
    //     Key k = shm->stats[j];
    //     if (!(strcmp(k.key, "") == 0)) {
    //       char holder[MAX];
    //       sprintf(holder, "%-32s %-5d %-5d %-5d %-5.1f %-5d\n", k.key, k.last, k.min, k.max, k.avg, k.count);
    //       strcat(msg.response, holder);
    //     }
    //   }
    //   sem_post(BLOCK_SHM);
    // } else if (cmd.command == 6) { // reset
    //   sem_wait(BLOCK_SHM);
    //   for (int j = 0; j < MAX_SENSORS; j++) cpySensor(&shm->sensors[j], &NULL_SENSOR);
    //   for (int j = 0; j < MAX_KEYS; j++) cpyKey(&shm->stats[j], &NULL_KEY);
    //   sem_post(BLOCK_SHM);
    //   strcpy(msg.response, "OK\n");
    // }
  }
}

void * dispatcher_func(void * param) {
  write_log(fp, "THREAD DISPATCHER CREATED");
  Job * job;
  while (1) {
    // get job from queue
    if (job == NULL) {
      sem_wait(BLOCK_QUEUE);
      if (queue->n > 0) {
        job = pop_job(queue->user);
        if (job == NULL) job = pop_job(queue->sensor); // if user queue is empty, get from sensor queue
      }
      sem_post(BLOCK_QUEUE);
    }

    // check if there is a message to send and if there is a worker available
    if (job != NULL && shm->available_workers > 0) {
      //! send job to worker
      job = NULL; // reset send
      #ifdef DEBUG
        printf("DEBUG >> Sending job to worker!\n");
      #endif
      
      //* signal worker to start job
      sem_post(BLOCK_WORKER); // TODO: change to conditional variable
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
  if ((BLOCK_QUEUE = sem_open(MUTEX_QUEUE, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) handle_error("INITIALIZING BLOCK_QUEUE SEMAPHORE"); // block when someone is using the queue

  /* Create FIFO's */
  if (mkfifo(SENSOR_FIFO, 0666) == -1) handle_error("CREATING SENSOR FIFO");
  if (mkfifo(USER_FIFO, 0666) == -1) handle_error("CREATING USER FIFO");

  /* Main */
  write_log(fp, "HOME_IOT SIMULATOR STARTING");

  /* Shared Memory */
  size_t mem_struct_size = sizeof(Mem_struct);
  size_t sensors_size = MAX_SENSORS*sizeof(Sensor);
  size_t alerts_size = MAX_ALERTS*sizeof(Alert);
  size_t stats_size = MAX_KEYS*sizeof(Stat);
  if ((shmid = shmget(IPC_PRIVATE, mem_struct_size + sensors_size + alerts_size + stats_size, 0666 | IPC_CREAT | IPC_EXCL)) == -1) handle_error_log(fp, "CREATING SHARED MEMORY");
  if ((shm = (Mem_struct *) shmat(shmid, NULL, 0)) == (Mem_struct *) -1) handle_error_log(fp, "ATTACHING SHARED MEMORY");

  /* Initialize Shared Memory */
  shm->workers = N_WORKERS;
  shm->available_workers = 0;
  shm->sensors = (Sensor *)((void *)shm + mem_struct_size); 
  shm->alerts = (Alert *)((void *)shm + mem_struct_size + sensors_size);
  shm->stats = (Stat *)((void *) shm + mem_struct_size + sensors_size + alerts_size);
  for (int i = 0; i < MAX_SENSORS; i++) cpySensor(&shm->sensors[i], &NULL_SENSOR);
  for (int i = 0; i < MAX_ALERTS; i++) cpyAlert(&shm->alerts[i], &NULL_ALERT);
  for (int i = 0; i < MAX_KEYS; i++) cpyStat(&shm->stats[i], &NULL_STAT);

  #ifdef DEBUG
    printf("DEBUG >> shmid = %d\n", shmid);
  #endif
  
  /* Internal Queue */
  if ((mqid = msgget(MESSAGE_QUEUE, IPC_CREAT | IPC_EXCL | 0666)) == -1) handle_error_log(fp, "CREATING INTERNAL QUEUE");
  queue->n = 0; // initialize queue

  #ifdef DEBUG
    printf("DEBUG >> mqid = %d\n", mqid);
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
