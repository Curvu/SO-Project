#ifndef FUNCTIONS_H
#define FUNCTIONS_H

/**
 * @brief verify if string has between 3 and 32 characters
 * @param str string to be verified
 * @return 1 if length is valid, 0 otherwise
 */
int verifyLength(char *);

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

#endif // FUNCTIONS_H
