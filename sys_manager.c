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
#define MUTEX_LOGGER "/mutex_logger"
#define MUTEX_ALERT "/mutex_alert"
int QUEUE_SZ, N_WORKERS, MAX_KEYS, MAX_SENSORS, MAX_ALERTS;
FILE *fp;

/* Shared Memory */
typedef struct {
  int *workers;
  Sensor *sensors;
  Alert *alerts;
  Stat *stats;
} Mem_struct;

Mem_struct * shm;
int shmid, mqid;

/* Signal Actions */
struct sigaction act; // main

/* Workers */
pid_t *processes;
int **pipes;

/* Mutexes and Semaphores and Condition Variables */
pthread_mutex_t BLOCK_QUEUE = PTHREAD_MUTEX_INITIALIZER;
sem_t *BLOCK_LOGGER, *BLOCK_SHM, *WAIT_ALERT;

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
InternalQueue queue;


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

  /* Print all jobs left in the queue */
  if (queue.n > 0) {
    char buffer[MAX];
    int i = 0;
    for (Jobs *aux = queue.sensor; aux; aux = aux->next) i++;
    sprintf(buffer, "%d TASKS FOR SENSORS WERE NOT EXECUTED", i);
    write_log(fp, buffer);

    i = 0;
    for (Jobs *aux = queue.user; aux; aux = aux->next) i++;
    sprintf(buffer, "%d TASKS FOR USERS WERE NOT EXECUTED", i);
    write_log(fp, buffer);
  }

  /* Kill all processes */
  for (int i = 0; i < N_WORKERS + 1; i++) if (processes[i] > 0) kill(processes[i], SIGUSR2);
  write_log(fp, "HOME_IOT SIMULATOR WAITING FOR LAST TASKS TO FINISH");
  for (int i = 0; i < N_WORKERS + 1; i++) wait(NULL);

  #ifdef DEBUG
    printf("DEBUG >> All threads and processes deleted!\n");
  #endif

  /* Remove shared memory */
  if (shmid > 0) {
    shmctl(shmid, IPC_RMID, NULL); // remove the shared memory
    shmdt(shm); // dettach from the shared memory
  }

  /* Close log file */
  if (fp) {
    write_log(fp, "HOME_IOT SIMULATOR CLOSING");
    fclose(fp);
  }

  /* Remove Semaphores and Mutexes and Condition Variables */
  sem_close(BLOCK_LOGGER);
  sem_unlink(MUTEX_LOGGER);
  sem_close(BLOCK_SHM);
  sem_unlink(MUTEX_SHM);
  pthread_mutex_destroy(&BLOCK_QUEUE);
  sem_close(WAIT_ALERT);
  sem_unlink(MUTEX_ALERT);

  /* Remove FIFO's */
  unlink(SENSOR_FIFO);
  unlink(USER_FIFO);

  /* Remove Message Queue */
  msgctl(mqid, IPC_RMID, NULL);

  /* Close all unnamed pipes */
  for (int i = 0; i < N_WORKERS; i++) {
    close(pipes[i][0]);
    close(pipes[i][1]);
  }

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
      printf("DEBUG >> %d workers available 🧌\n", sum_array(shm->workers, N_WORKERS)); fflush(stdout);
    #endif
  }
}

void sigusr1_handler(int sig) { // Sginal to close threads
  if (sig == SIGUSR1) {
    close(fd_sensor);
    close(fd_user);
    #ifdef DEBUG
      printf("DEBUG >> SIGUSR1 RECEIVED\n"); fflush(stdout);
    #endif
    pthread_exit(NULL);
  }
}

void sigusr2_handler(int sig) { // Signal to close processes
  if (sig == SIGUSR2) {
    #ifdef DEBUG
      printf("DEBUG >> SIGUSR2 RECEIVED\n"); fflush(stdout);
    #endif
    exit(0);
  }
}


