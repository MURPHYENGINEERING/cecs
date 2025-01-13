#include <assert.h>
#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cecs/cecs.h>


#define __always_inline __attribute__((__always_inline__)) inline


/** Convert a component ID to the LSB into the appropriate index into the
 * signature bit field array. */
#define CECS_COMPONENT_TO_LSB(component) \
  ((component) % ((cecs_component_t)8u * sizeof(cecs_component_t)))

/** Convert a component ID to the bit pattern representing it in the
 * appropriate index into the signature bit field array. */
#define CECS_COMPONENT_TO_BITS(component) \
  ((cecs_component_t)1u << CECS_COMPONENT_TO_LSB(component))


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


/** The next component ID to be assigned on registration */
cecs_component_t CECS_NEXT_COMPONENT_ID = (cecs_component_t)1u;

/** The next entity to be assigned on creation */
cecs_entity_t g_next_entity = CECS_ENTITY_INVALID + 1u;


/** A signature represents the components implemented by a type as a bit set. */
struct signature {
  cecs_component_t components[CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS)];
};


/** entity->signature pair */
struct sig_by_entity_entry {
  cecs_entity_t entity;
  struct signature sig;
};


/** Vector of entity->signature pairs */
struct sig_by_entity_bucket {
  size_t count;
  size_t cap;
  struct sig_by_entity_entry *pairs;
};

/** Minimum number of elements allocated for an entity->signature map bucket */
#define SIG_BY_ENTITY_MIN_BUCKET_SIZE ((size_t)32u)
/** Number of buckets in the entity->signature map */
#define N_SIG_BY_ENTITY_BUCKETS ((size_t)16384u)
/** Map from entity ID to the signature it implements */
struct sig_by_entity_bucket sigs_by_entity[N_SIG_BY_ENTITY_BUCKETS] = { 0u };


/** Vector of entity IDs */
struct entity_vec {
  size_t count;
  size_t cap;
  cecs_entity_t *entities;
};


/** Vector of free indices to be used when adding new entities to components */
struct index_vec {
  size_t count;
  size_t cap;
  size_t *indices;
};


/** Entity ID->index into component data array */
struct index_by_entity_pair {
  cecs_entity_t entity;
  size_t index;
};

/** Vector of entity ID->index pairs */
struct index_by_entity_bucket {
  size_t count;
  size_t cap;
  /* Stores the singulate pair when count == 1 */
  struct index_by_entity_pair value;
  struct index_by_entity_pair *pairs;
};

/** Minimum number of elements allocated for an entity->index map bucket */
#define INDEX_BY_ENTITY_MIN_BUCKET_SIZE ((size_t)8u)
/** Number of buckets in the entity ID->index map */
#define N_INDICES_BY_ENTITY_BUCKETS ((size_t)16384u)


/** ID->component data and entity vec pairs */
struct component_by_id_table {
  cecs_component_t id;
  size_t size;
  void *data;
  size_t cap;
  size_t count;
  struct index_by_entity_bucket indices_by_entity[N_INDICES_BY_ENTITY_BUCKETS];
  struct index_vec free_indices;
};

/** Vector of ID->component pairs, to be stored in the id->component map */
struct component_by_id_bucket {
  size_t count;
  size_t cap;
  struct component_by_id_table *pairs;
};

/** Minimum number of elements allocated for component data by index */
#define COMPONENT_TABLE_MIN_DATA_SIZE ((size_t)16384u)
/** Minimum number of elements allocated for a component ID->component map bucket */
#define COMPONENT_BY_ID_MIN_BUCKET_SIZE ((size_t)8u)
/** Number of buckets in the componet ID->data map */
#define N_COMPONENT_BY_ID_BUCKETS ((size_t)1024u)
/** Map from component ID to component data and implementing entities vec */
struct component_by_id_bucket components_by_id[N_COMPONENT_BY_ID_BUCKETS] = { 0u };


