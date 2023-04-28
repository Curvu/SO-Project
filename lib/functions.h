#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define DEBUG
#define MAX 200
#define STR 33

typedef struct {
  char id[STR], key[STR];
  int min, max, inter;
} Sensor;

typedef struct {
  char id[STR], key[STR];
  int min, max;
} Alert;

typedef struct {
  int user;
  int command;
  Alert alert;
} Message;

#define NULL_SENSOR (Sensor) { "", "", 0, 0, 0 }
#define NULL_ALERT (Alert) { "", "", 0, 0 }

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
 * @param Sensor* s1
 * @param Sensor* s2
 * @return 1 if s1 == s2, 0 otherwise
 */
int compareSensors(Sensor *, Sensor *);

/**
 * @brief check if sensor has the same id and key
 * @param Sensor* s
 * @param char* id
 * @param char* key
 * @return 1 if it's the same 0 otherwise
 */

int checkSensor(Sensor *, char *, char *);

/**
 * @brief copy a s2 to s1 
 * @param Sensor* s1
 * @param Sensor* s2
 */
void cpySensor(Sensor *, Sensor *);

/**
 * @brief compare two alerts
 * @param Alert* a1
 * @param Alert* a2
 * @return 1 if a1 == a2, 0 otherwise
 */
int compareAlerts(Alert *, Alert *);

#endif // FUNCTIONS_H
