/* SEED Labs - Pseudo Random Number Generation Lab - Tâche 5
 *
 * Extrait de l'énoncé : lecture d'une clé de 128 bits dans /dev/urandom.
 * TRAVAIL : modifier ce programme pour générer une clé de 256 bits et l'afficher
 *           en hexadécimal (64 caractères sur une ligne).
 *
 * Compilation : gcc genkey.c -o genkey
 */
#include <stdio.h>
#include <stdlib.h>

#define LEN 16 // 128 bits

int main()
{
  unsigned char *key = (unsigned char *) malloc(sizeof(unsigned char)*LEN);
  FILE* random = fopen("/dev/urandom", "r");
  fread(key, sizeof(unsigned char)*LEN, 1, random);
  fclose(random);

  /* TODO : afficher la clé en hexadécimal */

  free(key);
  return 0;
}