/** An archetype is a unique composition of components. */
struct archetype {
  /** Component signature implemented by this archetype */
  struct signature sig;
  /** Number of entities implementing this archetype */
  size_t count;
  /** Number of entities able to be stored in the entities array before resizing */
  size_t cap;
  /** Array of entities implementing this archetype */
  cecs_entity_t *entities;
};

/** Signature->Archetype to be stored in the sig->archetype map */
struct archetype_by_sig_entry {
  struct signature sig;
  struct archetype archetype;
};

/** Vector of signature->archetype pairs, to be stored in the sig->archetype map */
struct archetype_by_sig_bucket {
  size_t count;
  size_t cap;
  struct archetype_by_sig_entry *pairs;
};

/** Vector of pointers to archetypes, which are stored in the sig->archetype map */
struct achetype_vec {
  size_t count;
  size_t cap;
  struct archetype **elements;
};

/** Minimum number of elements to allocate for an entity vec for a given archetype */
#define ARCHETYPE_ENTITIES_VEC_MIN_SIZE ((size_t)16384u)
/** Minimum number of elements allocated for a vec of archetypes */
#define ARCHETYPES_VEC_MIN_SIZE ((size_t)32u)
/** Minimum number of elements allocated for a signature->archetype map bucket */
#define ARCHETYPES_BY_SIG_MIN_BUCKET_SIZE ((size_t)32u)
/** Number of buckets in the signature->archetype map */
#define N_ARCHETYPE_BY_SIG_BUCKETS ((size_t)256u)
/** Map from signature to the archetype it represents */
struct archetype_by_sig_bucket archetypes_by_sig[N_ARCHETYPE_BY_SIG_BUCKETS] = { 0u };


/** Vector of entities in a bucket in the entity set */
struct cecs_entity_set_bucket {
  size_t count;
  size_t cap;
  /* Holds the value of the bucket when count == 1 */
  cecs_entity_t value;
  cecs_entity_t *entities;
};

/** Number of buckets in an entity set */
#define N_ENTITY_SET_BUCKETS ((size_t)16384u)
/** Set of unique entity IDs */
struct cecs_entity_set {
  struct cecs_entity_set_bucket buckets[N_ENTITY_SET_BUCKETS];
};


/** Minmum number of elements allocated for an entity set bucket */
#define ENTITY_SET_MIN_BUCKET_SIZE ((size_t)8u)
/** Cache the set used to return query result so we don't have to allocate
 * buckets on every query */
struct cecs_entity_set query_result_cache;

/** Cache the vec of archetypes returned by a query so we don't have to
 * allocate on every query */
struct achetype_vec archetypes_vec_cache;


/** Grow the given vec to the minimum size if empty, or double its size */
#define GROW_VEC_IF_NEEDED(vec, min_size, entries, type)                           \
  if ((vec)->count >= (vec)->cap) {                                                \
    if ((vec)->cap == 0u) {                                                         \
      (vec)->cap     = (min_size);                                                  \
      (vec)->entries = realloc((vec)->entries, (vec)->cap * sizeof(type));        \
      memset((uint8_t *)(vec)->entries, 0u, (vec)->cap * sizeof(type));            \
    } else {                                                                         \
      (vec)->entries = realloc((vec)->entries, ((vec)->cap * 2u) * sizeof(type)); \
      memset(                                                                        \
        ((uint8_t *)(vec)->entries) + (vec)->cap * sizeof(type),                   \
        0u,                                                                          \
        (vec)->cap * sizeof(type)                                                   \
      );                                                                             \
      (vec)->cap *= 2u;                                                             \
    }                                                                                \
  }


/** Find the entry in vec `bucket` whose `identifier` matches `search` and
 * return it in `result` */
#define FIND_ENTRY_IN_BUCKET(bucket, identifier, search, result) \
  for (size_t _i = 0u; _i < (bucket)->count; ++_i) {              \
    if ((bucket)->pairs[_i].identifier == (search)) {            \
      (result) = &(bucket)->pairs[_i];                           \
    }                                                            \
  }


