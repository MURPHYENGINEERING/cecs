#include <assert.h>
#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cecs.h>


cecs_component_id_t CECS_NEXT_COMPONENT_ID = (cecs_component_id_t)0u;

/** This is where the actual entity data gets stored as arrays of entity IDs per
 * archetype */
static cecs_archetype_t archetypes[CECS_N_ARCHETYPES] = { 0 };
static size_t n_archetypes                            = 0;


static cecs_entity_t g_next_entity = CECS_ENTITY_INVALID + 1ull;


struct archetype_by_entity_entry {
  cecs_entity_t entity;
  cecs_archetype_t *archetype;
};


struct archetype_by_entity_list {
  size_t count;
  size_t cap;
  struct archetype_by_entity_entry *entries;
};

#define N_ARCHETYPE_BY_ENTITY_BUCKETS ((size_t)4096ull)
/** Map from entity ID to the archetype it implements */
static struct archetype_by_entity_list archetypes_by_entity[N_ARCHETYPE_BY_ENTITY_BUCKETS]
  = { 0 };



cecs_entity_t cecs_iter_next(cecs_iter_t *it)
{
  /* Check if we're at the end of the list before inspecting the current bucket */
  if (it->i_bucket == N_ARCHETYPE_BY_ENTITY_BUCKETS) {
    return CECS_ENTITY_INVALID;
  }

  if (it->i_entity == it->set.buckets[it->i_bucket].count) {
    /* Skip empty buckets until we reach a non-empty one or the end of the set */
    while ((it->i_bucket < N_ARCHETYPE_BY_ENTITY_BUCKETS)
           && (it->set.buckets[it->i_bucket].count == 0)) {
      ++it->i_bucket;
    }

    /* Restart at the beginning of the new bucket */
    it->i_entity = 0ull;
  }

  /* Check if we skipped empty buckets all the way to the end of the list */
  if (it->i_bucket == N_ARCHETYPE_BY_ENTITY_BUCKETS) {
    return CECS_ENTITY_INVALID;
  }

  /* Iterate the bucket */
  return it->set.buckets[it->i_bucket].entities[it->i_entity++];
}


static bool entity_set_put(struct cecs_entity_set *set, const cecs_entity_t entity)
{
  size_t i_bucket                  = (size_t)entity % N_ENTITY_SET_BUCKETS;
  struct cecs_entity_set_bucket *bucket = &set->buckets[i_bucket];

  /* Check for duplicates */
  for (size_t i = 0; i < bucket->count; ++i) {
    if (bucket->entities[i] == entity) {
      return false;
    }
  }

  /* Check if we need to expand the bucket */
  if (bucket->count >= bucket->cap) {
    if (bucket->cap == 0ull) {
      bucket->cap = 32ull;
    }
    bucket->cap *= 2ull;
    bucket->entities = realloc(bucket->entities, bucket->cap * sizeof(cecs_entity_t));
  }

  /* Add an entry to the end of the bucket and increase the count */
  bucket->entities[bucket->count++] = entity;

  return true;
}


static void set_archetype_by_entity(const cecs_entity_t entity, cecs_archetype_t *archetype)
{
  size_t i_bucket = (size_t)entity % N_ARCHETYPE_BY_ENTITY_BUCKETS;
  struct archetype_by_entity_list *list = &archetypes_by_entity[i_bucket];

  /* If entity already exists in bucket, just overwrite its value */
  for (size_t i = 0; i < list->count; ++i) {
    struct archetype_by_entity_entry *entry = &list->entries[i];
    if (entry->entity == entity) {
      entry->archetype = archetype;
      return;
    }
  }

  if (list->count >= list->cap) {
    if (list->cap == 0) {
      list->cap = 32u;
    }
    list->cap *= 2ull;
    list->entries
      = realloc(list->entries, list->cap * sizeof(struct archetype_by_entity_entry));
  }

  /* Entity wasn't found in bucket; add a new entry to the bucket list */
  struct archetype_by_entity_entry *entry = &list->entries[list->count++];
  entry->entity                           = entity;
  entry->archetype                        = archetype;
}


static cecs_archetype_t *get_archetype_by_entity(const cecs_entity_t entity)
{
  size_t i_bucket = (size_t)entity % N_ARCHETYPE_BY_ENTITY_BUCKETS;
  struct archetype_by_entity_list *list = &archetypes_by_entity[i_bucket];

  for (size_t i = 0; i < list->count; ++i) {
    if (list->entries[i].entity == entity) {
      return list->entries[i].archetype;
    }
  }

  return NULL;
}


static bool sigs_are_equal(const cecs_sig_t *lhs, const cecs_sig_t *rhs)
{
  for (cecs_component_id_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if (lhs->components[i] != rhs->components[i]) {
      return false;
    }
  }

  return true;
}


static void sig_union(cecs_sig_t *target, const cecs_sig_t *with)
{
  for (cecs_component_id_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    target->components[i] |= with->components[i];
  }
}


static void sig_remove(cecs_sig_t *target, const cecs_sig_t *to_remove)
{
  for (cecs_component_id_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    target->components[i] &= ~to_remove->components[i];
  }
}


static cecs_sig_t components_to_sig(const cecs_component_id_t n, va_list components)
{
  cecs_sig_t sig;
  memset(&sig, 0u, sizeof(sig));

  for (size_t i = 0; i < n; ++i) {
    cecs_component_id_t id = va_arg(components, cecs_component_id_t);
    CECS_ADD_COMPONENT(&sig, id);
  }

  return sig;
}


