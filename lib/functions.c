#include "functions.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isDigit(char c) {
  return c >= '0' && c <= '9';
}

int verifyLength(char *str) {
  int length = strlen(str);
  return length >= 3 && length <= 32;
}

int verifyID(char *id) {
  if (!verifyLength(id)) return 0;
  int length = strlen(id);
  for (int i = 0; i < length; i++) if (!isAlpha(id[i]) && !isDigit(id[i])) return 0;
  return 1;
}

int verifyKey(char *key) {
  if (!verifyLength(key)) return 0;
  int length = strlen(key);
  for (int i = 0; i < length; i++) if (!isDigit(key[i]) && !isAlpha(key[i]) && !(key[i] == '_')) return 0;
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

int compareSensors(Sensor s1, Sensor s2) {
  return (strcmp(s1.id, s2.id) == 0) && (strcmp(s1.key, s2.key) == 0);
}

int searchSensor(Sensor * sensors, Sensor s, int len) {
  for (int i = 0; i < len; i++) {
    if (compareSensors(sensors[i], s)) return i;
  }
  return -1;
}

int compareAlerts(Alert a1, Alert a2) {
  return (strcmp(a1.id, a2.id) == 0) && (strcmp(a1.key, a2.key) == 0) && (a1.min == a2.min) && (a1.max == a2.max) && (a1.user == a2.user);
}

int searchAlert(Alert * alerts, Alert a, int len, int flag) {
  for (int i = 0; i < len; i++) {
    if ((!flag && compareAlerts(alerts[i], a)) || (flag && (strcmp(alerts[i].id, a.id) == 0))) return i;
  }
  return -1;
}

int searchStat(Stat * stats, char *k, int len) { // CHECK THIS FKING FUNCTION
  for (int i = 0; i < len; i++) {
    if (strcmp(stats[i].key, k) == 0) return i;
  }
  return -1;
}

int sum_array(int * array, int size) {
  int sum = 0;
  for (int i = 0; i < size; i++) sum += array[i];
  return sum;
}