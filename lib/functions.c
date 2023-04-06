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

int compareSensor(Sensor *s1, Sensor *s2) {
  return strcmp(s1->id, s2->id) == 0 && strcmp(s1->key, s2->key) == 0 && s1->min == s2->min && s1->max == s2->max && s1->inter == s2->inter;
}
