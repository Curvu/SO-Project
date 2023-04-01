#include "functions.h"
#include <string.h>

/*
  id: alfanumérico 3 a 32 caracteres
  min, max: inteiros
  chave: [a-Z0-9_] 3 a 32 caracteres
*/
int isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int verifyLength(char *str) {
  int length = strlen(str);
  return length >= 3 && length <= 32;
}

int verifyID(char *id) {
  int length = strlen(id);
  for (int i = 0; i < length; i++) if (!isAlpha(id[i])) return 0;
  return 1;
}

int verifyKey(char *key) {
  int length = strlen(key);
  for (int i = 0; i < length; i++) if (!isAlpha(key[i]) && !(key[i] != '_')) return 0;
  return 1;
}