static void add_entity_to_archetype(const cecs_entity_t entity, cecs_archetype_t *archetype)
{
  size_t i_entity = archetype->n_entities++;
  if (archetype->n_entities >= archetype->cap_entities) {
    if (archetype->cap_entities == 0) {
      archetype->cap_entities = 512ull;
    }
    archetype->cap_entities *= 2;
    archetype->entities
      = realloc(archetype->entities, archetype->cap_entities * sizeof(cecs_entity_t));
  }

  archetype->entities[i_entity] = entity;
  set_archetype_by_entity(entity, archetype);
}


static void remove_entity_from_archetype(const cecs_entity_t entity, cecs_archetype_t *archetype)
{
  for (cecs_entity_t i = 0; i < archetype->n_entities; ++i) {
    if (archetype->entities[i] == entity) {
      /* Put the last entity in the array into the removed entity's slot */
      archetype->entities[i] = archetype->entities[--archetype->n_entities];
      return;
    }
  }
}


static cecs_archetype_t *get_or_add_archetype(const cecs_sig_t *sig)
{
  for (size_t i = 0; i < n_archetypes; ++i) {
    if (sigs_are_equal(&archetypes[i].sig, sig)) {
      return &archetypes[i];
    }
  }

  archetypes[n_archetypes].sig = *sig;
  return &archetypes[n_archetypes++];
}


static cecs_archetype_t *get_archetype_by_component(cecs_component_id_t id)
{
  (void)id;
  return NULL;
}


cecs_entity_t _cecs_query(cecs_iter_t *it, const cecs_component_id_t n, ...)
{
  /* For each archetype, check if its signature &s with the requested signature.
     If it does, that means the archetype contains entities with that
     component.

     Create a set to contain all entities exactly once, for iteration over the
     query.

     How do we get the same component data whether it was requested through one
     archetype vs another archetype?

     Two archetypes with the same component may both contain copies of that
     component data. This is incorrect.

     We need a component data map from entity ID to component data.
     This should be organized so that all entity component data is contiguous in
     memory. I.e., all positions are together, all velocities are together, etc.

     We can create an array of stride __cecs_xxx_size and create a map from
     entity to pointer into that array.

     We then have a free list for when components are removed from entities, we
     add that entity's component pointer to the free list.

     We can now get a component for an entity without a query.

     The query can just return a set of entities, composed from the results of
     the different archetypes.
  */
  va_list components;

  va_start(components, n);
  cecs_sig_t sig = components_to_sig(n, components);
  va_end(components);

  // TODO: Get multiple archetypes based on component signature
  //it.archetype = get_or_add_archetype(&sig);

  //it.first = &it.archetype->entities[0];
  //it.end   = &it.archetype->entities[it.archetype->n_entities];

  memset(&it, 0u, sizeof(it));

  cecs_entity_t n_entities = 0ull;

  for (cecs_component_id_t id = 0; id < CECS_N_COMPONENTS; ++id) {
    if (CECS_HAS_COMPONENT(&sig, id)) {
      cecs_archetype_t *archetype = get_archetype_by_component(id);
      for (size_t i = 0; i < archetype->n_entities; ++i) {
        if (entity_set_put(&it->set, archetype->entities[i])) {
          ++n_entities;
        }
      }
    }
  }

  return n_entities;
}


void *_cecs_get(cecs_entity_t entity, cecs_component_id_t id, size_t size)
{
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

  cecs_entity_t entity = g_next_entity++;

  cecs_archetype_t *archetype = get_or_add_archetype(&sig);

  add_entity_to_archetype(entity, archetype);

  return entity;
}


cecs_archetype_t *_cecs_add(cecs_entity_t entity, cecs_component_id_t n, ...)
{
  va_list components;

  cecs_archetype_t *prev_archetype = get_archetype_by_entity(entity);
  assert(
    prev_archetype != NULL && "Can't add component to entity that doesn't exist"
  );
  cecs_sig_t sig = prev_archetype->sig;

  va_start(components, n);
  cecs_sig_t sig_to_add = components_to_sig(n, components);
  va_end(components);

  sig_union(&sig, &sig_to_add);

  cecs_archetype_t *new_archetype = get_or_add_archetype(&sig);

  if (new_archetype == prev_archetype) {
    return prev_archetype;
  }

  remove_entity_from_archetype(entity, prev_archetype);
  add_entity_to_archetype(entity, new_archetype);

  return new_archetype;
}


cecs_archetype_t *_cecs_remove(cecs_entity_t entity, cecs_component_id_t n, ...)
{
  va_list components;

  cecs_archetype_t *prev_archetype = get_archetype_by_entity(entity);
  assert(
    prev_archetype != NULL
    && "Can't remove component from entity that doesn't exist"
  );
  cecs_sig_t sig = prev_archetype->sig;

  va_start(components, n);
  cecs_sig_t sig_to_remove = components_to_sig(n, components);
  va_end(components);

  sig_remove(&sig, &sig_to_remove);

  cecs_archetype_t *new_archetype = get_or_add_archetype(&sig);

  if (new_archetype == prev_archetype) {
    return prev_archetype;
  }

  remove_entity_from_archetype(entity, prev_archetype);
  add_entity_to_archetype(entity, new_archetype);

  return new_archetype;
}