/** Returns true if the two signatures are equivalent */
static bool __always_inline sigs_are_equal(const struct signature *lhs, const struct signature *rhs)
{
  for (cecs_component_t i = 0u; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if (lhs->components[i] != rhs->components[i]) {
      return false;
    }
  }

  return true;
}


/** Returns true if `look_for` is entirely represented by `in` */
static bool __always_inline sig_is_in(const struct signature *look_for, const struct signature *in)
{
  for (cecs_component_t i = 0u; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if ((look_for->components[i] & in->components[i]) != look_for->components[i]) {
      return false;
    }
  }

  return true;
}


/** Return the boolean OR union of the two signatures */
static void __always_inline sig_union(struct signature *target, const struct signature *with)
{
  for (cecs_component_t i = 0u; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    target->components[i] |= with->components[i];
  }
}


/** Return `target` less all the components in `to_remove` */
static void __always_inline sig_remove(struct signature *target, const struct signature *to_remove)
{
  for (cecs_component_t i = 0u; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    target->components[i] &= ~to_remove->components[i];
  }
}


/** Generate a signature reprensenting all the components in the given vec */
static struct signature components_to_sig(const cecs_component_t n, va_list components)
{
  struct signature sig;
  memset(&sig, 0u, sizeof(sig));

  for (size_t i = 0u; i < n; ++i) {
    cecs_component_t id = va_arg(components, cecs_component_t);
    assert(id > 0 && "Component was not registered with CECS_COMPONENT()");
    CECS_ADD_COMPONENT(&sig, id);
  }

  return sig;
}


/** Get all the archetypes that contain the given signature.
 * The caller is responsible for freeing the returned vec. */
static struct achetype_vec *get_archetypes_by_sig(const struct signature *sig)
{
  struct achetype_vec *vec = &archetypes_vec_cache;
  /* Clear previous results */
  vec->count = 0u;

  for (size_t i_bucket = 0u; i_bucket < N_ARCHETYPE_BY_SIG_BUCKETS; ++i_bucket) {
    struct archetype_by_sig_bucket *bucket = &archetypes_by_sig[i_bucket];
    for (size_t i_entry = 0u; i_entry < bucket->count; ++i_entry) {
      struct archetype_by_sig_entry *entry = &bucket->pairs[i_entry];
      if (sig_is_in(sig, &entry->sig)) {
        GROW_VEC_IF_NEEDED(vec, ARCHETYPES_VEC_MIN_SIZE, elements, struct archetype *);
        vec->elements[vec->count++] = &entry->archetype;
      }
    }
  }

  return vec;
}


/** Populate the sig->archetype map by assigning the value pointed to by the
 * given archetype pointer to the map value */
static struct archetype *set_archetype_by_sig(const struct signature *sig, const struct archetype *archetype)
{
  size_t i_bucket = 0u;
  for (size_t i = 0u; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    i_bucket ^= (size_t)(sig->components[i] % N_ARCHETYPE_BY_SIG_BUCKETS);
  }
  struct archetype_by_sig_bucket *bucket = &archetypes_by_sig[i_bucket];

  /* If entity already exists in bucket, just overwrite its value */
  for (size_t i = 0u; i < bucket->count; ++i) {
    struct archetype_by_sig_entry *entry = &bucket->pairs[i];
    if (sigs_are_equal(&entry->sig, sig)) {
      entry->archetype = *archetype;
      return &entry->archetype;
    }
  }

  /* Entity wasn't found in bucket. Check if we need to expand the bucket first
   */
  GROW_VEC_IF_NEEDED(bucket, ARCHETYPES_BY_SIG_MIN_BUCKET_SIZE, pairs, struct archetype_by_sig_entry);

  /* Add a new entry to the bucket */
  struct archetype_by_sig_entry *entry = &bucket->pairs[bucket->count++];

  entry->sig       = *sig;
  entry->archetype = *archetype;

