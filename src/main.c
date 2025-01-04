#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <cecs.h>


int main()
{
  printf("Hello, world!\n");

  signature_t signature;
  signature.components[0] = 0b00001000u;

  if (CECS_HAS_COMPONENT(&signature, 0, 0b00001000u)) {
    printf("Component detected!\n");
  }

  return 0;
}