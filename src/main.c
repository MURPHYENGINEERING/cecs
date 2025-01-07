#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cecs.h>


typedef struct {
  float x, y;
} position_t, velocity_t;


typedef struct {
  uint32_t hp;
} health_t;


CECS_COMPONENT_DECL(position_t);
CECS_COMPONENT_DECL(velocity_t);
CECS_COMPONENT_DECL(health_t);



void move_system()
{
  cecs_iter_t it;
  cecs_entity_t entity;

  cecs_query(&it, velocity_t);
  while ((entity = cecs_iter_next(&it))) {
    printf("[velocity] %llu\n", entity);
  }

  cecs_query(&it, position_t);
  while ((entity = cecs_iter_next(&it))) {
    printf("[position] %llu\n", entity);
  }

  cecs_query(&it, position_t, velocity_t);
  while ((entity = cecs_iter_next(&it))) {
    printf("[position, velocity] %llu\n", entity);
  }
  
  cecs_query(&it, velocity_t, position_t, health_t);
  while  ((entity = cecs_iter_next(&it))) {
    position_t *pos = cecs_get(entity, position_t);
    velocity_t *vel = cecs_get(entity, velocity_t);
    health_t *health = cecs_get(entity, health_t);

    printf("[position, velocity, health] %llu\n", entity);

    printf("Position: %f,%f     velocity: %f,%f,    health: %u\n", pos->x, pos->y, vel->x, vel->y, health->hp);
    pos->x += vel->x;
    pos->y += vel->y;
    health->hp -= 1u;
  }
}


int main()
{
  printf("Hello, world!\n");

  CECS_COMPONENT(position_t);
  CECS_COMPONENT(velocity_t);
  CECS_COMPONENT(health_t);

  position_t position = { .x = 0.0f, .y = 0.0f };
  velocity_t velocity = { .x = 1.0f, .y = 1.0f };
  health_t health = { .hp = 100 };

  cecs_entity_t entity = cecs_create(health_t, position_t);
  
  entity = cecs_create(velocity_t, position_t);
  cecs_add(entity, health_t);

  cecs_set(entity, position_t, &position);
  cecs_set(entity, velocity_t, &velocity);
  cecs_set(entity, health_t, &health);

  entity = cecs_create(velocity_t);
  cecs_add(entity, position_t);
  cecs_remove(entity, position_t);
  cecs_add(entity, position_t);
  cecs_add(entity, health_t);


  cecs_set(entity, position_t, &position);
  cecs_set(entity, velocity_t, &velocity);
  cecs_set(entity, health_t, &health);


  for (size_t i = 0; i < 4; ++i) {
    move_system();
  }

  return 0;
}