#include <assert.h>
#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cecs/cecs.h>


#define __always_inline __attribute__((__always_inline__))


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


/** entity->signature pair */
struct sig_by_entity_entry {
  cecs_entity_t entity;
  cecs_sig_t sig;
};


/** List of entity->signature pairs */
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


/** List of entity IDs */
struct entity_list {
  size_t count;
  size_t cap;
  cecs_entity_t *entities;
};


/** List of free indices to be used when adding new entities to components */
struct index_list {
  size_t count;
  size_t cap;
  size_t *indices;
};


/** Entity ID->index into component data array */
struct index_by_entity_pair {
  cecs_entity_t entity;
  size_t index;
};

/** List of entity ID->index pairs */
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


/** ID->component data and entity list pairs */
struct component_by_id_table {
  cecs_component_t id;
  size_t size;
  void *data;
  size_t cap;
  size_t count;
  struct index_by_entity_bucket indices_by_entity[N_INDICES_BY_ENTITY_BUCKETS];
  struct index_list free_indices;
};

/** List of ID->component pairs, to be stored in the id->component map */
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
/** Map from component ID to component data and implementing entities list */
struct component_by_id_bucket components_by_id[N_COMPONENT_BY_ID_BUCKETS] = { 0u };


/** An archetype is a unique composition of components. */
struct archetype {
  /** Component signature implemented by this archetype */
  cecs_sig_t sig;
  /** Number of entities implementing this archetype */
  size_t count;
  /** Number of entities able to be stored in the entities array before resizing */
  size_t cap;
  /** Array of entities implementing this archetype */
  cecs_entity_t *entities;
};

/** Signature->Archetype to be stored in the sig->archetype map */
struct archetype_by_sig_entry {
  cecs_sig_t sig;
  struct archetype archetype;
};

/** List of signature->archetype pairs, to be stored in the sig->archetype map */
struct archetype_by_sig_bucket {
  size_t count;
  size_t cap;
  struct archetype_by_sig_entry *pairs;
};

/** List of pointers to archetypes, which are stored in the sig->archetype map */
struct archetype_list {
  size_t count;
  size_t cap;
  struct archetype **elements;
};

/** Minimum number of elements to allocate for an entity list for a given archetype */
#define ARCHETYPE_ENTITIES_LIST_MIN_SIZE ((size_t)16384u)
/** Minimum number of elements allocated for a list of archetypes */
#define ARCHETYPES_LIST_MIN_SIZE ((size_t)32u)
/** Minimum number of elements allocated for a signature->archetype map bucket */
#define ARCHETYPES_BY_SIG_MIN_BUCKET_SIZE ((size_t)32u)
/** Number of buckets in the signature->archetype map */
#define N_ARCHETYPE_BY_SIG_BUCKETS ((size_t)256u)
/** Map from signature to the archetype it represents */
struct archetype_by_sig_bucket archetypes_by_sig[N_ARCHETYPE_BY_SIG_BUCKETS] = { 0u };


/** List of entities in a bucket in the entity set */
struct cecs_entity_set_bucket {
  size_t count;
  size_t cap;
  /* Holds the value of the bucket when count == 1 */
  cecs_entity_t value;
  cecs_entity_t *entities;
};

/** Number of buckets in an entity set */
#define N_ENTITY_SET_BUCKETS ((size_t)8192u)
/** Set of unique entity IDs */
struct cecs_entity_set {
  struct cecs_entity_set_bucket buckets[N_ENTITY_SET_BUCKETS];
};


/** Minmum number of elements allocated for an entity set bucket */
#define ENTITY_SET_MIN_BUCKET_SIZE ((size_t)16384u)
/** Cache the set used to return query result so we don't have to allocate
 * buckets on every query */
struct cecs_entity_set query_result_cache;


/** Grow the given list to the minimum size if empty, or double its size */
#define GROW_LIST_IF_NEEDED(list, min_size, entries, type)                           \
  if ((list)->count >= (list)->cap) {                                                \
    if ((list)->cap == 0u) {                                                         \
      (list)->cap     = (min_size);                                                  \
      (list)->entries = realloc((list)->entries, (list)->cap * sizeof(type));        \
      memset((uint8_t *)(list)->entries, 0u, (list)->cap * sizeof(type));            \
    } else {                                                                         \
      (list)->entries = realloc((list)->entries, ((list)->cap * 2u) * sizeof(type)); \
      memset(                                                                        \
        ((uint8_t *)(list)->entries) + (list)->cap * sizeof(type),                   \
        0u,                                                                          \
        (list)->cap * sizeof(type)                                                   \
      );                                                                             \
      (list)->cap *= 2u;                                                             \
    }                                                                                \
  }


