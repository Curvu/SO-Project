#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/msg.h>
#include <pthread.h>
#include "lib/functions.h"

struct sigaction act;
pthread_t thread;
int fifo, mqid, user_id;
char buffer[MAX];

void cleanup() {
  /* Send SIGINT to thread */
  pthread_kill(thread, SIGINT);
  pthread_join(thread, NULL);

  /* Close FIFO */
  close(fifo);

  /* Exit */
  exit(0);
}

void ctrlc_handler(int signo) {
  if (signo == SIGINT) {
    printf("\nSIGINT RECEIVED\n");
    cleanup();
    exit(0);
  }
}

void message_listener() { // this is a thread
  act.sa_handler = SIG_DFL; // default ctrl+c (not the handler)
  sigaction(SIGINT, &act, NULL);

  Message msg;
  while(1) {
    if (msgrcv(mqid, &msg, sizeof(Message), user_id, 0) == -1) {
      perror("Error reading from Message Queue");
      exit(EXIT_FAILURE);
    }
    printf("%s", msg.response);
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("You must do like this example: ./user_console 32\n");
    exit(EXIT_FAILURE);
  }

  verifyParam(argv[1], &user_id, 1);
  if (user_id <= 0) {
    printf("User id must be greater than 0!\n");
    exit(EXIT_FAILURE);
  }

  // TODO: check if user already exists

  #ifdef DEBUG
    printf("Hello, user %d!\n", user_id);
  #endif

  /* Signal Handler */
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask); // Block all signals during handler
  act.sa_handler = ctrlc_handler;
  sigaction(SIGINT, &act, NULL);

  /* Create Thread to listen to Message Queue */
  if (pthread_create(&thread, NULL, (void *) message_listener, NULL) != 0) {
    perror("Error creating thread");
    exit(EXIT_FAILURE);
  }

  /* Open FIFO */
  if ((fifo = open(USER_FIFO, O_WRONLY)) == -1) {
    perror("Error opening FIFO");
    exit(EXIT_FAILURE);
  }

  /* Open Message Queue */
  if ((mqid = msgget(MESSAGE_QUEUE_KEY, 0)) == -1) {
    perror("Error opening Message Queue");
    exit(EXIT_FAILURE);
  }

  /* Main */
  int val;
  char command[MAX];
  Command cmd;
  cmd.user = user_id;
  cmd.command = 0;

  while(1) {
    scanf(" %s", command);
    if (strcmp(command, "add_alert") == 0) { // <id> <key> <min> <max>
      val = scanf(" %[^ ] %[^ ] %d %d", cmd.alert.id, cmd.alert.key, &cmd.alert.min, &cmd.alert.max);
      if (val == 0 || val == EOF) {
        printf(">> Invalid command!\n");
        continue;
      }
      if (verifyID(cmd.alert.id) && verifyKey(cmd.alert.key)) cmd.command = 1;
      else printf(">> Some invalid Parameter!!\n");
    } else if (strcmp(command, "remove_alert") == 0) { // <id>
      val = scanf(" %[^\n]", cmd.alert.id);
      if (val == 0 || val == EOF) {
        printf(">> Invalid command!\n");
        continue;
      }
      if (verifyID(cmd.alert.id)) cmd.command = 2;
      else printf(">> Invalid ID!!\n");
    } else if (strcmp(command, "list_alerts") == 0) cmd.command = 3;
    else if (strcmp(command, "sensors") == 0) cmd.command = 4;
    else if (strcmp(command, "stats") == 0) cmd.command = 5;
    else if (strcmp(command, "reset") == 0) cmd.command = 6;
    else if (strcmp(command, "exit") == 0) {
      printf(">> Bye bye!\n");
      break;
    } else printf(">> Invalid command\n");
    if (cmd.command > 0) {
      cmd.alert.user = user_id;
      if (write(fifo, &cmd, sizeof(Command)) == -1) {
        perror("Error writing to FIFO");
        exit(EXIT_FAILURE);
      }

      cmd.command = 0;
      cmd.alert = NULL_ALERT;
    }
  }

  cleanup();
  return 0;
}
