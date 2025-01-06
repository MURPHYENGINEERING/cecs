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
  cecs_iter_t it        = cecs_query(velocity_t, position_t);
  
  cecs_entity_t *entity = it.first;
  while  (entity != it.end) {
    position_t *pos = cecs_get(it, entity, position_t);
    velocity_t *vel = cecs_get(it, entity, velocity_t);

    printf("[position, velocity] %llu\n", *entity);

    ++entity;

    pos->x += vel->x;
    pos->y += vel->y;
  }

  it = cecs_query(velocity_t);
  entity = it.first;
  while (entity != it.end) {
    printf("[velocity] %llu\n", *entity);

    ++entity;
  } 
  
  it = cecs_query(position_t);
  entity = it.first;
  while (entity != it.end) {
    printf("[position] %llu\n", *entity);

    ++entity;
  }
}


int main()
{
  printf("Hello, world!\n");

  CECS_COMPONENT(position_t);
  CECS_COMPONENT(velocity_t);

  /* 0 */
  cecs_entity_t entity = cecs_create(velocity_t);
  
  /* 1 */
  entity = cecs_create(velocity_t);
  cecs_add(entity, position_t);
  //cecs_remove(entity, velocity_t);

  /* 2 */
  entity = cecs_create(velocity_t, position_t);
  cecs_remove(entity, position_t);

  (void)entity;


  for (size_t i = 0; i < 1; ++i) {
    move_system();
  }

  return 0;
}