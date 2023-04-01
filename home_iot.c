#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>


typedef struct {
	//TODO Não faço ideia do que por aqui
} mem_struct;

// Semaphore for workers
sem_t sem_workers;

pthread_t console_reader, sensor_reader, dispatcher;
int shmid, *write_pos, *read_pos, *buf;
// pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
mem_struct* mem;

// char *get_hour(){
//     char *hour = malloc(8);
//     time_t t = time(NULL);
//     struct tm *timestruct = localtime(&t);
//     sprintf(hour, "%d:%d:%d", timestruct->tm_hour, timestruct->tm_min, timestruct->tm_sec);
//     return(hour);
// }
// De outra maneira (mais simples)
char *get_hour(){
    char *hour = malloc(8);
    time_t t = time(NULL);
    struct tm *timestruct = localtime(&t);
    return(memcpy(hour, &asctime(timestruct)[11], 8));
}

void worker(int num){
    //TODO Fazer o worker e sincronizar com semáforos
    printf("%s WORKER %d READY\n", get_hour(), num);
}

void* dispatcher_func(void* param){
    int N_WORKERS = *((int*) param);
    for (int i = 0; i < N_WORKERS; ++i){
        if (fork() == 0){
            worker(i+1);
            exit(0);
        }
    }
    for (int i = 0; i < N_WORKERS; ++i){
        wait(NULL);
    }
    pthread_exit(NULL);
    return NULL;
}


int main(int argc, char *argv[]){
    // Verificação de argumentos
    if (argc != 2){
        printf("$home_iot {config file}\n");
        return 1;
    }

    printf("%s HOME_IOT SIMULATOR STARTING\n", get_hour());

    // Handling do ficheiros
    FILE *cfg = fopen(argv[1], "r");
    if (cfg == NULL){
        printf("%s ERROR OPENING CONFIG FILE\n", get_hour());
        return 1;
    }
    // FILE *log = freopen("log.txt", "w", stdout);
    // //Encontrei essa função na net, escreve no ficheiro tudo que é escrito no terminal \o/
    // if (log == NULL){
    //     printf("%s ERROR OPENING LOG FILE\n", get_hour());
    //     return 1;
    // }

    // Ler config
    int QUEUE_SZ, N_WORKERS, MAX_KEYS, MAX_SENSORS, MAX_ALERTS;
    fscanf(cfg, "%d\n%d\n%d\n%d\n%d", &QUEUE_SZ, &N_WORKERS, &MAX_KEYS, &MAX_SENSORS, &MAX_ALERTS);
    if (QUEUE_SZ < 1 || N_WORKERS < 1 || MAX_KEYS < 1 || MAX_SENSORS < 1 || MAX_ALERTS < 0){
        printf("%s ERROR IN CONFIG FILE\n", get_hour());
        return 1;
    }

    // Criação da memória partilhada
    // shmid = shmget(IPC_PRIVATE, sizeof(mem_struct), IPC_CREAT | 0700);
    // if (shmid == -1){
    //     printf("%s ERROR CREATING SHARED MEMORY\n", get_hour());
    //     return 1;
    // }
    mem = (mem_struct*) shmat(shmid, NULL, 0);
    if (mem == NULL){
        printf("%s ERROR ATTACHING SHARED MEMORY\n", get_hour());
        return 1;
    }

    // Criação do processo worker
    if (fork() == 0){
        //TODO
    }
    // Criação do proceso Alerts Watcher
    if (fork() == 0){
        //TODO
    }

    // Criação das threads
    // TODO Ainda não possuimos as funções das threads
    // if (pthread_create(&console_reader, NULL, console_reader, NULL) != 0){
    //     printf("Error creating console reader thread\n");
    //     return 1;
    // }
    // if (pthread_create(&sensor_reader, NULL, sensor_reader, NULL) != 0){
    //     printf("Error creating sensor reader thread\n");
    //     return 1;
    // }
    if (pthread_create(&dispatcher, NULL, dispatcher_func, &N_WORKERS) != 0){
        printf("Error creating dispatcher thread\n");
        return 1;
    }

    // Criação do semáforo
    // if (sem_init(&sem_workers, 1, N_WORKERS) != 0){
    //     printf("Error creating semaphore\n");
    //     return 1;
    // }
    // Espera pelas threads
    // pthread_join(console_reader, NULL);
    // pthread_join(sensor_reader, NULL);
    pthread_join(dispatcher, NULL);

    
}