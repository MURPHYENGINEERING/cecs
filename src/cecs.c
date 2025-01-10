#include <assert.h>
#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cecs/cecs.h>


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
#define SIG_BY_ENTITY_MIN_BUCKET_SIZE ((size_t) 32u)
/** Number of buckets in the entity->signature map */
#define N_SIG_BY_ENTITY_BUCKETS ((size_t) 16384u)
/** Map from entity ID to the signature it implements */
struct sig_by_entity_bucket sigs_by_entity[N_SIG_BY_ENTITY_BUCKETS] = { 0u };


/* List of entity IDs */
struct entity_list {
  size_t count;
  size_t cap;
  cecs_entity_t *entities;
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
  struct index_by_entity_pair *pairs;
};

/** Minimum number of elements allocated for an entity->index map bucket */
#define INDEX_BY_ENTITY_MIN_BUCKET_SIZE ((size_t) 8u)
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
};

/** List of ID->component pairs, to be stored in the id->component map */
struct component_by_id_bucket {
  size_t count;
  size_t cap;
  struct component_by_id_table *pairs;
};

/** Minimum number of elements allocated for component data by index */
#define COMPONENT_TABLE_MIN_DATA_SIZE ((size_t) 16384u)
/** Minimum number of elements allocated for a component ID->component map bucket */
#define COMPONENT_BY_ID_MIN_BUCKET_SIZE ((size_t) 8u)
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
#define ARCHETYPE_ENTITIES_LIST_MIN_SIZE ((size_t) 16384u)
/** Minimum number of elements allocated for a list of archetypes */
#define ARCHETYPES_LIST_MIN_SIZE ((size_t)32u)
/** Minimum number of elements allocated for a signature->archetype map bucket */
#define ARCHETYPES_BY_SIG_MIN_BUCKET_SIZE ((size_t)32u)
/** Number of buckets in the signature->archetype map */
#define N_ARCHETYPE_BY_SIG_BUCKETS ((size_t)256u)
/** Map from signature to the archetype it represents */
struct archetype_by_sig_bucket archetypes_by_sig[N_ARCHETYPE_BY_SIG_BUCKETS] = { 0u };


/** Minmum number of elements allocated for an entity set bucket */
#define ENTITY_SET_MIN_BUCKET_SIZE ((size_t) 16384u)
/** Cache the set used to return query result so we don't have to allocate
 * buckets on every query */
struct cecs_entity_set query_result_cache;


/** Grow the given list to the minimum size if empty, or double its size */
#define GROW_LIST_IF_NEEDED(list, min_size, entries, type)                                               \
  if ((list)->count >= (list)->cap) {                                                                  \
    if ((list)->cap == 0u) {                                                                           \
      (list)->cap   = (min_size);                                                                      \
      (list)->entries = realloc((list)->entries, (list)->cap * sizeof(type));                              \
      memset((uint8_t *)(list)->entries, 0u, (list)->cap * sizeof(type));                                \
    } else {                                                                                           \
      (list)->entries = realloc((list)->entries, ((list)->cap * 2u) * sizeof(type));                       \
      memset(((uint8_t *)(list)->entries) + (list)->cap * sizeof(type), 0u, (list)->cap * sizeof(type)); \
      (list)->cap *= 2u;                                                                               \
    }                                                                                                  \
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
static bool sigs_are_equal(const cecs_sig_t *lhs, const cecs_sig_t *rhs)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if (lhs->components[i] != rhs->components[i]) {
      return false;
    }
  }

  return true;
}


/** Returns true if `look_for` is entirely represented by `in` */
static bool sig_is_in(const cecs_sig_t *look_for, const cecs_sig_t *in)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if ((look_for->components[i] & in->components[i]) != look_for->components[i]) {
      return false;
    }
  }

  return true;
}


/** Return the boolean OR union of the two signatures */
static void sig_union(cecs_sig_t *target, const cecs_sig_t *with)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    target->components[i] |= with->components[i];
  }
}


/** Return `target` less all the components in `to_remove` */
static void sig_remove(cecs_sig_t *target, const cecs_sig_t *to_remove)
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

  /* Iterate the bucket */
  return it->set->buckets[it->i_bucket].entities[it->i_entity++];
}


