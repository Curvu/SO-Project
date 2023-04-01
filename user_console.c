#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/functions.h"

// add_alert [id] [chave] [min] [max] > Adiciona uma nova regra de alerta ao sistema
// remove_alert [id] > Remove uma regra de alerta do sistema
// list_alerts > Lista todas as regras de alerta que existem no sistema 
// sensors > Lista todos os s ensors que enviaram dados ao sistema
// stats > Apresenta estatísticas referentes aos dados enviados pelos sensores
// reset > Limpa todas as estatísticas calculadas até ao momento pelo sistema (todos os sensores, criados por qualquer utilizador)
// exit > fecha o programa

#define MAX 200

int main(int argc, char **argv) {
  int user;
  if (argc != 2 || ((user = (int) strtol(argv[1], NULL, 10)) == 0 && argv[1][0] != '0')) {
    printf("You must provide an ID, e.g. ./user_console 32\n");
    return 1;
  }

  // TODO: check if user exists

  printf("Hello, user %d!\n", user);

  while(1) {
    char line[MAX];
    fgets(line, 100, stdin);
    line[strcspn(line, "\n")] = '\0'; // remove \n character
    if (line[0] == '\0') continue; // ignora linhas vazias (se houver)


    char *command = strtok(line, " "); // this is the command

    if (strcmp(command, "add_alert") == 0) {
      char *id = strtok(NULL, " ");
      char *key = strtok(NULL, " ");
      int min = (int) strtol(strtok(NULL, " "), NULL, 10);
      int max = (int) strtol(strtok(NULL, " "), NULL, 10);
      printf("id: %s, key: %s, min: %d, max: %d \n", id, key, min, max);

      // TODO: check if id and key are valid, add alert

    } else if (strcmp(command, "remove_alert") == 0) {
      char *id = strtok(NULL, " ");
      printf("id: %s \n", id);

      // TODO: check if id is valid, remove alert

    } else if (strcmp(command, "list_alerts") == 0) {
      // TODO: list alerts
    } else if (strcmp(command, "sensors") == 0) {
      // TODO: list sensors
    } else if (strcmp(command, "stats") == 0) {
      // TODO: list stats
    } else if (strcmp(command, "reset") == 0) {
      // TODO: reset stats
    } else if (strcmp(command, "exit") == 0) {
      // TODO: exit
      break;
    } else {
      printf("Invalid command\n");
    }
  }

  // TODO: free everything

  return 0;
}
