#ifndef __CECS_H__
#define __CECS_H__

#include <stdint.h>


typedef uint64_t cecs_id_t;
typedef cecs_id_t cecs_entity_t;
typedef cecs_id_t cecs_component_id_t;
typedef void cecs_iter_t;

#define CECS_N_COMPONENTS  1024u
#define CECS_MAX_COMPONENT ((cecs_component_id_t)(CECS_N_COMPONENTS - 1ull))

#define CECS_COMPONENT(type) (1u)

/** Convert a component ID to an index into a signature bit field array. */
#define CECS_COMPONENT_TO_INDEX(component) \
  ((component) / (8ull * sizeof(cecs_component_id_t)))

/** Convert a component ID to the LSB into the appropriate index into the
 * signature bit field array. */
#define CECS_COMPONENT_TO_LSB(component) \
  ((component) % (8ull * sizeof(cecs_component_id_t)))

/** Convert a component ID to the bit pattern representing it in the
 * appropriate index into the signature bit field array. */
#define CECS_COMPONENT_TO_BITS(component) \
  (1ull << CECS_COMPONENT_TO_LSB(component))

/** Returns whether the given signature contains the specified component ID. */
#define CECS_HAS_COMPONENT(signature, component)               \
  ((signature)->components[CECS_COMPONENT_TO_INDEX(component)] \
   & CECS_COMPONENT_TO_BITS(component))

/** Add the specified component ID to the given signature. */
#define CECS_ADD_COMPONENT(signature, component)              \
  ((signature)->components[CECS_COMPONENT_TO_INDEX(component)] \
   |= CECS_COMPONENT_TO_BITS(component))

/** Remove the specified component ID from the given signature. */
#define CECS_REMOVE_COMPONENT(signature, component)            \
  ((signature)->components[CECS_COMPONENT_TO_INDEX(component)] \
   &= ~CECS_COMPONENT_TO_BITS(component))


/** A signature represents the components implemented by a type as a bit set. */
typedef struct {
  cecs_component_id_t components[CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS)];
} cecs_sig_t;


cecs_iter_t *cecs_query(size_t n, ...);
void *cecs_get(cecs_iter_t *it, cecs_component_id_t id);


#endif