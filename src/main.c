#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cecs.h>


typedef struct {
  float x, y;
} position_t, velocity_t;


CECS_COMPONENT_DECL(position_t);
CECS_COMPONENT_DECL(velocity_t);



void move_system()
{
  cecs_iter_t it        = cecs_query(position_t, velocity_t);
  
  if (CECS_HAS_COMPONENT(&it.archetype->sig, CECS_ID_OF(position_t))) {
    printf("Component detected: POSITION\n");
  }
  if (CECS_HAS_COMPONENT(&it.archetype->sig, CECS_ID_OF(velocity_t))) {
    printf("Component detected: VELOCITY\n");
  }
  
  cecs_entity_t *entity = it.first;
  while  (entity != it.end) {
    position_t *pos = (position_t *)cecs_get(it, entity, position_t);
    velocity_t *vel = (velocity_t *)cecs_get(it, entity, velocity_t);

    (void)pos;
    (void)vel;

    printf("Entity: %p\n", entity);

    ++entity;

    // pos->x += vel->x;
    // pos->y += vel->y;
  }
}


int main()
{
  printf("Hello, world!\n");

  CECS_COMPONENT(position_t);
  CECS_COMPONENT(velocity_t);

  cecs_entity_t entity = cecs_create(position_t, velocity_t);

  (void)entity;

  for (size_t i = 0; i < 1; ++i) {
    move_system();
  }

  return 0;
}