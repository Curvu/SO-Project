/**
 * @file user_console.c
 * @authors
 * - Filipe Rodrigues (2021218054)
 * - Joás Silva (2021226149)
 */

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
char buffer[MAX], command[STR];
Command cmd;

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

void message_listener() { // this is a thread that will listen to the message queue
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
  cmd.user = user_id;
  cmd.command = 0;

  printf(">> Commands:\n");
  printf("> add_alert <id> <key> <min> <max>\n");
  printf("> remove_alert <id>\n");
  printf("> list_alerts\n");
  printf("> sensors\n");
  printf("> stats\n");
  printf("> reset\n");
  printf("> exit\n");

  while(1) {
    if (scanf(" %[^\n]", buffer) == EOF) {
      printf(">> Invalid command!\n");
      continue;
    }
    sscanf(buffer, " %s ", command);

    if (strcmp(command, "add_alert") == 0) { // <id> <key> <min> <max>
      if (sscanf(buffer, " %*s %s %s %d %d", cmd.alert.id, cmd.alert.key, &cmd.alert.min, &cmd.alert.max) != 4) {
        printf(">> Missing parameters!\n");
        continue;
      }
      if (verifyID(cmd.alert.id) && verifyKey(cmd.alert.key) && cmd.alert.min < cmd.alert.max) cmd.command = 1;
      else printf(">> Some invalid Parameter!!\n");
    } else if (strcmp(command, "remove_alert") == 0) { // <id>
      if (sscanf(buffer, " %*s %s", cmd.alert.id) != 1) {
        printf(">> Missing parameter!\n");
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
