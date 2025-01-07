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


struct entity_list {
  size_t count;
  size_t cap;
  cecs_entity_t *entities;
};

struct component_by_id_entry {
  cecs_component_t id;
  size_t size;
  void *data;
  struct entity_list entities;
};

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
  size_t n_entities;
  /** Number of entities able to be stored in the entities array before resizing */
  size_t cap_entities;
  /** Array of entities implementing this archetype */
  cecs_entity_t *entities;
};

struct archetype_by_sig_entry {
  cecs_sig_t sig;
  struct archetype archetype;
};

struct archetype_by_sig_bucket {
  size_t count;
  size_t cap;
  struct archetype_by_sig_entry *entries;
};

struct archetype_list {
  size_t count;
  size_t cap;
  struct archetype **elements;
};

#define N_ARCHETYPE_BY_SIG_BUCKETS ((size_t)256u)
/** Map from signature to the archetype it represents */
static struct archetype_by_sig_bucket archetypes_by_sig[N_ARCHETYPE_BY_SIG_BUCKETS]
  = { 0u };


#define N_COMPONENT_BY_ID_BUCKETS ((size_t)256u)
static struct component_by_id_bucket components_by_id[N_COMPONENT_BY_ID_BUCKETS] = { 0u };


static bool sigs_are_equal(const cecs_sig_t *lhs, const cecs_sig_t *rhs)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if (lhs->components[i] != rhs->components[i]) {
      return false;
    }
  }

  return true;
}


static bool sig_is_in(const cecs_sig_t *look_for, const cecs_sig_t *in)
{
  for (cecs_component_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    if ((look_for->components[i] & in->components[i]) != look_for->components[i]) {
      return false;
    }
  }

  return true;
}


static void append_archetype_to_list(struct archetype *archetype, struct archetype_list *list)
{
  if (list->count >= list->cap) {
    if (list->cap == 0u) {
      list->cap = 16u;
    }
    list->cap *= 2u;
    list->elements = realloc(list->elements, list->cap * sizeof(struct archetype*));
  }

  list->elements[list->count++] = archetype;
}


/** The caller is responsible for freeing the archetypes list */
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


static struct archetype *set_archetype_by_sig(const cecs_sig_t *sig, const struct archetype *archetype)
{
  size_t i_bucket = 0;
  for (size_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    i_bucket ^= (sig->components[i] % N_ARCHETYPE_BY_SIG_BUCKETS);
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

  /* Entity wasn't found in bucket. Check if we need to expand the bucket first */
  if (bucket->count >= bucket->cap) {
    if (bucket->cap == 0) {
      bucket->cap = 32u;
    }
    bucket->cap *= 2u;
    bucket->entries
      = realloc(bucket->entries, bucket->cap * sizeof(struct archetype_by_sig_entry));
  }

  /* Add a new entry to the bucket */
  struct archetype_by_sig_entry *entry = &bucket->entries[bucket->count++];

  entry->sig       = *sig;
  entry->archetype = *archetype;

  return &entry->archetype;
}


static struct archetype *get_archetype_by_sig(const cecs_sig_t *sig)
{
  size_t i_bucket = 0;
  for (size_t i = 0; i < CECS_COMPONENT_TO_INDEX(CECS_N_COMPONENTS); ++i) {
    i_bucket ^= (sig->components[i] % N_ARCHETYPE_BY_SIG_BUCKETS);
  }
  struct archetype_by_sig_bucket *bucket = &archetypes_by_sig[i_bucket];

  for (size_t i = 0; i < bucket->count; ++i) {
    if (sigs_are_equal(&bucket->entries[i].sig, sig)) {
      return &bucket->entries[i].archetype;
    }
  }

