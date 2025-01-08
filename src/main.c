#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cecs.h>


typedef struct {
  float x, y;
} has_position_t, has_velocity_t;


typedef struct {
  int32_t hp;
} has_health_t;

typedef struct {
} is_alive_t;


/* First you have to declare that the components exist. This records vital
information about their type and assigns each a unique ID. */
CECS_COMPONENT_DECL(has_position_t);
CECS_COMPONENT_DECL(has_velocity_t);
CECS_COMPONENT_DECL(has_health_t);
CECS_COMPONENT_DECL(is_alive_t);



void move_system()
{
  /* An entity set iterator allows you to iterate over all the entities that
  implement the queried component set. */
  cecs_iter_t it;
  cecs_entity_t entity;

  cecs_query(&it, is_alive_t, has_health_t);
  while ((entity = cecs_iter_next(&it))) {
    has_health_t *health = cecs_get(entity, has_health_t);

    health->hp -= 10;
    if (health->hp <= 0) {
      cecs_remove(entity, is_alive_t);
    }
    printf("[alive, health] %llu = %d\n", entity, health->hp);
  }

  cecs_query(&it, has_velocity_t);
  while ((entity = cecs_iter_next(&it))) {
    printf("[velocity] %llu\n", entity);
  }

  cecs_query(&it, has_position_t);
  while ((entity = cecs_iter_next(&it))) {
    printf("[position] %llu\n", entity);
  }

  cecs_query(&it, has_position_t, has_velocity_t);
  while ((entity = cecs_iter_next(&it))) {
    printf("[position, velocity] %llu\n", entity);
  }
  
  cecs_query(&it, has_velocity_t, has_position_t, has_health_t);
  while  ((entity = cecs_iter_next(&it))) {
    /* Getting the component data for a given entity,component pair allows you
    to update it in-situ */
    has_position_t *pos = cecs_get(entity, has_position_t);
    has_velocity_t *vel = cecs_get(entity, has_velocity_t);
    has_health_t *health = cecs_get(entity, has_health_t);

    printf("[position, velocity, health] %llu\n", entity);

    printf("Position: %f,%f     velocity: %f,%f,    health: %u\n", pos->x, pos->y, vel->x, vel->y, health->hp);
    
    /* The "move" system updates position based on velocity. */
    pos->x += vel->x;
    pos->y += vel->y;
  }
}


int main()
{
  printf("Hello, world!\n");
  printf("================================================================================\n");

  /* In addition to declaring the components, they must be registered in some
  setup function. This adds entries to the component table so they can be
  assigned to entities. */
  CECS_COMPONENT(has_position_t);
  CECS_COMPONENT(has_velocity_t);
  CECS_COMPONENT(has_health_t);
  CECS_COMPONENT(is_alive_t);

  /* Create dummy data to populate entity's components with */
  has_position_t position = { .x = 0.0f, .y = 0.0f };
  has_velocity_t velocity = { .x = 1.0f, .y = 1.0f };
  has_health_t health = { .hp = 30 };

  /* Create an entity implementing a set of compnents */
  cecs_entity_t entity1 = cecs_create(is_alive_t, has_health_t, has_position_t);
  cecs_zero(entity1, has_position_t);
  cecs_set(entity1, has_health_t, &health);
  
  /* Create an entity and add another component to it */
  cecs_entity_t entity = cecs_create(has_velocity_t, has_position_t);
  cecs_add(entity, has_health_t);

  /* Populate component data for the entity with the dummy data from above */
  cecs_zero(entity, has_position_t);
  cecs_set(entity, has_velocity_t, &velocity);
  cecs_set(entity, has_health_t, &health);

  /* Try adding and removing components to and from the entity after creation */
  entity = cecs_create(has_velocity_t);
  cecs_add(entity, has_position_t);
  cecs_remove(entity, has_position_t);
  cecs_add(entity, has_position_t);
  cecs_add(entity, has_health_t);

  cecs_zero(entity, has_position_t);
  cecs_set(entity, has_velocity_t, &velocity);
  cecs_set(entity, has_health_t, &health);

  cecs_remove(entity1, has_position_t);


  /* Iterate the "move" system, which updates position based on velocity. */
  for (size_t i = 0; i < 4; ++i) {
    move_system();
    printf("--------------------------------------------------------------------------------\n");
  }

  return 0;
}