#include <assert.h>
#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cecs/cecs.h>


/** The next component ID to be assigned on registration */
cecs_component_t CECS_NEXT_COMPONENT_ID = (cecs_component_t)0u;

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
  struct sig_by_entity_entry *entries;
};

/** Number of buckets in the entity->signature map */
#define N_SIG_BY_ENTITY_BUCKETS ((size_t)4096u)
/** Map from entity ID to the signature it implements */
struct sig_by_entity_bucket sigs_by_entity[N_SIG_BY_ENTITY_BUCKETS] = { 0u };


/* List of entity IDs */
struct entity_list {
  size_t count;
  size_t cap;
  cecs_entity_t *entities;
};


/** ID->component data and entity list pairs */
struct component_by_id_entry {
  cecs_component_t id;
  size_t size;
  void *data;
  struct entity_list entities;
};

/** List of ID->component pairs, to be stored in the id->component map */
struct component_by_id_bucket {
  size_t count;
  size_t cap;
  struct component_by_id_entry *entries;
};

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
  struct archetype_by_sig_entry *entries;
};

/** List of pointers to archetypes, which are stored in the sig->archetype map */
struct archetype_list {
  size_t count;
  size_t cap;
  struct archetype **elements;
};

/** Number of buckets in the signature->archetype map */
#define N_ARCHETYPE_BY_SIG_BUCKETS ((size_t)256u)
/** Map from signature to the archetype it represents */
struct archetype_by_sig_bucket archetypes_by_sig[N_ARCHETYPE_BY_SIG_BUCKETS]
  = { 0u };


/** Number of buckets in the componet ID->data map */
#define N_COMPONENT_BY_ID_BUCKETS ((size_t)256u)
/** Map from component ID to component data and implementing entities list */
struct component_by_id_bucket components_by_id[N_COMPONENT_BY_ID_BUCKETS] = { 0u };


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
  for (size_t i = 0; i < (bucket)->count; ++i) {                 \
    if ((bucket)->entries[i].identifier == (search)) {           \
      (result) = &(bucket)->entries[i];                          \
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
    CECS_ADD_COMPONENT(&sig, id);
  }

  return sig;
}


/** Append  the given archetype pointer to the archetype pointer list, growing
 * if needed */