  return &entry->archetype;
}


/** Return the archetype implementing the given signature */
static struct archetype *get_archetype_by_sig(const struct signature *sig)
{
  size_t i_bucket = 0u;
  for (size_t i = 0u; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    i_bucket ^= (size_t)(sig->components[i] % N_ARCHETYPE_BY_SIG_BUCKETS);
  }
  struct archetype_by_sig_bucket *bucket = &archetypes_by_sig[i_bucket];

  for (size_t i = 0u; i < bucket->count; ++i) {
    if (sigs_are_equal(&bucket->pairs[i].sig, sig)) {
      return &bucket->pairs[i].archetype;
    }
  }

  return NULL;
}


/** Get the archetype implementing the given signature, or add an empty one and
 * return that */
static struct archetype *get_or_add_archetype_by_sig(const struct signature *sig)
{
  struct archetype *existing = get_archetype_by_sig(sig);
  if (existing) {
    return existing;
  }

  struct archetype atype;
  memset(&atype, 0u, sizeof(atype));
  atype.sig = *sig;

  return set_archetype_by_sig(sig, &atype);
}


/** Populate the entity->signature map */
static void set_sig_by_entity(const cecs_entity_t entity, struct signature *sig)
{
  const size_t i_bucket = (size_t)(entity % N_SIG_BY_ENTITY_BUCKETS);
  struct sig_by_entity_bucket *bucket = &sigs_by_entity[i_bucket];

  /* If entity already exists in bucket, just overwrite its value */
  for (size_t i = 0u; i < bucket->count; ++i) {
    struct sig_by_entity_entry *entry = &bucket->pairs[i];
    if (entry->entity == entity) {
      entry->sig = *sig;
      return;
    }
  }

  /* Entity wasn't found in bucket; add a new entry to the bucket bucket */
  GROW_VEC_IF_NEEDED(bucket, SIG_BY_ENTITY_MIN_BUCKET_SIZE, pairs, struct sig_by_entity_entry);
  struct sig_by_entity_entry *entry = &bucket->pairs[bucket->count++];

  entry->entity = entity;
  entry->sig    = *sig;
}


/** Get the signature implemented by the given entity */
static struct signature *get_sig_by_entity(const cecs_entity_t entity)
{
  const size_t i_bucket = (size_t)(entity % N_SIG_BY_ENTITY_BUCKETS);

  struct sig_by_entity_bucket *bucket = &sigs_by_entity[i_bucket];

  struct sig_by_entity_entry *entry = NULL;
  FIND_ENTRY_IN_BUCKET(bucket, entity, entity, entry);

  if (entry) {
    return &entry->sig;
  } else {
    return NULL;
  }
}


