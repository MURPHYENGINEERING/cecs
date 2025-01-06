#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include <cecs.h>


cecs_component_id_t CECS_NEXT_COMPONENT_ID = 0ull;

cecs_archetype_t cecs_archetypes[CECS_N_ARCHETYPES];


cecs_iter_t cecs_query(size_t n, ...)
{
  va_list components;

  cecs_iter_t it;
  memset(&it.sig, 0u, sizeof(it.sig));

  va_start(components, n);
  for (size_t i = 0; i < n; ++i) {
    cecs_component_id_t id = va_arg(components, cecs_component_id_t);
    CECS_ADD_COMPONENT(&it.sig, id);
  }
  va_end(components);

  it.first = &cecs_archetypes[0].entities[0];
  it.end = &cecs_archetypes[0].entities[2];

  return it;
}


void *cecs_get(cecs_iter_t *it, cecs_entity_t *entity, cecs_component_id_t id)
{
  (void)it;
  (void)entity;
  (void)id;
  return NULL;
}