/** Find the entry in list `bucket` whose `identifier` matches `search` and
 * return it in `result` */
#define FIND_ENTRY_IN_BUCKET(bucket, identifier, search, result) \
  for (size_t _i = 0; _i < (bucket)->count; ++_i) {              \
    if ((bucket)->pairs[_i].identifier == (search)) {            \
      (result) = &(bucket)->pairs[_i];                           \
    }                                                            \
  }


/** Returns true if the two signatures are equivalent */
static bool __always_inline sigs_are_equal(const cecs_sig_t *lhs, const cecs_sig_t *rhs)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if (lhs->components[i] != rhs->components[i]) {
      return false;
    }
  }

  return true;
}


/** Returns true if `look_for` is entirely represented by `in` */
static bool __always_inline sig_is_in(const cecs_sig_t *look_for, const cecs_sig_t *in)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if ((look_for->components[i] & in->components[i]) != look_for->components[i]) {
      return false;
    }
  }

  return true;
}


/** Return the boolean OR union of the two signatures */
static void __always_inline sig_union(cecs_sig_t *target, const cecs_sig_t *with)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    target->components[i] |= with->components[i];
  }
}


/** Return `target` less all the components in `to_remove` */
static void __always_inline sig_remove(cecs_sig_t *target, const cecs_sig_t *to_remove)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    target->components[i] &= ~to_remove->components[i];
  }
}


/** Generate a signature reprensenting all the components in the given list */
static cecs_sig_t components_to_sig(const cecs_component_t n, va_list components)
{
  cecs_sig_t sig;
  memset(&sig, 0u, sizeof(sig));

  for (size_t i = 0; i < n; ++i) {
    cecs_component_t id = va_arg(components, cecs_component_t);
    assert(id > 0 && "Component was not registered with CECS_COMPONENT()");
    CECS_ADD_COMPONENT(&sig, id);
  }

  return sig;
}


/** Get all the archetypes that contain the given signature.
 * The caller is responsible for freeing the returned list. */
static struct archetype_list *get_archetypes_by_sig(const cecs_sig_t *sig)
{
  struct archetype_list *list = malloc(sizeof(struct archetype_list));
  memset(list, 0u, sizeof(*list));

  for (size_t i_bucket = 0; i_bucket < N_ARCHETYPE_BY_SIG_BUCKETS; ++i_bucket) {
    struct archetype_by_sig_bucket *bucket = &archetypes_by_sig[i_bucket];
    for (size_t i_entry = 0; i_entry < bucket->count; ++i_entry) {
      struct archetype_by_sig_entry *entry = &bucket->pairs[i_entry];
      if (sig_is_in(sig, &entry->sig)) {
        GROW_LIST_IF_NEEDED(list, ARCHETYPES_LIST_MIN_SIZE, elements, struct archetype *);
        list->elements[list->count++] = &entry->archetype;
      }
    }
  }

  return list;
}


/** Populate the sig->archetype map by assigning the value pointed to by the
 * given archetype pointer to the map value */
static struct archetype *set_archetype_by_sig(const cecs_sig_t *sig, const struct archetype *archetype)
{
  size_t i_bucket = 0;
  for (size_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    i_bucket ^= (size_t)(sig->components[i] % N_ARCHETYPE_BY_SIG_BUCKETS);
  }
  struct archetype_by_sig_bucket *bucket = &archetypes_by_sig[i_bucket];

  /* If entity already exists in bucket, just overwrite its value */
  for (size_t i = 0; i < bucket->count; ++i) {
    struct archetype_by_sig_entry *entry = &bucket->pairs[i];
    if (sigs_are_equal(&entry->sig, sig)) {
      entry->archetype = *archetype;
      return &entry->archetype;
    }
  }

  /* Entity wasn't found in bucket. Check if we need to expand the bucket first
   */
  GROW_LIST_IF_NEEDED(bucket, ARCHETYPES_BY_SIG_MIN_BUCKET_SIZE, pairs, struct archetype_by_sig_entry);

  /* Add a new entry to the bucket */
  struct archetype_by_sig_entry *entry = &bucket->pairs[bucket->count++];

  entry->sig       = *sig;
  entry->archetype = *archetype;

  return &entry->archetype;
}


/** Return the archetype implementing the given signature */
static struct archetype *get_archetype_by_sig(const cecs_sig_t *sig)
{
  size_t i_bucket = 0;
  for (size_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    i_bucket ^= (size_t)(sig->components[i] % N_ARCHETYPE_BY_SIG_BUCKETS);
  }
  struct archetype_by_sig_bucket *bucket = &archetypes_by_sig[i_bucket];