/* ----------------------------- */
/*       Process Functions       */
/* ----------------------------- */
void worker(int num) {
  char buffer[MAX], log[MAX];
  sprintf(buffer, "WORKER %d READY", num);

  Job job;
  Message msg; // message to send to user (message queue)

  int i, fd = pipes[num-1][0];
  fd_set set;
  while(1) {
    // set signal handler for sigusr2
    act.sa_handler = sigusr2_handler;
    sigaction(SIGUSR2, &act, NULL);

    // Worker is ready to receive job
    write_log(fp, buffer);
    shm->workers[num-1] = 1; // worker is available

    // wait for job
    FD_ZERO(&set);
    FD_SET(fd, &set);
    select(fd+1, &set, NULL, NULL, NULL);
    // block signal handler
    act.sa_handler = SIG_IGN;
    sigaction(SIGUSR2, &act, NULL);
    if (read(pipes[num-1][0], &job, sizeof(Job)) < 0) handle_error_log(fp, "READING FROM PIPE");

    //* start job
    if (job.type) { /* Job from user */
      Command c = job.cmd;
      msg.type = c.user;
      if (c.command == 1) { /* Add Alert */
        sem_wait(BLOCK_SHM);
        if ((i = searchAlert(shm->alerts, NULL_ALERT, MAX_ALERTS, 0)) != -1) {
          shm->alerts[i] = c.alert;
          strcpy(msg.response, "OK\n");
        } else strcpy(msg.response, "ERROR\n");
        sem_post(BLOCK_SHM);
        sprintf(log, "WORKER%d: ADD ALERT %s (%s %d TO %d) PROCESSING COMPLETED", num, c.alert.id, c.alert.key, c.alert.min, c.alert.max);
      } else if (c.command == 2) { /* Remove Alert */
        sem_wait(BLOCK_SHM);
        if ((i = searchAlert(shm->alerts, c.alert, MAX_ALERTS, 1)) != -1) { // found alert
          shm->alerts[i] = NULL_ALERT;
          strcpy(msg.response, "OK\n");
        } else strcpy(msg.response, "ERROR\n");
        sem_post(BLOCK_SHM);
        sprintf(log, "WORKER%d: REMOVE ALERT %s PROCESSING COMPLETED", num, c.alert.id);
      } else if (c.command == 3) { /* List Alerts */
        sprintf(msg.response, "%-32s %-32s %-5s %-5s\n", "ID", "KEY", "MIN", "MAX");
        sem_wait(BLOCK_SHM);
        for (int j = 0; j < MAX_ALERTS; j++) {
          if (!compareAlerts(shm->alerts[j], NULL_ALERT)) { // its not null
            char holder[MAX];
            sprintf(holder, "%-32s %-32s %-5d %-5d\n", shm->alerts[j].id, shm->alerts[j].key, shm->alerts[j].min, shm->alerts[j].max);
            strcat(msg.response, holder);
          }
        }
        sem_post(BLOCK_SHM);
        sprintf(log, "WORKER%d: LIST ALERTS PROCESSING COMPLETED", num);
      } else if (c.command == 4) { /* List Sensors */
        strcpy(msg.response, "ID\n");
        sem_wait(BLOCK_SHM);
        for (int j = 0; j < MAX_SENSORS; j++) {
          if (!compareSensors(shm->sensors[j], NULL_SENSOR)) { // its not null
            char holder[STR+1];
            sprintf(holder, "%s\n", shm->sensors[j].id);
            strcat(msg.response, holder);
          }
        }
        sem_post(BLOCK_SHM);
        sprintf(log, "WORKER%d: LIST ACTIVE SENSORS PROCESSING COMPLETED", num);
      } else if (c.command == 5) { /* List Stats */
        sprintf(msg.response, "%-32s %-5s %-5s %-5s %-5s %-5s\n", "Key", "Last", "Min", "Max", "Avg", "Count");
        sem_wait(BLOCK_SHM);
        for (int j = 0; j < MAX_KEYS; j++) {
          Stat stat = shm->stats[j];
          if (!(strcmp(stat.key, "") == 0)) { // its not null (empty string)
            char holder[MAX];
            sprintf(holder, "%-32s %-5d %-5d %-5d %-5.1f %-5d\n", stat.key, stat.last, stat.min, stat.max, stat.avg, stat.count);
            strcat(msg.response, holder);
          }
        }
        sem_post(BLOCK_SHM);
        sprintf(log, "WORKER%d: LIST STATS PROCESSING COMPLETED", num);
      } else if (c.command == 6) { /* Reset */
        sem_wait(BLOCK_SHM);
        for (int j = 0; j < MAX_SENSORS; j++) shm->sensors[j] = NULL_SENSOR;
        for (int j = 0; j < MAX_KEYS; j++) shm->stats[j] = NULL_STAT;
        sem_post(BLOCK_SHM);
        strcpy(msg.response, "OK\n");
        sprintf(log, "WORKER%d: RESET PROCESSING COMPLETED", num);
      }

      /* Send response to user */
      if (msgsnd(mqid, &msg, sizeof(Message) - sizeof(long), 0) < 0) handle_error_log(fp, "SENDING MESSAGE TO USER");
    } else { /* Job from sensor */
      Sensor s;
      int val;
      sscanf(job.content, " %[^#]#%[^#]#%d", s.id, s.key, &val);
      sem_wait(BLOCK_SHM);
      /* Sensor doesn't exist && there is space for new sensor */
      if (((i = searchSensor(shm->sensors, s, MAX_SENSORS)) == -1) && ((i = searchSensor(shm->sensors, NULL_SENSOR, MAX_SENSORS)) != -1)) shm->sensors[i] = s; // create sensor
      if (i != -1) { // sensor exist
        if ((i = searchStat(shm->stats, s.key, MAX_KEYS)) != -1) { /* Key exist */
          Stat *stat = &shm->stats[i];
          stat->avg = (stat->avg * stat->count + val) / (stat->count + 1); // update average
          stat->count++;
          stat->last = val;
          if (val < stat->min) stat->min = val;
          if (val > stat->max) stat->max = val;
        } else if ((i = searchStat(shm->stats, "", MAX_KEYS)) != -1) { /* Find empty key */
          strcpy(shm->stats[i].key, s.key); // create key
          Stat *stat = &shm->stats[i];
          stat->avg = val;
          stat->count = 1;
          stat->last = val;
          stat->max = val;
          stat->min = val;
        } // ignore if key list is full
      }
      sem_post(BLOCK_SHM);
      sem_post(WAIT_ALERT); // alert watcher to look for alerts
      sprintf(log, "WORKER%d: %s DATA PROCESSING COMPLETED", num, s.key);
    }
    write_log(fp, log);
  }
}