  return NULL;
}


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


static bool put_entity_in_set(struct cecs_entity_set *set, const cecs_entity_t entity)
{
  size_t i_bucket = (size_t)entity % N_ENTITY_SET_BUCKETS;

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


static void append_entity_to_list(struct entity_list *list, const cecs_entity_t entity)
{
  /* Grow the list if needed */
  if (list->count >= list->cap) {
    if (list->cap == 0u) {
      list->cap = 32u;
    }
    list->cap *= 2u;
    list->entities = realloc(list->entities, list->cap * sizeof(cecs_entity_t));
  }

  list->entities[list->count++] = entity;
}


static struct component_by_id_entry *add_entity_to_component(const cecs_component_t id, const cecs_entity_t entity)
{
  struct component_by_id_entry *entry = NULL;
  struct entity_list *list            = NULL;

  size_t i_bucket = (size_t)id % N_SIG_BY_ENTITY_BUCKETS;

  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  /* Find component in bucket */
  for (size_t i = 0; i < bucket->count; ++i) {
    if (bucket->entries[i].id == id) {
      entry = &bucket->entries[i];
      break;
    }
  }

  /* If component ID not found in bucket, */
  if (!entry) {
    /* Grow the bucket if needed */
    if (bucket->count >= bucket->cap) {
      if (bucket->cap == 0u) {
        bucket->cap = 32u;
      }
      bucket->cap *= 2u;
      bucket->entries
        = realloc(bucket->entries, bucket->cap * sizeof(struct sig_by_entity_entry));
    }
    entry = &bucket->entries[bucket->count++];
  }

  entry->id = id;
  append_entity_to_list(&entry->entities, entity);

  return entry;
}


static struct component_by_id_entry *get_component_by_id(const cecs_component_t id)
{
  size_t i_bucket = (size_t)id % N_COMPONENT_BY_ID_BUCKETS;
  struct component_by_id_bucket *bucket = &components_by_id[i_bucket];

  for (size_t i = 0; i < bucket->count; ++i) {
    if (bucket->entries[i].id == id) {
      return &bucket->entries[i];
    }
  }

  return NULL;
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


static void add_entity_to_archetype(const cecs_entity_t entity, struct archetype *archetype)
{
  /* Don't add duplicates */
  for (size_t i = 0; i < archetype->n_entities; ++i) {
    if (archetype->entities[i] == entity) {
      return;
    }
  }

  const size_t i_entity = archetype->n_entities++;
  if (archetype->n_entities >= archetype->cap_entities) {
    if (archetype->cap_entities == 0) {
      archetype->cap_entities = 512u;
    }
    archetype->cap_entities *= 2;
    archetype->entities
      = realloc(archetype->entities, archetype->cap_entities * sizeof(cecs_entity_t));
  }

  archetype->entities[i_entity] = entity;
}


static void remove_entity_from_archetype(const cecs_entity_t entity, struct archetype *archetype)
{
  for (cecs_entity_t i = 0; i < archetype->n_entities; ++i) {
    if (archetype->entities[i] == entity) {
      /* Put the last entity in the array into the removed entity's slot */
      archetype->entities[i] = archetype->entities[--archetype->n_entities];
      return;
    }
  }
}


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
    for (size_t i = 0; i < archetype->n_entities; ++i) {
      if (put_entity_in_set(&it->set, archetype->entities[i])) {
        ++n_entities;
      }
    }
  }

  free(archetypes);

  return n_entities;
}


void *_cecs_get(const cecs_entity_t entity, const cecs_component_t id)
{
  (void)entity;
  (void)id;
  return NULL;
}


cecs_entity_t _cecs_create(const cecs_component_t n, ...)
{
  va_list components;
  va_start(components, n);
  cecs_sig_t sig = components_to_sig(n, components);
  va_end(components);

  cecs_entity_t entity = g_next_entity++;

  set_sig_by_entity(entity, &sig);

  add_entity_to_archetype(entity, get_or_add_archetype_by_sig(&sig));

  return entity;
}


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
}


void _cecs_remove(const cecs_entity_t entity, const cecs_component_t n, ...)
{
  va_list components;

  cecs_sig_t *sig = get_sig_by_entity(entity);

  remove_entity_from_archetype(entity, get_archetype_by_sig(sig));

  va_start(components, n);
  cecs_sig_t sig_to_remove = components_to_sig(n, components);
  va_end(components);

  sig_remove(sig, &sig_to_remove);

  add_entity_to_archetype(entity, get_archetype_by_sig(sig));
  set_sig_by_entity(entity, sig);
}


void cecs_register_component(const cecs_component_t id, const size_t size)
{
  (void)id;
  (void)size;
}