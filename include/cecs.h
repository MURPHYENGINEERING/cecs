#ifndef __CECS_H__
#define __CECS_H__

#include <stdint.h>


typedef unsigned char bool;
#define false ((bool)0u)
#define true  ((bool)0u)


/** An ID tracks an element of data in the CECS system */
typedef uint64_t cecs_id_t;
/** Represents an object that composes over components */
typedef cecs_id_t cecs_entity_t;
/** A component is a struct of data that can be composed to form an entity's
 * state */
typedef cecs_id_t cecs_component_id_t;

/** Used to track the ID that will be assigned to the next component defined */
extern cecs_component_id_t CECS_NEXT_COMPONENT_ID;


#define CECS_ID_OF(type)   __cecs_##type##_id
#define CECS_SIZE_OF(type) __cecs_##type##_size


/** Maximum number of entities that can implement a particular archetype.
 * This is not the maximum number of entities that can exist, only the maximum
 * number that can have the same archetype. */
#define CECS_N_ENTITIES ((cecs_entity_t)65536u)
/** Maximum entity ID based on `CECS_N_ENTITIES` */
#define CECS_MAX_ENTITY ((cecs_entity_t)(CECS_N_ENTITIES - (cecs_entity_t)1u))


#define CECS_N_COMPONENTS ((cecs_component_id_t)1024u)
#define CECS_MAX_COMPONENT \
  ((cecs_component_id_t)(CECS_N_COMPONENTS - (cecs_component_id_t)1u))

/** Convert a component ID to an index into a signature bit field array. */
#define CECS_COMPONENT_TO_INDEX(component) \
  ((component) / ((cecs_component_id_t)8u * sizeof(cecs_component_id_t)))

/** Convert a component ID to the LSB into the appropriate index into the
 * signature bit field array. */
#define CECS_COMPONENT_TO_LSB(component) \
  ((component) % ((cecs_component_id_t)8u * sizeof(cecs_component_id_t)))

/** Convert a component ID to the bit pattern representing it in the
 * appropriate index into the signature bit field array. */
#define CECS_COMPONENT_TO_BITS(component) \
  ((cecs_component_id_t)1u << CECS_COMPONENT_TO_LSB(component))


/** Returns whether the given signature contains the specified component ID. */
#define CECS_HAS_COMPONENT(signature, component)               \
  ((signature)->components[CECS_COMPONENT_TO_INDEX(component)] \
   & CECS_COMPONENT_TO_BITS(component))

/** Add the specified component ID to the given signature. */
#define CECS_ADD_COMPONENT(signature, component)               \
  ((signature)->components[CECS_COMPONENT_TO_INDEX(component)] \
   |= CECS_COMPONENT_TO_BITS(component))

/** Remove the specified component ID from the given signature. */
#define CECS_REMOVE_COMPONENT(signature, component)            \
  ((signature)->components[CECS_COMPONENT_TO_INDEX(component)] \
   &= ~CECS_COMPONENT_TO_BITS(component))


#define CECS_COMPONENT_DECL(type)                           \
  static const size_t CECS_SIZE_OF(type)      = (size_t)0u; \
  static cecs_component_id_t CECS_ID_OF(type) = (cecs_component_id_t)0u

#define CECS_COMPONENT(type) \
  ((__cecs_##type##_id) = (cecs_component_id_t)(CECS_NEXT_COMPONENT_ID++))


#define FE_0(WHAT)
#define FE_1(WHAT, X) WHAT(X) 
#define FE_2(WHAT, X, ...) WHAT(X), FE_1(WHAT, __VA_ARGS__)
#define FE_3(WHAT, X, ...) WHAT(X), FE_2(WHAT, __VA_ARGS__)
#define FE_4(WHAT, X, ...) WHAT(X), FE_3(WHAT, __VA_ARGS__)
#define FE_5(WHAT, X, ...) WHAT(X), FE_4(WHAT, __VA_ARGS__)
//... repeat as needed

#define GET_MACRO(_0,_1,_2,_3,_4,_5,NAME,...) NAME 
#define FOR_EACH(action,...) \
  GET_MACRO(_0,__VA_ARGS__,FE_5,FE_4,FE_3,FE_2,FE_1,FE_0)(action,__VA_ARGS__)

#define FOR_EACH_NARG(...) FOR_EACH_NARG_(__VA_ARGS__, FOR_EACH_RSEQ_N())
#define FOR_EACH_NARG_(...) FOR_EACH_ARG_N(__VA_ARGS__) 
#define FOR_EACH_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N 
#define FOR_EACH_RSEQ_N() 8, 7, 6, 5, 4, 3, 2, 1, 0


/** Get the component of the specified type for the given entity, drawing from
  * the given query result */
#define cecs_get(it, entity, type) \
  (type *)_cecs_get(&it, entity, CECS_ID_OF(type), CECS_SIZE_OF(type))

/** Create a new entity with the given components */
#define cecs_create(...) \
  _cecs_create(FOR_EACH_NARG(__VA_ARGS__), FOR_EACH(CECS_ID_OF, __VA_ARGS__))

/** Get an iterator over entities that have the specified components */
#define cecs_query(...) \
  _cecs_query(FOR_EACH_NARG(__VA_ARGS__), FOR_EACH(CECS_ID_OF, __VA_ARGS__))


/** A signature represents the components implemented by a type as a bit set. */
typedef struct {
  cecs_component_id_t components[CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS)];
} cecs_sig_t;


/** An archetype is a unique composition of components. */
typedef struct {
  /** Component signature implemented by this archetype */
  cecs_sig_t sig;
  /** Number of components implemented by this archetype */
  size_t n_components;
  /** Array of sizes in bytes of the structs pointed to by the `components` array */
  size_t component_sizes[CECS_N_COMPONENTS];
  /** Array of pointers to the structs implementing the components in this archetype */
  void *components[CECS_N_COMPONENTS];
  /** Number of entities implementing this archetype */
  size_t n_entities;
  /** Array of entities implementing this archetype */
  cecs_entity_t entities[CECS_N_ENTITIES];
} cecs_archetype_t;


/** Iterator over entities matching the selected archetype. */
typedef struct {
  /** Archetype being iterated over */
  cecs_archetype_t *archetype;
  /** Pointer to the first entity implementing this archetype */
  cecs_entity_t *first;
  /** Pointer to one past the last entity implementing this archetype */
  cecs_entity_t *end;
} cecs_iter_t;


/** Maximum number of combinations of components that can be represented */
#define CECS_N_ARCHETYPES 8192u


/** Return an iterator over the entities representing the archetype specified in
 * the varargs parameter */
cecs_iter_t _cecs_query(cecs_component_id_t n, ...);
/** Get a pointer to the component implemented by the specified entity, drawing
 * from the given entity iterator over an archetype */
void *_cecs_get(cecs_iter_t *it, cecs_entity_t *entity, cecs_component_id_t id, size_t size);

/** Create an entity with the given components */
cecs_entity_t _cecs_create(cecs_component_id_t n, ...);

/** Add the given components to the specified entity */
cecs_archetype_t *cecs_add(cecs_entity_t entity, cecs_component_id_t n, ...);

/** Remove the given components from the specified entity */
cecs_archetype_t *cecs_remove(cecs_entity_t entity, cecs_component_id_t n, ...);


#endif