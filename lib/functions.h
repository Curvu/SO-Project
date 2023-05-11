/**
 * @file functions.h
 * @authors
 * - Filipe Rodrigues (2021218054)
 * - Joás Silva (2021226149)
 */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEBUG

#define MESSAGE_QUEUE_KEY 13579
#define BUFFER 2048
#define MAX 200
#define STR 33


typedef struct {
  char key[STR];
  int last, min, max;
  unsigned int count;
  double avg;
  bool checked;
} Stat;

typedef struct {
  char id[STR];
} Sensor;

typedef struct {
  char id[STR], key[STR];
  int min, max, user;
} Alert;

typedef struct {
  int user;
  int command;
  Alert alert;
} Command;

typedef struct {
  long type; // this will be the id of the user_console
  char response[BUFFER];
} Message;

#define NULL_SENSOR (Sensor) { "" }
#define NULL_ALERT (Alert) { "", "", 0, 0, 0 }
#define NULL_STAT (Stat) {"", 0, 0, 0, 0, 0, false}

#define SENSOR_FIFO "sensor_fifo"
#define USER_FIFO "user_fifo"


/**
 * @brief verify if char is alpha
 * @param c 
 * @return int 
 */
int isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/**
 * @brief verify if char is digit
 * @param c 
 * @return int 
 */
int isDigit(char c) {
  return c >= '0' && c <= '9';
}

/**
 * @brief verify if string is between 3 and 32
 * @param str 
 * @return int 
 */
int verifyLength(char *str) {
  int length = strlen(str);
  return length >= 3 && length <= 32;
}

/**
 * @brief verify if id is alfa-numeric
 * @param id
 * @return 1 if id is valid, 0 otherwise
 */
int verifyID(char * id) {
  if (!verifyLength(id)) return 0;
  int length = strlen(id);
  for (int i = 0; i < length; i++) if (!isAlpha(id[i]) && !isDigit(id[i])) return 0;
  return 1;
}

/**
 * @brief verify if key is alfa-numeric or '_'
 * @param char* key
 * @return 1 if key is valid, 0 otherwise
 */
int verifyKey(char * key) {
  if (!verifyLength(key)) return 0;
  int length = strlen(key);
  for (int i = 0; i < length; i++) if (!isDigit(key[i]) && !isAlpha(key[i]) && !(key[i] == '_')) return 0;
  return 1;
}

/**
 * @brief
 * @param parameter
 * @param var to attach the parameter
 * @param type 1 is int otherwise is string
 */
void verifyParam(char * param, void * var, int type) {
  int toggle; 
  if (type) toggle = sscanf(param, " %d", (int*) var); // int
  else toggle = sscanf(param, " %s", (char*) var); // string

  if (toggle == 0 || toggle == EOF) {
    printf("Invalid parameter: %s\n", param);
    exit(0);
  }
}

/**
 * @brief hour:minute:second
 * @return string with hour
 */
char * get_hour() {
  char *hour = malloc(9 * sizeof(char));
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  sprintf(hour, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
  return hour;
}

/**
 * @brief compare two sensors
 * @param Sensor s1
 * @param Sensor s2
 * @return 1 if s1 == s2, 0 otherwise
 */
int compareSensors(Sensor s1, Sensor s2) {
  return (strcmp(s1.id, s2.id) == 0);
}

/**
 * @brief Search sensor in sensor list
 * @param Sensor* sensors
 * @param Sensor s
 * @param int len
 * @return return index if finds, -1 otherwise
 */
int searchSensor(Sensor * sensors, Sensor s, int len) {
  for (int i = 0; i < len; i++) {
    if (compareSensors(sensors[i], s)) return i;
  }
  return -1;
}

/**
 * @brief compare two alerts
 * @param Alert a1
 * @param Alert a2
 * @return 1 if a1 == a2, 0 otherwise
 */
int compareAlerts(Alert a1, Alert a2) {
  return (strcmp(a1.id, a2.id) == 0) && (strcmp(a1.key, a2.key) == 0) && (a1.min == a2.min) && (a1.max == a2.max) && (a1.user == a2.user);
}

/**
 * @brief Search alert in alert list
 * @param Alert* alerts
 * @param Alert a
 * @param int len
 * @param int flag - 0 if it's comparing all vars, 1 if comparing only id
 * @return return index if finds, -1 otherwise
 */
int searchAlert(Alert * alerts, Alert a, int len, int flag) {
  for (int i = 0; i < len; i++) {
    if ((!flag && compareAlerts(alerts[i], a)) || (flag && (strcmp(alerts[i].id, a.id) == 0))) return i;
  }
  return -1;
}

/**
 * @brief Search key in stat list
 * @param Stat* stats
 * @param char* k
 * @param int len
 * @return return index if finds, -1 otherwise
 */
int searchStat(Stat * stats, char *k, int len) { // CHECK THIS FKING FUNCTION
  for (int i = 0; i < len; i++) {
    if (strcmp(stats[i].key, k) == 0) return i;
  }
  return -1;
}

/**
 * @brief sum all values in array
 * @param int* array
 * @param int len
 * @return return sum
 */
int sum_array(int * array, int size) {
  int sum = 0;
  for (int i = 0; i < size; i++) sum += array[i];
  return sum;
}

#endif // FUNCTIONS_H
