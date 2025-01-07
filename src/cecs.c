#include <assert.h>
#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cecs.h>


cecs_component_t CECS_NEXT_COMPONENT_ID = (cecs_component_t)0u;

static cecs_entity_t g_next_entity = CECS_ENTITY_INVALID + 1u;


struct sig_by_entity_entry {
  cecs_entity_t entity;
  cecs_sig_t sig;
};


struct sig_by_entity_bucket {
  size_t count;
  size_t cap;
  struct sig_by_entity_entry *entries;
};

#define N_SIG_BY_ENTITY_BUCKETS ((size_t)4096u)
/** Map from entity ID to the signature it implements */
static struct sig_by_entity_bucket sigs_by_entity[N_SIG_BY_ENTITY_BUCKETS] = { 0u };


struct component_by_id_entry {
  cecs_component_t id;
  size_t size;
  void *data;
};

struct component_by_id_bucket {
  size_t count;
  size_t cap;
  struct component_by_id_entry *entries;
};

#define N_COMPONENT_BY_ID_BUCKETS ((size_t)4096u)
static struct component_by_id_bucket components_by_id[N_COMPONENT_BY_ID_BUCKETS] = { 0u };


cecs_entity_t cecs_iter_next(cecs_iter_t *it)
{
  /* Check if we're at the end of the bucket before inspecting the current bucket */
  if (it->i_bucket == N_ENTITY_SET_BUCKETS) {
    return CECS_ENTITY_INVALID;
  }

  if (it->i_entity >= it->set.buckets[it->i_bucket].count) {
    /* Skip empty buckets until we reach a non-empty one or the end of the set */
    while ((it->i_bucket < N_ENTITY_SET_BUCKETS)
           && (it->set.buckets[it->i_bucket].count == 0)) {
      ++it->i_bucket;
    }

    /* Restart at the beginning of the new bucket */
    it->i_entity = (size_t)0u;
  }

  /* Check if we skipped empty buckets all the way to the end of the bucket,
     or if the bucket we skipped to is empty (redundant?) */
  if (it->i_bucket == N_ENTITY_SET_BUCKETS) {
    return CECS_ENTITY_INVALID;
  }

  /* Iterate the bucket */
  return it->set.buckets[it->i_bucket].entities[it->i_entity++];
}


static bool entity_set_put(struct cecs_entity_set *set, const cecs_entity_t entity)
{
  size_t i_bucket                       = (size_t)entity % N_ENTITY_SET_BUCKETS;
  struct cecs_entity_set_bucket *bucket = &set->buckets[i_bucket];

  /* Check for duplicates */
  for (size_t i = 0; i < bucket->count; ++i) {
    if (bucket->entities[i] == entity) {
      return false;
    }
  }

  /* Check if we need to expand the bucket */
  if (bucket->count >= bucket->cap) {
    if (bucket->cap == 0u) {
      bucket->cap = 32u;
    }
    bucket->cap *= 2u;
    bucket->entities = realloc(bucket->entities, bucket->cap * sizeof(cecs_entity_t));
  }

  /* Add an entry to the end of the bucket and increase the count */
  bucket->entities[bucket->count++] = entity;

  return true;
}


static void set_sig_by_entity(const cecs_entity_t entity, cecs_sig_t *sig)
{
  size_t i_bucket = (size_t)entity % N_SIG_BY_ENTITY_BUCKETS;
  struct sig_by_entity_bucket *bucket = &sigs_by_entity[i_bucket];

  /* If entity already exists in bucket, just overwrite its value */
  for (size_t i = 0; i < bucket->count; ++i) {
    struct sig_by_entity_entry *entry = &bucket->entries[i];
    if (entry->entity == entity) {
      entry->sig = *sig;
      return;
    }
  }

  /* Grow the bucket if needed */
  if (bucket->count >= bucket->cap) {
    if (bucket->cap == 0) {
      bucket->cap = 32u;
    }
    bucket->cap *= 2u;
    bucket->entries
      = realloc(bucket->entries, bucket->cap * sizeof(struct sig_by_entity_entry));
  }

  /* Entity wasn't found in bucket; add a new entry to the bucket bucket */
  struct sig_by_entity_entry *entry = &bucket->entries[bucket->count++];

  entry->entity = entity;
  entry->sig    = *sig;
}


