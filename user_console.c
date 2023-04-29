#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "lib/functions.h"

struct sigaction act;
int fifo;
char buffer[MAX];

void cleanup() {
  /* Close FIFO */
  close(fifo);
  exit(0);
}

void ctrlc_handler(int signo) {
  printf("\nSIGINT RECEIVED\n");
  cleanup();
  exit(0);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("You must do like this example: ./user_console 32\n");
    exit(EXIT_FAILURE);
  }

  int user_id;
  verifyParam(argv[1], &user_id, 1);
  if (user_id <= 0) {
    printf("User id must be greater than 0!\n");
    exit(EXIT_FAILURE);
  }

  // TODO: check if user already exists

  #ifdef DEBUG
    printf("Hello, user %d!\n", user_id);
  #endif

  Command cmd;
  cmd.user = user_id;
  cmd.command = 0;

  /* Signal Handler */
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask); // Block all signals during handler
  act.sa_handler = ctrlc_handler;
  sigaction(SIGINT, &act, NULL);

  /* Open FIFO */
  if ((fifo = open(USER_FIFO, O_WRONLY)) == -1) {
    perror("Error opening FIFO");
    exit(EXIT_FAILURE);
  }

  /* Main */
  char command[MAX];
  int val;
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
      val = scanf(" %[^ ]", cmd.alert.id);
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
      printf("> Bye bye!\n");
      break;
    } else printf("> Invalid command\n");
    if (cmd.command > 0) {
      cmd.alert.user = user_id;
      if (write(fifo, &cmd, sizeof(Command)) == -1) {
        perror("Error writing to FIFO");
        exit(EXIT_FAILURE);
      }
      cmd.command = 0;
      cpyAlert(&cmd.alert, &NULL_ALERT);
    }
  }

  cleanup();
  return 0;
}