  for (size_t i = 0; i < bucket->count; ++i) {
    if (sigs_are_equal(&bucket->pairs[i].sig, sig)) {
      return &bucket->pairs[i].archetype;
    }
  }

  return NULL;
}


/** Get the archetype implementing the given signature, or add an empty one and
 * return that */
static struct archetype *get_or_add_archetype_by_sig(const cecs_sig_t *sig)
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
static void set_sig_by_entity(const cecs_entity_t entity, cecs_sig_t *sig)
{
  const size_t i_bucket = (size_t)(entity % N_SIG_BY_ENTITY_BUCKETS);
  struct sig_by_entity_bucket *bucket = &sigs_by_entity[i_bucket];

  /* If entity already exists in bucket, just overwrite its value */
  for (size_t i = 0; i < bucket->count; ++i) {
    struct sig_by_entity_entry *entry = &bucket->pairs[i];
    if (entry->entity == entity) {
      entry->sig = *sig;
      return;
    }
  }

  /* Entity wasn't found in bucket; add a new entry to the bucket bucket */
  GROW_LIST_IF_NEEDED(bucket, SIG_BY_ENTITY_MIN_BUCKET_SIZE, pairs, struct sig_by_entity_entry);
  struct sig_by_entity_entry *entry = &bucket->pairs[bucket->count++];

  entry->entity = entity;
  entry->sig    = *sig;
}


/** Get the signature implemented by the given entity */
static cecs_sig_t *get_sig_by_entity(const cecs_entity_t entity)
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
    GROW_LIST_IF_NEEDED(bucket, COMPONENT_BY_ID_MIN_BUCKET_SIZE, pairs, struct component_by_id_table);
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
      /* Bucket list is already allocated, check it for duplicates */
      FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_pair);
      /* Don't allow duplicates */
      if (index_pair) {
        return;
      }
    } else {
      /* Bucket list is not yet allocated, check if the singulate value is a
       * duplicate */
      if (index_bucket->value.entity == entity) {
        return;
      }
    }

    /* No duplicates, found, add entity to component */
    GROW_LIST_IF_NEEDED(index_bucket, INDEX_BY_ENTITY_MIN_BUCKET_SIZE, pairs, struct index_by_entity_pair);

    if (index_bucket->count == 1u) {
      /* Transitioning from 0 to 1 elements. Put the singulate value in the list */
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
  /* Component bucket has entity list, search it for the entity */
  FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_pair);

  if (!index_pair) {
    /* Entity doesn't have component */
    return;
  }

  /* Grow the free indices list if needed */
  GROW_LIST_IF_NEEDED(&component->free_indices, 32u, indices, size_t);
  /* Add the removed entity's data index to the free list */
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
  /* Don't add duplicates */
  for (size_t i = 0; i < archetype->count; ++i) {
    if (archetype->entities[i] == entity) {
      return;
    }
  }

  GROW_LIST_IF_NEEDED(archetype, ARCHETYPE_ENTITIES_LIST_MIN_SIZE, entities, sizeof(cecs_entity_t));

  archetype->entities[archetype->count++] = entity;
}


/** Increment the given entity set iterator */
cecs_entity_t cecs_iter_next(cecs_iter_t *it)
{
  /* Check if we're at the end of the bucket before inspecting the current bucket */
  if (it->i_bucket == N_ENTITY_SET_BUCKETS) {
    return CECS_ENTITY_INVALID;
  }

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

  const struct cecs_entity_set_bucket *bucket = &it->set->buckets[it->i_bucket];
  /* If the bucket has one element then its value is in the bucket directly */
  if (bucket->count == 1u) {
    ++it->i_entity;
    return bucket->value;
  } else {
    /* Iterate the bucket */
    return it->set->buckets[it->i_bucket].entities[it->i_entity++];
  }
}


/** Get an iterator over the entities that implement the given components.
 * Returns the number of entities in the iterator.
 */
