#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cecs.h>


typedef struct {
  float x, y;
} position_t, velocity_t;


cecs_component_id_t POSITION_ID;
cecs_component_id_t VELOCITY_ID;


CECS_COMPONENT_DECL(position_t);
CECS_COMPONENT_DECL(velocity_t);



void move_system()
{
  CECS_COMPONENT(position_t);
  CECS_COMPONENT(velocity_t);

  cecs_iter_t it        = cecs_query(2, POSITION_ID, VELOCITY_ID);
  
  if (CECS_HAS_COMPONENT(&it.archetype->sig, POSITION_ID)) {
    printf("Component detected: POSITION\n");
  }
  if (CECS_HAS_COMPONENT(&it.archetype->sig, VELOCITY_ID)) {
    printf("Component detected: VELOCITY\n");
  }
  
  cecs_entity_t *entity = it.first;
  while  (entity != it.end) {
    position_t *pos = (position_t *)cecs_get(&it, entity, position_t);
    velocity_t *vel = (velocity_t *)cecs_get(&it, entity, velocity_t);

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

  cecs_entity_t entity = cecs_create(position_t, velocity_t);

  (void)entity;

  for (size_t i = 0; i < 1; ++i) {
    move_system();
  }

  return 0;
}