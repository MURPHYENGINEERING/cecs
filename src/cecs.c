#include <stdint.h>
#include <stdlib.h>

#include <cecs.h>


cecs_iter_t *cecs_query(size_t n, ...)
{
  (void)n;
  return NULL;
}


void* cecs_get(cecs_iter_t* it, cecs_component_id_t id)
{
  (void)it;
  (void)id;
  return NULL;
}