/** Add a unique entity ID to the given entity set */
static bool put_entity_in_set(struct cecs_entity_set *set, const cecs_entity_t entity)
{
  size_t i_bucket = (size_t)(entity % N_ENTITY_SET_BUCKETS);

  struct cecs_entity_set_bucket *bucket = &set->buckets[i_bucket];

  /* Check for duplicates */
  for (size_t i = 0; i < bucket->count; ++i) {
    if (bucket->entities[i] == entity) {
      return false;
    }
  }

  GROW_LIST_IF_NEEDED(bucket, ENTITY_SET_MIN_BUCKET_SIZE, entities, cecs_entity_t);

  /* Add an entry to the end of the bucket and increase the count */
  bucket->entities[bucket->count++] = entity;

  return true;
}


/** Populate the entity->signature map */
static void set_sig_by_entity(const cecs_entity_t entity, cecs_sig_t *sig)
{
  size_t i_bucket = (size_t)(entity % N_SIG_BY_ENTITY_BUCKETS);

  struct sig_by_entity_bucket *bucket = &sigs_by_entity[i_bucket];

  /* If entity already exists in bucket, just overwrite its value */
  for (size_t i = 0; i < bucket->count; ++i) {
    struct sig_by_entity_entry *entry = &bucket->pairs[i];
    if (entry->entity == entity) {
      entry->sig = *sig;
      return;
    }
  }

  GROW_LIST_IF_NEEDED(bucket, SIG_BY_ENTITY_MIN_BUCKET_SIZE, pairs, struct sig_by_entity_entry);

  /* Entity wasn't found in bucket; add a new entry to the bucket bucket */
  struct sig_by_entity_entry *entry = &bucket->pairs[bucket->count++];

  entry->entity = entity;
  entry->sig    = *sig;
}


/** Get the signature implemented by the given entity */
static cecs_sig_t *get_sig_by_entity(const cecs_entity_t entity)
{
  size_t i_bucket = (size_t)(entity % N_SIG_BY_ENTITY_BUCKETS);

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

  /* If component ID not found in bucket, */
  if (!table) {
    GROW_LIST_IF_NEEDED(bucket, COMPONENT_BY_ID_MIN_BUCKET_SIZE, pairs, struct component_by_id_table);
    table = &bucket->pairs[bucket->count++];
  }

  const size_t i_index_bucket = (size_t)(entity % N_INDICES_BY_ENTITY_BUCKETS);
  struct index_by_entity_bucket *index_bucket = &table->indices_by_entity[i_index_bucket];

  struct index_by_entity_pair *index_pair = NULL;
  FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_pair);

  /* Don't allow duplicates */
  if (index_pair) {
    return;
  }

  /* Not a duplicate; add to the end of the bucket */
  GROW_LIST_IF_NEEDED(index_bucket, INDEX_BY_ENTITY_MIN_BUCKET_SIZE, pairs, cecs_entity_t);
  index_pair = &index_bucket->pairs[index_bucket->count++];

  index_pair->entity = entity;
  /* The index is the last table in the component data table, then increment the
   * count of data elements in the table. */
  index_pair->index = table->count++;

  /* Grow the component table if needed */
  if (table->count >= table->cap) {
    if (table->cap == 0u) {
      /* Allocate for the first time */
      table->cap  = COMPONENT_TABLE_MIN_DATA_SIZE;
      table->data = realloc(table->data, table->cap * table->size);
      memset(table->data, 0u, table->cap * table->size);

    } else {
      /* Double the size and zero out the new data capacity */
      table->data = realloc(table->data, table->cap * 2u * table->size);
      memset(
        ((uint8_t *)table->data) + table->cap * table->size,
        0u,
        table->cap * table->size
      );
      table->cap *= 2u;
    }
  }
}


static void remove_entity_from_component(const cecs_component_t id, const cecs_entity_t entity)
{
  assert(id > 0 && "Component was not registered with CECS_COMPONENT()");

  const size_t i_bucket = (size_t)(id % N_COMPONENT_BY_ID_BUCKETS);
  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  struct component_by_id_table *entry = NULL;
  FIND_ENTRY_IN_BUCKET(bucket, id, id, entry);

  if (!entry) {
    /* Component doesn't exist */
    return;
  }

  const size_t i_index_bucket = (size_t)(entity % N_INDICES_BY_ENTITY_BUCKETS);
  struct index_by_entity_bucket *index_bucket = &entry->indices_by_entity[i_index_bucket];

  struct index_by_entity_pair *index_pair = NULL;
  FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_pair);

  if (!index_pair) {
    /* Entity doesn't have component */
    return;
  }

  /* Find the entity with the last entry in the component data table.
   * This will replace our chosen entity to remove. */
  /* TODO: Currently we don't evict component data when removed. Maybe use a
   * free list and newly-added entities can choose indices from it instead. */

  /* Move the last entry in the bucket into this position, overwriting the
   * removed entity */
  memcpy(index_pair, &index_bucket->pairs[--index_bucket->count], sizeof(struct index_by_entity_pair));
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