static void append_archetype_to_list(struct archetype *archetype, struct archetype_list *list)
{
  GROW_LIST_IF_NEEDED(list, 32u, elements, struct archetype *);

  list->elements[list->count++] = archetype;
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
      struct archetype_by_sig_entry *entry = &bucket->entries[i_entry];
      if (sig_is_in(sig, &entry->sig)) {
        append_archetype_to_list(&entry->archetype, list);
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
    struct archetype_by_sig_entry *entry = &bucket->entries[i];
    if (sigs_are_equal(&entry->sig, sig)) {
      entry->archetype = *archetype;
      return &entry->archetype;
    }
  }

  /* Entity wasn't found in bucket. Check if we need to expand the bucket first
   */
  GROW_LIST_IF_NEEDED(bucket, 64u, entries, struct archetype_by_sig_entry);

  /* Add a new entry to the bucket */
  struct archetype_by_sig_entry *entry = &bucket->entries[bucket->count++];

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
    if (sigs_are_equal(&bucket->entries[i].sig, sig)) {
      return &bucket->entries[i].archetype;
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

  if (it->i_entity >= it->set.buckets[it->i_bucket].count) {
    /* Skip empty buckets until we reach a non-empty one or the end of the set */
    while ((++it->i_bucket < N_ENTITY_SET_BUCKETS)
           && (it->set.buckets[it->i_bucket].count == 0)) {
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

  GROW_LIST_IF_NEEDED(bucket, 64u, entities, cecs_entity_t);

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
    struct sig_by_entity_entry *entry = &bucket->entries[i];
    if (entry->entity == entity) {
      entry->sig = *sig;
      return;
    }
  }

  GROW_LIST_IF_NEEDED(bucket, 64u, entries, struct sig_by_entity_entry);

  /* Entity wasn't found in bucket; add a new entry to the bucket bucket */
  struct sig_by_entity_entry *entry = &bucket->entries[bucket->count++];

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


/** Register the given component size with its ID */
static struct component_by_id_entry *register_component(const cecs_component_t id, const size_t size)
{
  const size_t i_bucket = (size_t)(id % N_COMPONENT_BY_ID_BUCKETS);
  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  struct component_by_id_entry *entry = NULL;
  FIND_ENTRY_IN_BUCKET(bucket, id, id, entry);

  /* If component ID not found in bucket, */
  if (!entry) {
    GROW_LIST_IF_NEEDED(bucket, 32u, entries, struct component_by_id_entry);
    entry = &bucket->entries[bucket->count++];
  }

  /* Populate the component ID and size */
  entry->id   = id;
  entry->size = size; 

  return entry;
}


/** Add a block of component data for the given entity */
static void add_entity_to_component(const cecs_component_t id, const cecs_entity_t entity)
{
  struct component_by_id_entry *entry = NULL;

  const size_t i_bucket = (size_t)(id % N_COMPONENT_BY_ID_BUCKETS);

  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  FIND_ENTRY_IN_BUCKET(bucket, id, id, entry);

  /* If component ID not found in bucket, */
  if (!entry) {
    GROW_LIST_IF_NEEDED(bucket, 64u, entries, struct component_by_id_entry);

    entry = &bucket->entries[bucket->count++];
  }

  /* Don't allow duplicates */
  for (size_t i = 0; i < entry->entities.count; ++i) {
    if (entry->entities.entities[i] == entity) {
      return;
    }
  }

  /* Grow the component store and entities list to a minimum starting size */
  if (entry->entities.count >= entry->entities.cap) {
    if (entry->entities.cap == 0u) {
      entry->entities.cap = 64u;

      /* Reallocate and zero the entities list for this component */
      entry->entities.entities
        = realloc(entry->entities.entities, entry->entities.cap * sizeof(cecs_entity_t));
      memset((uint8_t *)entry->entities.entities, 0u, entry->entities.cap * sizeof(cecs_entity_t));

      /* Reallocate and zero the entity data for this component */
      entry->data = realloc(entry->data, entry->entities.cap * entry->size);
      memset((uint8_t *)entry->data, 0u, entry->entities.cap * entry->size);

    } else {
      /* Reallocate and zero the entities list for this component */
      entry->entities.entities
        = realloc(entry->entities.entities, entry->entities.cap * 2u * sizeof(cecs_entity_t));
      memset(
        ((uint8_t *)entry->entities.entities) + entry->entities.cap * sizeof(cecs_entity_t),
        0u,
        entry->entities.cap * sizeof(cecs_entity_t)
      );

      /* Reallocate and zero the entity data for this component */
      entry->data = realloc(entry->data, entry->entities.cap * 2u * entry->size);
      memset(
        ((uint8_t *)entry->data) + entry->entities.cap * entry->size,
        0u,
        entry->entities.cap * entry->size
      );

      entry->entities.cap *= 2u;
    }
  }

  entry->entities.entities[entry->entities.count++] = entity;
}


static void remove_entity_from_component(const cecs_component_t id, const cecs_entity_t entity)
{
  struct component_by_id_entry *entry = NULL;

  const size_t i_bucket = (size_t)(id % N_COMPONENT_BY_ID_BUCKETS);

  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  FIND_ENTRY_IN_BUCKET(bucket, id, id, entry);

  if (!entry) {
    /* Component doesn't exist */
    return;
  }

  size_t i = 0;
  while (i < entry->entities.count) {
    if (entry->entities.entities[i] == entity) {
      break;
    }
    ++i;
  }

  if (i == entry->entities.count) {
    /* Entity doesn't implement component */
    return;
  }

  /* Overwrite the entry with the last entry in the list */
  entry->entities.entities[i] = entry->entities.entities[--entry->entities.count];
  /* Overwrite the entry data with the last entry data in the list*/
  memcpy(
    ((uint8_t *)entry->data) + (i * entry->size),
    ((uint8_t *)entry->data) + (entry->entities.count * entry->size),
    entry->size
  );
}


/** Get the component data for the given component ID */
static struct component_by_id_entry *get_component_by_id(const cecs_component_t id)
{
  size_t i_bucket = (size_t)(id % N_COMPONENT_BY_ID_BUCKETS);
  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  struct component_by_id_entry *entry = NULL;
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

  GROW_LIST_IF_NEEDED(archetype, 512u, entities, sizeof(cecs_entity_t));

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
      if (put_entity_in_set(&it->set, archetype->entities[i])) {
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
  const size_t i_bucket = (size_t)(id % N_COMPONENT_BY_ID_BUCKETS);

  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  struct component_by_id_entry *entry = NULL;
  FIND_ENTRY_IN_BUCKET(bucket, id, id, entry);

  assert(
    entry && "Can't get component that wasn't registered using CECS_COMPONENT()"
  );

  size_t i = 0u;
  while (i < entry->entities.count) {
    if (entry->entities.entities[i] == entity) {
      break;
    }
    ++i;
  }

  if (i == entry->entities.count) {
    return NULL;
  }

  return (void *)(((uint8_t *)entry->data) + (i * entry->size));
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
  register_component(id, size);
}


/** Populate the component data for the given entity, component pair */
bool _cecs_set(const cecs_entity_t entity, const cecs_component_t id, void *data)
{
  struct component_by_id_entry *entry = get_component_by_id(id);

  for (size_t i = 0; i < entry->entities.count; ++i) {
    if (entry->entities.entities[i] == entity) {
      memcpy(((uint8_t *)entry->data) + (i * entry->size), data, entry->size);
      return true;
    }
  }

  return false;
}


/** Zero out the component data for the given entity, component pair */
bool _cecs_zero(const cecs_entity_t entity, const size_t n, ...)
{
  bool changed = false;

  va_list components;

  va_start(components, n);
  for (size_t i = 0; i < n; ++i) {
    struct component_by_id_entry *entry
      = get_component_by_id(va_arg(components, cecs_component_t));

    for (size_t j = 0; j < entry->entities.count; ++j) {
      if (entry->entities.entities[j] == entity) {
        memset(((uint8_t *)entry->data) + (j * entry->size), 0u, entry->size);
        changed = true;
      }
    }
  }
  va_end(components);

  return changed;
}