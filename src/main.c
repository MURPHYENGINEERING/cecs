#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <cecs.h>


typedef struct {
  float x, y;
} position_t, velocity_t;


cecs_component_id_t POSITION_ID = CECS_COMPONENT(position_t);
cecs_component_id_t VELOCITY_ID = CECS_COMPONENT(velocity_t);


void move_system()
{
  cecs_iter_t *it = cecs_query(2, POSITION_ID, VELOCITY_ID);
  position_t *pos = (position_t*) cecs_get(it, POSITION_ID);
  velocity_t *vel = (velocity_t*) cecs_get(it, VELOCITY_ID);

  pos->x += vel->x;
  pos->y += vel->y;
}


int main()
{
  printf("Hello, world!\n");

  //for (size_t i = 0; i < 10; ++i) {
  //  move_system();
  //}
  signature_t sig;
  sig.components[3] = 0xff00000000000000;

  if (CECS_HAS_COMPONENT(&sig, 255u)) {
    printf("Found component!\n");
  }

  return 0;
}