/** Remove the given entity from the specified archetype because its archetype changed */
static void remove_entity_from_archetype(const cecs_entity_t entity, struct archetype *archetype)
{
  for (cecs_entity_t i = 0; i < archetype->count; ++i) {
    if (archetype->entities[i] == entity) {
      /* Put the last entity in the array into the removed entity's slot */
      archetype->entities[i] = archetype->entities[--archetype->count];
      return;
    }
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
    struct cecs_entity_set_bucket *bucket = &it->set->buckets[i];
    bucket->count                         = 0u;
  }

  va_start(components, n);
  cecs_sig_t sig = components_to_sig(n, components);
  va_end(components);

  struct archetype_list *archetypes = get_archetypes_by_sig(&sig);
  if (!archetypes) {
    return 0u;
  }

  for (size_t i_archetype = 0; i_archetype < archetypes->count; ++i_archetype) {
    struct archetype *archetype = archetypes->elements[i_archetype];
    for (size_t i = 0; i < archetype->count; ++i) {
      if (put_entity_in_set(it->set, archetype->entities[i])) {
        ++n_entities;
      }
    }
  }

  free(archetypes);

  return n_entities;
}


/** Get a pointer to the component data for the given entity and component pair */
void *_cecs_get(const cecs_entity_t entity, const cecs_component_t id)
{
  struct component_by_id_table *component = get_component_by_id(id);

  const size_t i_index_bucket = (size_t)(entity % N_INDICES_BY_ENTITY_BUCKETS);
  struct index_by_entity_bucket *index_bucket = &component->indices_by_entity[i_index_bucket];

  struct index_by_entity_pair *index_by_entity = NULL;
  FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_by_entity);

  if (!index_by_entity) {
    /* Entity doesn't have component */
    return NULL;
  }

  return (void *)(((uint8_t *)component->data) + (index_by_entity->index * component->size));
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

  add_entity_to_archetype(entity, get_or_add_archetype_by_sig(sig));
  set_sig_by_entity(entity, sig);

  for (size_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if (CECS_HAS_COMPONENT(&sig_to_add, i)) {
      add_entity_to_component(i, entity);
    }
  }
}


/** Remove the specified components from the given entity */
void _cecs_remove(const cecs_entity_t entity, const cecs_component_t n, ...)
{
  va_list components;

  cecs_sig_t *sig = get_sig_by_entity(entity);

  remove_entity_from_archetype(entity, get_archetype_by_sig(sig));

  va_start(components, n);
  cecs_sig_t sig_to_remove = components_to_sig(n, components);
  va_end(components);

  sig_remove(sig, &sig_to_remove);

  add_entity_to_archetype(entity, get_or_add_archetype_by_sig(sig));
  set_sig_by_entity(entity, sig);

  for (size_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if (CECS_HAS_COMPONENT(&sig_to_remove, i)) {
      remove_entity_from_component(i, entity);
    }
  }
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
  FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_by_entity);

  if (!index_by_entity) {
    /* Entity doesn't have component, there isn't even a bucket for it */
    return false;
  }

  for (size_t i = 0; i < index_bucket->count; ++i) {
    if (index_bucket->pairs[i].entity == entity) {
      memcpy(
        ((uint8_t *)entry->data) + (index_bucket->pairs[i].index * entry->size),
        data,
        entry->size
      );
      return true;
    }
  }

  /* Entity doesn't have component */
  return false;
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
    FIND_ENTRY_IN_BUCKET(index_bucket, entity, entity, index_by_entity);

    if (!index_by_entity) {
      /* Entity doesn't have component, there isn't even a bucket for it */
      continue;
    }

    for (size_t i = 0; i < index_bucket->count; ++i) {
      if (index_bucket->pairs[i].entity == entity) {
        memset(
          ((uint8_t *)entry->data) + (index_bucket->pairs[i].index * entry->size),
          0u,
          entry->size
        );
        changed = true;
      }
    }
  }
  va_end(components);

  return changed;
}