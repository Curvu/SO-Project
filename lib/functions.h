#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define DEBUG
#define MAX 200
#define STR 33

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
 * @brief get hour:minute:second
 * @param char* hour
 */
void get_hour(char *);

#endif // FUNCTIONS_H
