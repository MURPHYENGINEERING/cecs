#ifndef __CECS_H__
#define __CECS_H__

#include <stdint.h>


typedef uint64_t cecs_id_t;
typedef cecs_id_t cecs_entity_t;
typedef cecs_id_t cecs_component_id_t;
typedef void cecs_iter_t;

#define CECS_N_COMPONENTS  256u
#define CECS_MAX_COMPONENT ((cecs_component_id_t)(CECS_N_COMPONENTS - 1u))

#define CECS_COMPONENT(type) (1u)

typedef struct {
  cecs_component_id_t components[CECS_N_COMPONENTS / sizeof(cecs_component_id_t)];
} signature_t;


#define CECS_HAS_COMPONENT(signature, component) \
  ((signature)->components[component / sizeof(cecs_component_id_t)] & (1 << (component % sizeof(cecs_component_id_t))))


typedef struct {
} cecs_container_t;


cecs_iter_t *cecs_query(size_t n, ...);
void* cecs_get(cecs_iter_t* it, cecs_component_id_t id);


#endif