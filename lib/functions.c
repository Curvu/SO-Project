#include "functions.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int verifyLength(char *str) {
  int length = strlen(str);
  return length >= 3 && length <= 32;
}

int verifyID(char *id) {
  if (!verifyLength(id)) return 0;
  int length = strlen(id);
  for (int i = 0; i < length; i++) if (!isAlpha(id[i])) return 0;
  return 1;
}

int verifyKey(char *key) {
  if (!verifyLength(key)) return 0;
  int length = strlen(key);
  for (int i = 0; i < length; i++) if (!isAlpha(key[i]) && !(key[i] != '_')) return 0;
  return 1;
}

void verifyParam(char *param, void *var, int type) {
  int toggle; 
  if (type) toggle = sscanf(param, " %d", (int*) var); // int
  else toggle = sscanf(param, " %s", (char*) var); // string
  
  if (toggle == 0 || toggle == EOF) {
    printf("Invalid parameter: %s\n", param);
    exit(0);
  }
}

void get_hour(char * hour) {
	time_t sec = time(NULL);
	struct tm *t = localtime(&sec);
  sprintf(hour, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
}

void write_log(char * content) {
  char hour[7], buffer[MAX];
  get_hour(hour);
  sprintf(buffer, "%s %s", hour, content);

  FILE *fp = fopen(LOG_FILE, "a+");
  if (fp != NULL) {
    fprintf(fp, "%s\n", buffer);
    printf("%s\n", buffer);
  } else printf("COULD NOT OPEN LOG FILE!\n");
  fclose(fp);
}
