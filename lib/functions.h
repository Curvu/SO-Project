#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define DEBUG
#define MAX 200
#define STR 33

typedef struct {
  char id[STR], key[STR];
  int min, max, inter;
} Sensor;

#define NULL_SENSOR (Sensor) { "", "", 0, 0, 0 }

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
 * @return 1 if s1 is equal to s2, 0 otherwise
 */
int compareSensor(Sensor *, Sensor *);

#endif // FUNCTIONS_H
