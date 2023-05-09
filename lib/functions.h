#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include <stdbool.h>

#define DEBUG

#define MUTEX_SENSOR "/mutex_sensor"
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
  char id[STR], key[STR];
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

#define NULL_SENSOR (Sensor) { "", "" }
#define NULL_ALERT (Alert) { "", "", 0, 0, 0 }
#define NULL_STAT (Stat) {"", 0, 0, 0, 0, 0, false}

#define SENSOR_FIFO "sensor_fifo"
#define USER_FIFO "user_fifo"

/**
 * @brief verify if id is alfa-numeric
 * @param id
 * @return 1 if id is valid, 0 otherwise
 */
int verifyID(char *);

/**
 * @brief verify if key is alfa-numeric or '_'
 * @param char* key
 * @return 1 if key is valid, 0 otherwise
 */
int verifyKey(char *);

/**
 * @brief
 * @param parameter
 * @param var to attach the parameter
 * @param type 1 is int otherwise is string
 */
void verifyParam(char *, void *, int);

/**
 * @brief hour:minute:second
 * @return string with hour
 */
char *get_hour();

/**
 * @brief compare two sensors
 * @param Sensor s1
 * @param Sensor s2
 * @return 1 if s1 == s2, 0 otherwise
 */
int compareSensors(Sensor, Sensor);

/**
 * @brief Search sensor in sensor list
 * @param Sensor* sensors
 * @param Sensor s
 * @param int len
 * @return return index if finds, -1 otherwise
 */
int searchSensor(Sensor *, Sensor, int);

/**
 * @brief compare two alerts
 * @param Alert a1
 * @param Alert a2
 * @return 1 if a1 == a2, 0 otherwise
 */
int compareAlerts(Alert, Alert);

/**
 * @brief Search alert in alert list
 * @param Alert* alerts
 * @param Alert a
 * @param int len
 * @param int flag - 0 if it's comparing all vars, 1 if comparing only id
 * @return return index if finds, -1 otherwise
 */
int searchAlert(Alert *, Alert, int, int);

/**
 * @brief Search key in stat list
 * @param Stat* stats
 * @param char* k
 * @param int len
 * @return return index if finds, -1 otherwise
 */
int searchStat(Stat *, char *, int);

/**
 * @brief sum all values in array
 * @param int* array
 * @param int len
 * @return return sum
 */
int sum_array(int *, int);

#endif // FUNCTIONS_H