/** Add a block of component data for the given entity */
static void add_entity_to_component(const cecs_component_t id, const cecs_entity_t entity)
{
  assert(id > 0 && "Component was not registered with CECS_COMPONENT()");

  const size_t i_bucket = (size_t)(id % N_COMPONENT_BY_ID_BUCKETS);
  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  struct component_by_id_table *table = NULL;
  FIND_ENTRY_IN_BUCKET(bucket, id, id, table);

  if (!table) {
    /* Component ID not found in bucket, */
    GROW_VEC_IF_NEEDED(bucket, COMPONENT_BY_ID_MIN_BUCKET_SIZE, pairs, struct component_by_id_table);
    table = &bucket->pairs[bucket->count++];
  }

  const size_t i_index_bucket = (size_t)(entity % N_INDICES_BY_ENTITY_BUCKETS);
  struct index_by_entity_bucket *index_bucket = &table->indices_by_entity[i_index_bucket];

  size_t index = (size_t)0u;

  /* Get an index into the component data table for this entity. */
  if (table->free_indices.count > 0) {
    /* There is a free slot due to another entity being removed from the
     * component; use that. */
    index = table->free_indices.indices[--table->free_indices.count];
  } else {
    /* No free slots, add a slot to the data table */
    index = table->count++;
  }

  if (index_bucket->count == 0u) {
    /* Bucket is empty, put the index-by-entity pair in the singulate value */
    index_bucket->value.entity = entity;
    index_bucket->value.index  = index;
    index_bucket->count        = 1u;

  } else {
    struct index_by_entity_pair *index_pair = NULL;
    if (index_bucket->pairs) {
      /* Bucket vec is already allocated, check it for duplicates */
      FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_pair);
      /* Don't allow duplicates */
      if (index_pair) {
        return;
      }
    } else {
      /* Bucket vec is not yet allocated, check if the singulate value is a
       * duplicate */
      if (index_bucket->value.entity == entity) {
        return;
      }
    }

    /* No duplicates, found, add entity to component */
    GROW_VEC_IF_NEEDED(index_bucket, INDEX_BY_ENTITY_MIN_BUCKET_SIZE, pairs, struct index_by_entity_pair);

    if (index_bucket->count == 1u) {
      /* Transitioning from 0 to 1 elements. Put the singulate value in the vec */
      index_bucket->pairs[0u] = index_bucket->value;
    }

    /* Add the new pair to the end of the bucket */
    index_pair         = &index_bucket->pairs[index_bucket->count++];
    index_pair->entity = entity;
    index_pair->index  = index;
  }

  /* Grow the component table if needed */
  if (table->count >= table->cap) {
    if (table->cap == 0u) {
      /* Allocate for the first time */
      table->cap  = COMPONENT_TABLE_MIN_DATA_SIZE;
      table->data = realloc(table->data, table->cap * table->size);
      assert(table->data && "Out of memory");
#ifdef CECS_ZERO_COMPONENT_DATA
      memset(table->data, 0u, table->cap * table->size);
#endif

    } else {
      /* Double the size and zero out the new data capacity */
      table->data = realloc(table->data, table->cap * 2u * table->size);
      assert(table->data && "Out of memory");
#ifdef CECS_ZERO_COMPONENT_DATA
      memset(
        ((uint8_t *)table->data) + table->cap * table->size,
        0u,
        table->cap * table->size
      );
#endif
      table->cap *= 2u;
    }
  }
}


static void remove_entity_from_component(const cecs_component_t id, const cecs_entity_t entity)
{
  assert(id > 0 && "Component was not registered with CECS_COMPONENT()");

  const size_t i_bucket = (size_t)(id % N_COMPONENT_BY_ID_BUCKETS);
  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  struct component_by_id_table *component = NULL;
  FIND_ENTRY_IN_BUCKET(bucket, id, id, component);

  if (!component) {
    /* Component doesn't exist */
    return;
  }

  const size_t i_index_bucket = (size_t)(entity % N_INDICES_BY_ENTITY_BUCKETS);
  struct index_by_entity_bucket *index_bucket
    = &component->indices_by_entity[i_index_bucket];

  if (index_bucket->count == 0u) {
    /* Component doesn't belong to any entities */
    return;
  }

  if (index_bucket->count == 1u) {
    /* Entity is stored in component's singulate value */
    if (index_bucket->value.entity == entity) {
      index_bucket->count = 0u;
      return;
    } else {
      /* Entity doesn't have this component */
      return;
    }
  }

  struct index_by_entity_pair *index_pair = NULL;
  /* Component bucket has entity vec, search it for the entity */
  FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_pair);

  if (!index_pair) {
    /* Entity doesn't have component */
    return;
  }

  /* Grow the free indices vec if needed */
  GROW_VEC_IF_NEEDED(&component->free_indices, 32u, indices, size_t);
  /* Add the removed entity's data index to the free vec */
  component->free_indices.indices[component->free_indices.count++] = index_pair->index;

  if (index_bucket->count > 1u) {
    /* Move the last entry in the bucket into this position, overwriting the
     * removed entity, and decrement the bucket's count */
    memcpy(index_pair, &index_bucket->pairs[--index_bucket->count], sizeof(struct index_by_entity_pair));
  } else {
    /* We don't need to fill any gaps in the bucket, just set its count to zero */
    index_bucket->count = 0u;
  }
}


