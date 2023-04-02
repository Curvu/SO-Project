#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/functions.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("You must do like this example: ./user_console 32\n");
    exit(0);
  }

  int user;
  verifyParam(argv[1], &user, 1);

  #ifdef DEBUG
    printf("Hello, user %d!\n", user);
  #endif

  // TODO: check if user already exists


  char command[MAX], id[STR], key[STR];
  int min, max;
  while(1) {
    scanf(" %s", command);

    if (strcmp(command, "add_alert") == 0) {
      scanf(" %[^ ] %[^ ] %d %d", id, key, &min, &max);
      if (!(verifyID(id) && verifyKey(key))) {
        printf(">> Some invalid Parameter!!\n");
        continue;
      }

      #ifdef DEBUG
        printf("id: %s, key: %s, min: %d, max: %d \n", id, key, min, max);
      #endif

      // TODO: check if still have space for a new alert, and add it (dont understand this because [id] [key])
    } else if (strcmp(command, "remove_alert") == 0) {
      scanf(" %[^ ]", id);

      #ifdef DEBUG
        printf("id: %s \n", id);
      #endif

      // TODO: check if id is valid, remove alert
    } else if (strcmp(command, "list_alerts") == 0) {
      printf("this is the alert list.\n");
      // TODO: list alerts
    } else if (strcmp(command, "sensors") == 0) {
      printf("this is the list of sensors\n");
      // TODO: list sensors
    } else if (strcmp(command, "stats") == 0) {
      printf("list of stats\n");
      // TODO: list stats
    } else if (strcmp(command, "reset") == 0) {
      printf("restting test\n");
      // TODO: reset stats
    } else if (strcmp(command, "exit") == 0) {
      printf("bye bye!\n");
      // TODO: exit
      break;
    } else printf("Invalid command\n");
  }

  // TODO: free everything

  return 0;
}
