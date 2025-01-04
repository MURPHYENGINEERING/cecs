#ifndef __CECS_H__
#define __CECS_H__

#include <stdint.h>


typedef uint32_t entity_t;
typedef uint8_t component_group_t;
typedef uint64_t component_id_t;

typedef struct {
  component_group_t group;
  component_id_t id;
} component_t;


#define CECS_N_COMPONENTS  256u
#define CECS_MAX_COMPONENT (CECS_N_COMPONENTS - 1u)


typedef struct {
  component_id_t components[CECS_N_COMPONENTS / (sizeof(component_id_t) * 8u)];
} signature_t;


#define CECS_HAS_COMPONENT(signature, component_group, component)            \
  ((signature)->components[((component_group % 8u) * component) / sizeof(component_id_t)] \
   & (component % (sizeof(component_id_t))))


struct cecs_container {
};


#endif