static cecs_sig_t *get_sig_by_entity(const cecs_entity_t entity)
{
  size_t i_bucket = (size_t)entity % N_SIG_BY_ENTITY_BUCKETS;
  struct sig_by_entity_bucket *bucket = &sigs_by_entity[i_bucket];

  for (size_t i = 0; i < bucket->count; ++i) {
    if (bucket->entries[i].entity == entity) {
      return &bucket->entries[i].sig;
    }
  }

  return NULL;
}


static bool sigs_are_equal(const cecs_sig_t *lhs, const cecs_sig_t *rhs)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if (lhs->components[i] != rhs->components[i]) {
      return false;
    }
  }

  return true;
}


static void set_component_by_id(const cecs_component_t id, const size_t size, void *data)
{
  size_t i_bucket                       = (size_t)id % N_SIG_BY_ENTITY_BUCKETS;
  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  /* If entity already exists in bucket, just overwrite its value */
  for (size_t i = 0; i < bucket->count; ++i) {
    struct component_by_id_entry *entry = &bucket->entries[i];
    if (entry->id == id) {
      entry->size = size;
      return;
    }
  }

  /* Grow the bucket if needed */
  if (bucket->count >= bucket->cap) {
    if (bucket->cap == 0) {
      bucket->cap = 32u;
    }
    bucket->cap *= 2u;
    bucket->entries
      = realloc(bucket->entries, bucket->cap * sizeof(struct sig_by_entity_entry));
  }

  /* Entity wasn't found in bucket; add a new entry to the bucket bucket */
  struct component_by_id_entry *entry = &bucket->entries[bucket->count++];

  entry->id   = id;
  entry->size = size;
  entry->data = data;
}


static size_t get_component_by_id(const cecs_component_t id, void **data)
{
  size_t i_bucket = (size_t)id % N_COMPONENT_BY_ID_BUCKETS;
  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  for (size_t i = 0; i < bucket->count; ++i) {
    if (bucket->entries[i].id == id) {
      *data = bucket->entries[i].data;
    }
  }

  return 0u;
}


static void sig_union(cecs_sig_t *target, const cecs_sig_t *with)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    target->components[i] |= with->components[i];
  }
}


static void sig_remove(cecs_sig_t *target, const cecs_sig_t *to_remove)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    target->components[i] &= ~to_remove->components[i];
  }
}


static cecs_sig_t components_to_sig(const cecs_component_t n, va_list components)
{
  cecs_sig_t sig;
  memset(&sig, 0u, sizeof(sig));

  for (size_t i = 0; i < n; ++i) {
    cecs_component_t id = va_arg(components, cecs_component_t);
    CECS_ADD_COMPONENT(&sig, id);
  }

  return sig;
}


cecs_entity_t _cecs_query(cecs_iter_t *it, const cecs_component_t n, ...)
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

     We then have a free bucket for when components are removed from entities,
     we add that entity's component pointer to the free bucket.

     We can now get a component for an entity without a query.

     The query can just return a set of entities, composed from the results of
     the different archetypes.
  */
  va_list components;

  va_start(components, n);
  cecs_sig_t sig = components_to_sig(n, components);
  va_end(components);

  memset(it, 0u, sizeof(*it));

  cecs_entity_t n_entities = 0u;

  for (cecs_component_t id = 0; id < CECS_N_COMPONENTS; ++id) {
    if (CECS_HAS_COMPONENT(&sig, id)) {
    }
  }

  return n_entities;
}


void *_cecs_get(cecs_entity_t entity, cecs_component_t id, size_t size)
{
  (void)entity;
  (void)id;
  (void)size;
  return NULL;
}


cecs_entity_t _cecs_create(cecs_component_t n, ...)
{
  va_list components;
  va_start(components, n);
  cecs_sig_t sig = components_to_sig(n, components);
  va_end(components);

  cecs_entity_t entity = g_next_entity++;

  set_sig_by_entity(entity, &sig);

  return entity;
}


void _cecs_add(cecs_entity_t entity, cecs_component_t n, ...)
{
  va_list components;

  cecs_sig_t *sig = get_sig_by_entity(entity);

  va_start(components, n);
  cecs_sig_t sig_to_add = components_to_sig(n, components);
  va_end(components);

  sig_union(sig, &sig_to_add);
}


void _cecs_remove(cecs_entity_t entity, cecs_component_t n, ...)
{
  va_list components;

  cecs_sig_t *sig = get_sig_by_entity(entity);

  va_start(components, n);
  cecs_sig_t sig_to_remove = components_to_sig(n, components);
  va_end(components);

  sig_remove(sig, &sig_to_remove);
}