#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <cecs.h>


cecs_component_id_t CECS_NEXT_COMPONENT_ID = (cecs_component_id_t) 0u;

/** This is where the actual entity data gets stored as arrays of entity IDs per
  * archetype */
static cecs_archetype_t archetypes[CECS_N_ARCHETYPES];
static size_t n_archetypes = 0;

/** Map from entity ID to the archetype it implements */
static cecs_archetype_t *archetypes_by_entity[CECS_N_ENTITIES];

/** The number of entities currently living in the world */
static cecs_entity_t n_entities = 0;


static bool sigs_are_equal(cecs_sig_t *lhs, cecs_sig_t *rhs)
{
  for (cecs_component_id_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if (lhs->components[i] != rhs->components[i]) {
      return false;
    }
  }

  return true;
}


static void sig_union(cecs_sig_t *target, cecs_sig_t *with)
{
  for (cecs_component_id_t i = 0; i  < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    target->components[i] |= with->components[i];
  }
}


static void sig_remove(cecs_sig_t *target, cecs_sig_t *to_remove)
{
  for (cecs_component_id_t i = 0; i  < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    target->components[i] &= ~to_remove->components[i];
  }
}


static cecs_sig_t components_to_sig(cecs_component_id_t n, va_list components)
{
  cecs_sig_t sig;
  memset(&sig, 0u, sizeof(sig));

  for (size_t i = 0; i < n; ++i) {
    cecs_component_id_t id = va_arg(components, cecs_component_id_t);
    CECS_ADD_COMPONENT(&sig, id);
  }

  return sig;
}


static void add_entity_to_archetype(cecs_entity_t entity, cecs_archetype_t *archetype)
{
  archetype->entities[archetype->n_entities++] = entity;
}


static void remove_entity_from_archetype(cecs_entity_t entity, cecs_archetype_t *archetype)
{
  if (archetype->n_entities > 1) {
    for (cecs_entity_t i = 0; i < archetype->n_entities; ++i) {
      if (archetype->entities[i] == entity) {
        /* Put the last entity in the array into the removed entity's slot */
        archetype->entities[i] = archetype->entities[--archetype->n_entities];
      }
    }
  } else if (archetype->n_entities == 1) {
    /* Last entity in the array, just remove it */
    --archetype->n_entities;
  }
}


static cecs_archetype_t *get_or_add_archetype(cecs_sig_t *sig)
{
  for (size_t i = 0; i < n_archetypes; ++i) {
    if (sigs_are_equal(&archetypes[i].sig, sig)) {
      return &archetypes[i];
    }
  }

  archetypes[n_archetypes].sig = *sig;
  return &archetypes[n_archetypes++];
}


cecs_iter_t _cecs_query(cecs_component_id_t n, ...)
{
  va_list components;

  cecs_iter_t it;

  va_start(components, n);
  cecs_sig_t sig = components_to_sig(n, components);
  va_end(components);

  it.archetype = get_or_add_archetype(&sig);

  it.first = &it.archetype->entities[0];
  it.end = &it.archetype->entities[it.archetype->n_entities];

  return it;
}


void *_cecs_get(cecs_iter_t *it, cecs_entity_t *entity, cecs_component_id_t id, size_t size)
{
  (void)it;
  (void)entity;
  (void)id;
  (void)size;
  return NULL;
}


cecs_entity_t _cecs_create(cecs_component_id_t n, ...)
{
  va_list components;
  va_start(components, n);
  cecs_sig_t sig = components_to_sig(n, components);
  va_end(components);

  cecs_entity_t entity = n_entities++;

  cecs_archetype_t *archetype = get_or_add_archetype(&sig);
  
  add_entity_to_archetype(entity, archetype);

  archetypes_by_entity[entity] = archetype;

  return entity;
}


cecs_archetype_t *cecs_add(cecs_entity_t entity, cecs_component_id_t n, ...)
{
  va_list components;

  cecs_archetype_t *prev_archetype = archetypes_by_entity[entity];
  cecs_sig_t sig = prev_archetype->sig;

  va_start(components, n);
  cecs_sig_t sig_to_add = components_to_sig(n, components);
  va_end(components);

  /* Union the current components with the new components */
  sig_union(&sig, &sig_to_add);

  if (sigs_are_equal(&sig, &sig_to_add)) {
    /* The entity already contains all the given components, do nothing */
    return prev_archetype;
  }

  cecs_archetype_t *new_archetype = get_or_add_archetype(&sig);

  remove_entity_from_archetype(entity, prev_archetype);
  add_entity_to_archetype(entity, new_archetype);

  return new_archetype;
}


cecs_archetype_t *cecs_remove(cecs_entity_t entity, cecs_component_id_t n, ...)
{
  va_list components;

  cecs_archetype_t *prev_archetype = archetypes_by_entity[entity];
  cecs_sig_t sig = prev_archetype->sig;

  va_start(components, n);
  cecs_sig_t sig_to_remove = components_to_sig(n, components);
  va_end(components);

  sig_remove(&sig, &sig_to_remove);

  cecs_archetype_t *new_archetype = get_or_add_archetype(&sig);

  remove_entity_from_archetype(entity, prev_archetype);
  add_entity_to_archetype(entity, new_archetype);

  return new_archetype;
}