void alert_watcher() {
  write_log(fp, "ALERT WATCHER READY");
  int i, k;
  Message msg;

  act.sa_handler = sigusr2_handler;
  sigaction(SIGUSR2, &act, NULL);

  while(1) {
    // go through alerts check min max and compare with stats
    sem_wait(WAIT_ALERT);
    for (i = 0; i < MAX_ALERTS; i++) {
      if (!compareAlerts(shm->alerts[i], NULL_ALERT)) {
        Alert a = shm->alerts[i];
        if ((k = searchStat(shm->stats, a.key, MAX_KEYS)) != -1) {
          Stat stat = shm->stats[k];
          if (stat.last < a.min || stat.last > a.max) { // alert triggered
            // send message to user
            msg.type = a.user;
            sprintf(msg.response, "ALERT %s (%s %d TO %d) TRIGGERED\n", a.id, a.key, a.min, a.max);
            if (msgsnd(mqid, &msg, sizeof(Message) - sizeof(long), 0) < 0) handle_error_log(fp, "SENDING MESSAGE TO USER");
            write_log(fp, msg.response);
          }
        }
      }
    }
  }
}


/* ----------------------------- */
/*        Queue Functions        */
/* ----------------------------- */
Jobs * create_job(Jobs* jobs, Job job) {
  Jobs *new = malloc(sizeof(Jobs));
  if (new == NULL) handle_error_log(fp, "CREATING NEW JOB");
  new->job = job;

  if (jobs == NULL) { // empty list - add to the beginning
    new->next = NULL;
    printf("DEBUG >> Job created\n");
  } else { // non-empty list - add to the end
    Jobs *aux = jobs;
    while(aux->next != NULL) aux = aux->next;
    aux->next = new;
    new->next = NULL;
  }
  queue.n++;
  return new;
}

void remove_job(Jobs **jobs) {
  if (*jobs == NULL) return; // empty list
  Jobs *aux = *jobs;
  *jobs = aux->next;
  free(aux); // free memory of the old head node
  queue.n--;
}


/* ----------------------------- */
/*        Thread Functions       */
/* ----------------------------- */
void * sensor_reader_func(void * param) {
  write_log(fp, "THREAD SENSOR_READER CREATED");
  /* open pipe */
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
      printf("DEBUG >> Sensor Reader: %s\n", job.content);
    #endif

    /* Send job to queue */
    pthread_mutex_lock(&BLOCK_QUEUE);
    if (queue.n < QUEUE_SZ) queue.sensor = create_job(queue.sensor, job);
    else write_log(fp, "QUEUE IS FULL");
    pthread_mutex_unlock(&BLOCK_QUEUE);
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
    pthread_mutex_lock(&BLOCK_QUEUE);
    if (queue.n < QUEUE_SZ) queue.user = create_job(queue.user, job);
    else write_log(fp, "QUEUE IS FULL");
    pthread_mutex_unlock(&BLOCK_QUEUE);
  }
}