cecs_entity_t _cecs_query(cecs_iter_t *it, const cecs_component_t n, ...)
{
  va_list components;
  cecs_entity_t n_entities = 0u;

  memset(it, 0u, sizeof(*it));

  /* Use a prebuilt set so we aren't allocating set lists on every query */
  it->set = &query_result_cache;

  /* Clear the previous results from the cached set */
  for (size_t i = 0; i < N_ENTITY_SET_BUCKETS; ++i) {
    it->set->buckets[i].count = 0u;
  }

  va_start(components, n);
  cecs_sig_t sig = components_to_sig(n, components);
  va_end(components);

  struct archetype_list *archetypes = get_archetypes_by_sig(&sig);
  if (archetypes->count == 0u) {
    /* There are no entities with this archetype */
    free(archetypes);
    return 0u;
  }

  for (size_t i_archetype = 0; i_archetype < archetypes->count; ++i_archetype) {
    struct archetype *archetype = archetypes->elements[i_archetype];
    for (size_t i = 0; i < archetype->count; ++i) {
      const cecs_entity_t entity = archetype->entities[i];
      const size_t i_bucket      = (size_t)(entity % N_ENTITY_SET_BUCKETS);
      struct cecs_entity_set_bucket *bucket = &it->set->buckets[i_bucket];

      if (bucket->count == 0u) {
        /* The bucket is empty, the first entity can go in the singulate value
         * instead of allocating a list */
        bucket->value = entity;
        bucket->count = 1u;
        /* Next entity in archetype */
        continue;
      }

      /* Check for duplicates */
      bool exists = false;
      if (bucket->entities) {
        /* The bucket list is populated, we have to check every single entry */
        for (size_t j = 0; j < bucket->count; ++j) {
          if (bucket->entities[j] == entity) {
            exists = true;
            break;
          }
        }
      } else {
        /* Bucket list is not populated */
        exists = bucket->value == entity;
      }
      if (exists) {
        /* Go to next entity in archetype */
        continue;
      }

      GROW_LIST_IF_NEEDED(bucket, ENTITY_SET_MIN_BUCKET_SIZE, entities, cecs_entity_t);
      if (bucket->count == 1u) {
        /* We're transitioning from 1 element to 2 elements, move the
         * singulate value to the list */
        bucket->entities[0u] = bucket->value;
      }
      /* Add an entry to the end of the bucket and increase the count */
      bucket->entities[bucket->count++] = entity;
      /* Track the total number of entities returned by the query */
      ++n_entities;
    }
  }

  /* If archetypes->elements didn't exist we'd already be out of here */
  free(archetypes->elements);
  free(archetypes);

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
  cecs_sig_t sig = components_to_sig(n, components);
  va_end(components);

  cecs_entity_t entity = g_next_entity++;

  set_sig_by_entity(entity, &sig);

  add_entity_to_archetype(entity, get_or_add_archetype_by_sig(&sig));

  for (size_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if (CECS_HAS_COMPONENT(&sig, i)) {
      add_entity_to_component(i, entity);
    }
  }

  return entity;
}


/** Add the specified components to the given entity */
void _cecs_add(const cecs_entity_t entity, const cecs_component_t n, ...)
{
  va_list components;

  cecs_sig_t *sig = get_sig_by_entity(entity);

  va_start(components, n);
  cecs_sig_t sig_to_add = components_to_sig(n, components);
  va_end(components);

  sig_union(sig, &sig_to_add);

  /* Add the entity to its new archetype */
  add_entity_to_archetype(entity, get_or_add_archetype_by_sig(sig));
  /* Update the cached signature for this entity */
  set_sig_by_entity(entity, sig);

  /* Add the entity to all components named */
  va_start(components, n);
  add_entity_to_component(va_arg(components, cecs_component_t), entity);
  va_end(components);
}


/** Remove the specified components from the given entity */
void _cecs_remove(const cecs_entity_t entity, const cecs_component_t n, ...)
{
  va_list components;

  /* Get the entity's current signature from the cache */
  cecs_sig_t *sig = get_sig_by_entity(entity);

  /* Remove the entity from its current archetype */
  struct archetype *archetype = get_archetype_by_sig(sig);
  for (cecs_entity_t i = 0; i < archetype->count; ++i) {
    if (archetype->entities[i] == entity) {
      /* Put the last entity in the array into the removed entity's slot */
      archetype->entities[i] = archetype->entities[--archetype->count];
      break;
    }
  }

  va_start(components, n);
  cecs_sig_t sig_to_remove = components_to_sig(n, components);
  va_end(components);

  sig_remove(sig, &sig_to_remove);

  /* Add the entity to its new archetype */
  add_entity_to_archetype(entity, get_or_add_archetype_by_sig(sig));
  /* Cache the entity's new signature */
  set_sig_by_entity(entity, sig);

  /* Remove the entity from all components named in the list */
  va_start(components, n);
  remove_entity_from_component(va_arg(components, cecs_component_t), entity);
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
    GROW_LIST_IF_NEEDED(bucket, COMPONENT_BY_ID_MIN_BUCKET_SIZE, pairs, struct component_by_id_table);
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
  for (size_t k = 0; k < n; ++k) {
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