/** Get the component data for the given component ID */
static struct component_by_id_table *get_component_by_id(const cecs_component_t id)
{
  assert(id > 0 && "Component was not registered with CECS_COMPONENT()");

  size_t i_bucket = (size_t)(id % N_COMPONENT_BY_ID_BUCKETS);
  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  struct component_by_id_table *entry = NULL;
  FIND_ENTRY_IN_BUCKET(bucket, id, id, entry);

  return entry;
}


/** Add the given entity to the specified archetype so we can easily find all
 * entities implementing that archetype */
static void add_entity_to_archetype(const cecs_entity_t entity, struct archetype *archetype)
{
  GROW_VEC_IF_NEEDED(archetype, ARCHETYPE_ENTITIES_VEC_MIN_SIZE, entities, sizeof(cecs_entity_t));

  archetype->entities[archetype->count++] = entity;
}


/** Increment the given entity set iterator */
cecs_entity_t cecs_iter_next(cecs_iter_t *it)
{
  /* Check if we're at the end of the bucket before inspecting the current bucket */
  if (it->i_bucket == N_ENTITY_SET_BUCKETS) {
    return CECS_ENTITY_INVALID;
  }

  const struct cecs_entity_set_bucket *bucket = &it->set->buckets[it->i_bucket];
  if (it->i_entity >= it->set->buckets[it->i_bucket].count) {
    /* Skip empty buckets until we reach a non-empty one or the end of the set */
    while ((++it->i_bucket < N_ENTITY_SET_BUCKETS)
           && (it->set->buckets[it->i_bucket].count == 0)) {
    }

    /* Restart at the beginning of the new bucket */
    it->i_entity = (size_t)0u;
  }

  /* Check if we skipped empty buckets all the way to the end of the bucket,
     or if the bucket we skipped to is empty (redundant?) */
  if (it->i_bucket == N_ENTITY_SET_BUCKETS) {
    return CECS_ENTITY_INVALID;
  }

  bucket = &it->set->buckets[it->i_bucket];
  /* If the bucket has one element then its value is in the bucket directly */
  if (bucket->count == 1u) {
    ++it->i_entity;
    return bucket->value;
  } else {
    /* Iterate the bucket */
    return bucket->entities[it->i_entity++];
  }
}


/** Get an iterator over the entities that implement the given components.
 * Returns the number of entities in the iterator.
 */
cecs_entity_t _cecs_query(cecs_iter_t *it, const cecs_component_t n, ...)
{
  va_list components;
  cecs_entity_t n_entities = 0u;

  /* Use a prebuilt set so we aren't allocating set vectors on every query */
  it->set = &query_result_cache;
  /* Zero out the iterator */
  it->i_bucket = 0u;
  it->i_entity = 0u;

  /* Clear the previous results from the cached set */
  for (size_t i = 0u; i < N_ENTITY_SET_BUCKETS; ++i) {
    it->set->buckets[i].count = 0u;
  }

  va_start(components, n);
  struct signature sig = components_to_sig(n, components);
  va_end(components);

  struct achetype_vec *archetypes = get_archetypes_by_sig(&sig);
  if (archetypes->count == 0u) {
    /* There are no entities with this archetype */
    return 0u;
  }

  for (size_t i_archetype = 0u; i_archetype < archetypes->count; ++i_archetype) {
    struct archetype *archetype = archetypes->elements[i_archetype];
    for (size_t i = 0u; i < archetype->count; ++i) {
      const cecs_entity_t entity = archetype->entities[i];
      const size_t i_bucket      = (size_t)(entity % N_ENTITY_SET_BUCKETS);
      struct cecs_entity_set_bucket *bucket = &it->set->buckets[i_bucket];

      if (bucket->count == 0u) {
        /* The bucket is empty, the first entity can go in the singulate value
         * instead of allocating a vec */
        bucket->value = entity;
        bucket->count = 1u;
        ++n_entities;
        /* Next entity in archetype */
        continue;
      }

      /* Check for duplicates */
      bool exists = false;
      if (bucket->entities) {
        /* The bucket vec is populated, we have to check every single entry */
        for (size_t j = 0u; j < bucket->count; ++j) {
          if (bucket->entities[j] == entity) {
            exists = true;
            break;
          }
        }
      } else {
        /* Bucket vec is not populated */
        exists = bucket->value == entity;
      }
      if (exists) {
        /* Go to next entity in archetype */
        continue;
      }

      GROW_VEC_IF_NEEDED(bucket, ENTITY_SET_MIN_BUCKET_SIZE, entities, cecs_entity_t);
      if (bucket->count == 1u) {
        /* We're transitioning from 1 element to 2 elements, move the
         * singulate value to the vec */
        bucket->entities[0u] = bucket->value;
      }
      /* Add an entry to the end of the bucket and increase the count */
      bucket->entities[bucket->count++] = entity;
      /* Track the total number of entities returned by the query */
      ++n_entities;
    }
  }

  /* If archetypes->elements didn't exist we'd already be out of here */

  return n_entities;
}


