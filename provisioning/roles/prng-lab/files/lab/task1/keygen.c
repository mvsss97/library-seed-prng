/* SEED Labs - Pseudo Random Number Generation Lab
 * Listing 1 : "Generating a 128-bit encryption key"
 *
 * Compilation : gcc keygen.c -o keygen
 * Exécution   : ./keygen
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define KEYSIZE 16

void main()
{
  int i;
  char key[KEYSIZE];

  printf("%lld\n", (long long) time(NULL));
  srand (time(NULL));          /* (1) */

  for (i = 0; i< KEYSIZE; i++){
     key[i] = rand()%256;
     printf("%.2x", (unsigned char)key[i]);
  }
  printf("\n");
}
