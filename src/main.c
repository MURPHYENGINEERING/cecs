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


/* First you have to declare that the components exist. This records vital
information about their type and assigns each a unique ID. */
CECS_COMPONENT_DECL(position_t);
CECS_COMPONENT_DECL(velocity_t);
CECS_COMPONENT_DECL(health_t);



void move_system()
{
  /* An entity set iterator allows you to iterate over all the entities that
  implement the queried component set. */
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
    /* Getting the component data for a given entity,component pair allows you
    to update it in-situ */
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

  /* In addition to declaring the components, they must be registered in some
  setup function. This adds entries to the component table so they can be
  assigned to entities. */
  CECS_COMPONENT(position_t);
  CECS_COMPONENT(velocity_t);
  CECS_COMPONENT(health_t);

  /* Create dummy data to populate entity's components with */
  position_t position = { .x = 0.0f, .y = 0.0f };
  velocity_t velocity = { .x = 1.0f, .y = 1.0f };
  health_t health = { .hp = 100 };

  /* Create an entity implementing a set of compnents */
  cecs_entity_t entity = cecs_create(health_t, position_t);
  
  /* Create an entity and add another component to it */
  entity = cecs_create(velocity_t, position_t);
  cecs_add(entity, health_t);

  /* Populate component data for the entity with the dummy data from above */
  cecs_set(entity, position_t, &position);
  cecs_set(entity, velocity_t, &velocity);
  cecs_set(entity, health_t, &health);

  /* Try adding and removing components to and from the entity after creation */
  entity = cecs_create(velocity_t);
  cecs_add(entity, position_t);
  cecs_remove(entity, position_t);
  cecs_add(entity, position_t);
  cecs_add(entity, health_t);

  cecs_set(entity, position_t, &position);
  cecs_set(entity, velocity_t, &velocity);
  cecs_set(entity, health_t, &health);


  /* Iterate the "move" system, which updates position based on velocity. */
  for (size_t i = 0; i < 4; ++i) {
    move_system();
  }

  return 0;
}