void * dispatcher_func(void * param) {
  write_log(fp, "THREAD DISPATCHER CREATED");
  Job job;
  job.type = -1;
  char log[MAX];

  while (1) {
    // get job from queue
    if (job.type == -1) {
      pthread_mutex_lock(&BLOCK_QUEUE);
      if (queue.n > 0) {
        if (queue.user != NULL) {
          job = queue.user->job;
          remove_job(&queue.user);
        } else if (queue.sensor != NULL) {
          job = queue.sensor->job;
          remove_job(&queue.sensor);
        }
      }
      pthread_mutex_unlock(&BLOCK_QUEUE);
    }

    // check if there is a message to send and if there is a worker available
    if (job.type != -1 && sum_array(shm->workers, N_WORKERS) > 0) {
      // find the available worker
      int i;
      for (i = 0; i < N_WORKERS; i++) if (shm->workers[i]) break;
      shm->workers[i] = 0; // set worker to busy
      
      // send job to worker - pipe
      #ifdef DEBUG
        printf("DEBUG >> Sending job to worker %d!\n", i+1);
      #endif

      
      if (job.type) { /* From user */
        int c = job.cmd.command;
        Alert a = job.cmd.alert;
        if (c == 1) sprintf(log, "DISPATCHER: ADD ALERT %s (%s %d TO %d) SENT FOR PROCESSING ON WORKER %d", a.id, a.key, a.min, a.max, i+1);
        if (c == 2) sprintf(log, "DISPATCHER: REMOVE ALERT %s SENT FOR PROCESSING ON WORKER %d", a.id, i+1);
        if (c == 3) sprintf(log, "DISPATCHER: LIST ALERTS SENT FOR PROCESSING ON WORKER %d", i+1);
        if (c == 4) sprintf(log, "DISPATCHER: LIST ACTIVE SENSORS SENT FOR PROCESSING ON WORKER %d", i+1);
        if (c == 5) sprintf(log, "DISPATCHER: LIST STATS SENT FOR PROCESSING ON WORKER %d", i+1);
        if (c == 6) sprintf(log, "DISPATCHER: RESET STATS SENT FOR PROCESSING ON WORKER %d", i+1);
      } else { /* From sensor */
        char id[STR], key[STR];
        sscanf(job.content, " %[^#]#%[^#]#%*d", id, key);
        sprintf(log, "DISPATCHER: %s DATA (FROM %s SENSOR) SENT FOR PROCESSING ON WORKER %d", key, id, i+1);
      }
      write_log(fp, log);

      write(pipes[i][1], &job, sizeof(Job));
      job.type = -1;
    }
  }
}