/** Get a pointer to the component data for the given entity and component pair */
void *_cecs_get(const cecs_entity_t entity, const cecs_component_t id)
{
  struct component_by_id_table *component = get_component_by_id(id);

  const size_t i_index_bucket = (size_t)(entity % N_INDICES_BY_ENTITY_BUCKETS);
  struct index_by_entity_bucket *index_bucket
    = &component->indices_by_entity[i_index_bucket];

  struct index_by_entity_pair *index_by_entity = NULL;
  if (index_bucket->count == 1u) {
    index_by_entity = &index_bucket->value;
  } else {
    FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_by_entity);
  }

  if (!index_by_entity) {
    /* Entity doesn't have component */
    return NULL;
  }

  return (void *)(((uint8_t *)component->data)
                  + (index_by_entity->index * component->size));
}


/** Create a new entity implementing the given components */
cecs_entity_t _cecs_create(const cecs_component_t n, ...)
{
  va_list components;
  va_start(components, n);
  struct signature sig = components_to_sig(n, components);
  va_end(components);

  const cecs_entity_t entity = g_next_entity++;

  /* Update the entity's cached signature */
  set_sig_by_entity(entity, &sig);

  /* Add the entity to its new archetype */
  add_entity_to_archetype(entity, get_or_add_archetype_by_sig(&sig));

  /* Add the entity to all the named components */
  va_start(components, n);
  for (size_t i = 0u; i < n; ++i) {
    add_entity_to_component(va_arg(components, cecs_component_t), entity);
  }
  va_end(components);

  return entity;
}


/** Add the specified components to the given entity */
void _cecs_add(const cecs_entity_t entity, const cecs_component_t n, ...)
{
  va_list components;

  struct signature *sig      = get_sig_by_entity(entity);
  struct signature sig_added = *sig;

  va_start(components, n);
  struct signature sig_to_add = components_to_sig(n, components);
  va_end(components);

  sig_union(&sig_added, &sig_to_add);

  if (sigs_are_equal(&sig_added, sig)) {
    /* Nothing to do */
    return;
  }

  /* Add the entity to its new archetype */
  add_entity_to_archetype(entity, get_or_add_archetype_by_sig(&sig_added));
  /* Update the cached signature for this entity */
  set_sig_by_entity(entity, &sig_added);

  /* Add the entity to all components named */
  va_start(components, n);
  for (size_t i = 0u; i < n; ++i) {
    add_entity_to_component(va_arg(components, cecs_component_t), entity);
  }
  va_end(components);
}


