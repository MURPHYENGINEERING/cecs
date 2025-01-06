#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cecs.h>


typedef struct {
  float x, y;
} position_t, velocity_t;


cecs_component_id_t POSITION_ID;
cecs_component_id_t VELOCITY_ID;


void move_system()
{
  POSITION_ID = CECS_COMPONENT(position_t);
  VELOCITY_ID = CECS_COMPONENT(velocity_t);

  cecs_iter_t it        = cecs_query(2, POSITION_ID, VELOCITY_ID);
  
  if (CECS_HAS_COMPONENT(&it.sig, POSITION_ID)) {
    printf("Component detected: POSITION\n");
  }
  if (CECS_HAS_COMPONENT(&it.sig, VELOCITY_ID)) {
    printf("Component detected: VELOCITY\n");
  }
  
  cecs_entity_t *entity = it.first;
  while  (entity != it.end) {
    position_t *pos = (position_t *)cecs_get(&it, entity, POSITION_ID);
    velocity_t *vel = (velocity_t *)cecs_get(&it, entity, VELOCITY_ID);

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

  for (size_t i = 0; i < 1; ++i) {
    move_system();
  }

  return 0;
}