/* ----------------------------- */
/*             Main              */
/* ----------------------------- */
int main(int argc, char **argv) {
  /* Handling arguments */
  if (argc != 2) handle_error("INVALID NUMBER OF ARGUMENTS");
  if ((fp = fopen(LOG_FILE, "a+")) == NULL) handle_error("OPENING LOG FILE");

  /* Handling file */
  FILE *cfg = fopen(argv[1], "r");
  if (cfg == NULL) handle_error("OPENING CONFIG FILE");

  /* Read Config */
  if (fscanf(cfg, "%d\n%d\n%d\n%d\n%d", &QUEUE_SZ, &N_WORKERS, &MAX_KEYS, &MAX_SENSORS, &MAX_ALERTS) != 5) handle_error("INVALID CONFIG FILE");
  if (QUEUE_SZ < 1 || N_WORKERS < 1 || MAX_KEYS < 1 || MAX_SENSORS < 1 || MAX_ALERTS < 0) handle_error("INVALID ARGUMENT IN CONFIG FILE");
  fclose(cfg);

  /* Signal (block all signals during setup) */
  act.sa_flags = 0;
  sigfillset(&act.sa_mask);
  sigdelset(&act.sa_mask, SIGINT);
  sigdelset(&act.sa_mask, SIGTSTP);
  sigdelset(&act.sa_mask, SIGUSR1);
  sigdelset(&act.sa_mask, SIGUSR2);
  sigprocmask(SIG_SETMASK, &act.sa_mask, NULL); // this will block all signals

  /* Signal Handlers - for now both are blocked */
  act.sa_handler = SIG_IGN;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTSTP, &act, NULL);
  sigaction(SIGUSR1, &act, NULL); // to close threads
  sigaction(SIGUSR2, &act, NULL); // to close threads

  /* Semaphores */
  if ((BLOCK_SHM = sem_open(MUTEX_SHM, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) handle_error("INITIALIZING BLOCK_SHM SEMAPHORE");          // block when someone is using the shared memory
  if ((BLOCK_LOGGER = sem_open(MUTEX_LOGGER, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) handle_error("INITIALIZING BLOCK_LOGGER SEMAPHORE"); // block when someone is using the log file
  if ((WAIT_ALERT = sem_open(MUTEX_ALERT, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) handle_error("INITIALIZING WAIT_ALERT SEMAPHORE");       // block when someone is using the alerts array

  /* Create FIFO's */
  if (mkfifo(SENSOR_FIFO, 0666) == -1) handle_error("CREATING SENSOR FIFO");
  if (mkfifo(USER_FIFO, 0666) == -1) handle_error("CREATING USER FIFO");

  /* Main */
  write_log(fp, "HOME_IOT SIMULATOR STARTING");

  /* Shared Memory */
  size_t mem_struct_size = sizeof(Mem_struct);
  size_t workers_size = N_WORKERS*sizeof(int);
  size_t sensors_size = MAX_SENSORS*sizeof(Sensor);
  size_t alerts_size = MAX_ALERTS*sizeof(Alert);
  size_t stats_size = MAX_KEYS*sizeof(Stat);
  if ((shmid = shmget(IPC_PRIVATE, mem_struct_size + workers_size + sensors_size + alerts_size + stats_size, 0666 | IPC_CREAT | IPC_EXCL)) == -1) handle_error_log(fp, "CREATING SHARED MEMORY");
  if ((shm = (Mem_struct *) shmat(shmid, NULL, 0)) == (Mem_struct *) -1) handle_error_log(fp, "ATTACHING SHARED MEMORY");

  /* Initialize Shared Memory */
  shm->workers = (int *)((void *)shm + mem_struct_size); // workers are the first thing in the shared memory (after the struct
  shm->sensors = (Sensor *)((void *)shm + workers_size + mem_struct_size); 
  shm->alerts = (Alert *)((void *)shm + workers_size + mem_struct_size + sensors_size);
  shm->stats = (Stat *)((void *) shm + workers_size + mem_struct_size + sensors_size + alerts_size);
  for (int i = 0; i < N_WORKERS; i++) shm->workers[i] = 0; // initialize workers (1 = available, 0 not available)
  for (int i = 0; i < MAX_SENSORS; i++) shm->sensors[i] = NULL_SENSOR;
  for (int i = 0; i < MAX_ALERTS; i++) shm->alerts[i] = NULL_ALERT;
  for (int i = 0; i < MAX_KEYS; i++) shm->stats[i] = NULL_STAT;

  #ifdef DEBUG
    printf("DEBUG >> shmid = %d\n", shmid);
  #endif
  
  /* Message Queue */
  if ((mqid = msgget(MESSAGE_QUEUE_KEY, IPC_CREAT | IPC_EXCL | 0666)) == -1) handle_error_log(fp, "CREATING MESSAGE QUEUE");

  /* Initialize Queue */
  queue.n = 0;

  #ifdef DEBUG
    printf("DEBUG >> mqid = %d\n", mqid);
  #endif

  /* Unnamed Pipes */
  pipes = (int **) malloc(N_WORKERS * sizeof(int *));
  for (int i = 0; i < N_WORKERS; i++) {
    if ((pipes[i] = (int *) malloc(2 * sizeof(int))) == NULL) handle_error_log(fp, "ALLOCATING MEMORY PIPES ARRAY");
    if (pipe(pipes[i]) == -1) handle_error_log(fp, "CREATING PIPE");
  }

  printf("HOME_IOT SIMULATOR STARTING\n");

  /* Sensor Reader, Console Reader and Dispatcher */
  if (pthread_create(&sensor_reader, NULL, sensor_reader_func, NULL) != 0) handle_error_log(fp, "CREATING SENSOR_READER");
  if (pthread_create(&console_reader, NULL, console_reader_func, NULL) != 0) handle_error_log(fp, "CREATING CONSOLE_READER");
  if (pthread_create(&dispatcher, NULL, dispatcher_func, NULL) != 0) handle_error_log(fp, "CREATING DISPATCHER");

  /* Processes */
  if ((processes = (pid_t *) malloc((N_WORKERS + 1) * sizeof(pid_t))) == NULL) handle_error_log(fp, "ALLOCATING MEMORY PROCESS ARRAY");
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
