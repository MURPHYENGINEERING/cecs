#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <cecs.h>


int main()
{
  printf("Hello, world!\n");

  signature_t signature;
  signature.components[0] = 0b10000010u;

  if (CECS_HAS_COMPONENT(&signature, 1)) {
    printf("Component detected!\n");
  }

  return 0;
}