/** Remove the specified components from the given entity */
void _cecs_remove(const cecs_entity_t entity, const cecs_component_t n, ...)
{
  va_list components;

  /* Get the entity's current signature from the cache */
  struct signature *sig        = get_sig_by_entity(entity);
  struct signature sig_removed = *sig;

  va_start(components, n);
  struct signature sig_to_remove = components_to_sig(n, components);
  va_end(components);

  sig_remove(&sig_removed, &sig_to_remove);

  if (sigs_are_equal(&sig_removed, sig)) {
    /* Nothing to do */
    return;
  }

  /* Remove the entity from its current archetype */
  struct archetype *archetype = get_archetype_by_sig(sig);

  for (cecs_entity_t i = 0u; i < archetype->count; ++i) {
    if (archetype->entities[i] == entity) {
      /* Put the last entity in the array into the removed entity's slot */
      archetype->entities[i] = archetype->entities[--archetype->count];
      break;
    }
  }

  /* Add the entity to its new archetype */
  add_entity_to_archetype(entity, get_or_add_archetype_by_sig(&sig_removed));
  /* Cache the entity's new signature */
  set_sig_by_entity(entity, &sig_removed);

  /* Remove the entity from all components named in the vec */
  va_start(components, n);
  for (size_t i = 0u; i < n; ++i) {
    remove_entity_from_component(va_arg(components, cecs_component_t), entity);
  }
  va_end(components);
}


/** Register the given component as having the specified size */
void cecs_register_component(const cecs_component_t id, const size_t size)
{
  const size_t i_bucket = (size_t)(id % N_COMPONENT_BY_ID_BUCKETS);
  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  struct component_by_id_table *entry = NULL;
  FIND_ENTRY_IN_BUCKET(bucket, id, id, entry);

  /* If component ID not found in bucket, */
  if (!entry) {
    GROW_VEC_IF_NEEDED(bucket, COMPONENT_BY_ID_MIN_BUCKET_SIZE, pairs, struct component_by_id_table);
    entry = &bucket->pairs[bucket->count++];
  }

  /* Populate the component ID and size */
  entry->id   = id;
  entry->size = size;
}


/** Populate the component data for the given entity, component pair */
bool _cecs_set(const cecs_entity_t entity, const cecs_component_t id, void *data)
{
  struct component_by_id_table *entry = get_component_by_id(id);

  const size_t i_index_bucket = (size_t)(entity % N_INDICES_BY_ENTITY_BUCKETS);
  struct index_by_entity_bucket *index_bucket = &entry->indices_by_entity[i_index_bucket];

  struct index_by_entity_pair *index_by_entity = NULL;
  if (index_bucket->count == 1u) {
    index_by_entity = &index_bucket->value;
  } else {
    FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_by_entity);
  }

  if (!index_by_entity) {
    /* Entity doesn't have component, there isn't even a bucket for it */
    return false;
  }

  memcpy(
    ((uint8_t *)entry->data) + (index_by_entity->index * entry->size),
    data,
    entry->size
  );

  return true;
}


/** Zero out the component data for the given entity, component pair */
bool _cecs_zero(const cecs_entity_t entity, const size_t n, ...)
{
  bool changed = false;

  va_list components;
  va_start(components, n);

  for (size_t k = 0u; k < n; ++k) {
    struct component_by_id_table *entry
      = get_component_by_id(va_arg(components, cecs_component_t));

    const size_t i_index_bucket = (size_t)(entity % N_INDICES_BY_ENTITY_BUCKETS);
    struct index_by_entity_bucket *index_bucket = &entry->indices_by_entity[i_index_bucket];

    struct index_by_entity_pair *index_by_entity = NULL;
    if (index_bucket->count == 1u) {
      index_by_entity = &index_bucket->pairs[0u];
    } else {
      FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_by_entity);
    }

    if (!index_by_entity) {
      /* Entity doesn't have component, there isn't even a bucket for it */
      continue;
    }

    memset(
      ((uint8_t *)entry->data) + (index_by_entity->index * entry->size),
      0u,
      entry->size
    );
    changed = true;
  }

  va_end(components);

  return changed;
}