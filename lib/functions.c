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
  for (int i = 0; i < length; i++) if (!isAlpha(key[i]) && !(key[i] == '_')) return 0;
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

char * get_hour() {
  char *hour = malloc(9 * sizeof(char));
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  sprintf(hour, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
  return hour;
}

int compareSensors(Sensor *s1, Sensor *s2) {
  return (strcmp(s1->id, s2->id) == 0) && (strcmp(s1->key, s2->key) == 0) && (s1->min == s2->min) && (s1->max == s2->max) && (s1->inter == s2->inter);
}

int compareAlerts(Alert *a1, Alert *a2) {
  return (strcmp(a1->id, a2->id) == 0) && (strcmp(a1->key, a2->key) == 0) && (a1->min == a2->min) && (a1->max == a2->max);
}
