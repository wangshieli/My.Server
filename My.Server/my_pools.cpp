#include "pch.h"
#include "my_pools.h"

#define APR_IF_VALGRIND(x)

#define APR_VALGRIND_NOACCESS(addr_, size_)                     \
    APR_IF_VALGRIND(VALGRIND_MAKE_MEM_NOACCESS(addr_, size_))
#define APR_VALGRIND_UNDEFINED(addr_, size_)                    \
    APR_IF_VALGRIND(VALGRIND_MAKE_MEM_UNDEFINED(addr_, size_))

#define MIN_ALLOC (2 * BOUNDARY_SIZE)
#define MAX_INDEX   20

#define BOUNDARY_INDEX 12
#define BOUNDARY_SIZE (1 << BOUNDARY_INDEX)

#define GUARDPAGE_SIZE 0

struct apr_allocator_t
{
	size_t	max_index;
	size_t	max_free_index;
	size_t	current_free_index;
	apr_thread_mutex_t	*mutex;
	apr_pool_t	*owner;
	apr_memnode_t	*free[MAX_INDEX];
};
#define SIZEOF_ALLOCATOR_T  APR_ALIGN_DEFAULT(sizeof(apr_allocator_t))

int apr_allocator_create(apr_allocator_t **allocator)
{
	apr_allocator_t *new_allocator;

	*allocator = NULL;

	if ((new_allocator = (apr_allocator_t*)malloc(SIZEOF_ALLOCATOR_T)) == NULL)
		return -1;

	memset(new_allocator, 0, SIZEOF_ALLOCATOR_T);
	new_allocator->max_free_index = APR_ALLOCATOR_MAX_FREE_UNLIMITED;
//	apr_allocator_max_free_set(new_allocator, 1024 * 10);

	*allocator = new_allocator;

	return 0;
}

void apr_allocator_destroy(apr_allocator_t *allocator)
{
	size_t index;
	apr_memnode_t *node, **ref;

	for (index = 0; index < MAX_INDEX; index++)
	{
		ref = &allocator->free[index];
		while ((node = *ref) != NULL)
		{
			*ref = node->next;
			free(node);
		}
	}

	free(allocator);
}

void apr_allocator_mutex_set(apr_allocator_t *allocator, apr_thread_mutex_t *mutex)
{
	allocator->mutex = mutex;
}

apr_thread_mutex_t * apr_allocator_mutex_get(apr_allocator_t *allocator)
{
	return allocator->mutex;
}

void apr_allocator_owner_set(apr_allocator_t *allocator, apr_pool_t *pool)
{
	allocator->owner = pool;
}

apr_pool_t *apr_allocator_owner_get(apr_allocator_t *allocator)
{
	return allocator->owner;
}

void apr_allocator_max_free_set(apr_allocator_t *allocator, size_t in_size)
{
	size_t max_free_index;
	size_t size = in_size;

	apr_thread_mutex_t *mutex;

	mutex = apr_allocator_mutex_get(allocator);
	if (mutex != NULL)
		apr_thread_mutex_lock(mutex);

	max_free_index = APR_ALIGN(size, BOUNDARY_SIZE) >> BOUNDARY_INDEX;
	allocator->current_free_index += max_free_index;
	allocator->current_free_index -= allocator->max_free_index;
	allocator->max_free_index = max_free_index;
	if (allocator->current_free_index > max_free_index)
		allocator->current_free_index = max_free_index;

	if (mutex != NULL)
		apr_thread_mutex_unlock(mutex);
}

static inline size_t allocator_align(size_t in_size)
{
	size_t size = in_size;

	size = APR_ALIGN(size + APR_MEMNODE_T_SIZE, BOUNDARY_SIZE);
	if (size < in_size)
		return 0;
	if (size < MIN_ALLOC)
		size = MIN_ALLOC;

	return size;
}

size_t apr_allocator_align(apr_allocator_t *allocator, size_t size)
{
	(void)allocator;
	return allocator_align(size);
}

static inline apr_memnode_t *allocator_alloc(apr_allocator_t *allocator, size_t in_size)
{
	apr_memnode_t *node, **ref;
	size_t max_index;
	size_t size, i, index;

	size = allocator_align(in_size);
	if (!size)
		return NULL;

	index = (size >> BOUNDARY_INDEX) - 1;
	if (index > MAXUINT32)
		return NULL;

	if (index <= allocator->max_index)
	{
		if (allocator->mutex)
			apr_thread_mutex_lock(allocator->mutex);

		max_index = allocator->max_index;
		ref = &allocator->free[index];
		i = index;
		while (*ref == NULL && i < max_index)
		{
			ref++;
			i++;
		}

		if ((node = *ref) != NULL)
		{
			if ((*ref = node->next) == NULL && i >= max_index)
			{
				do
				{
					ref--;
					max_index--;
				} while (*ref == NULL && max_index);

				allocator->max_index = max_index;
			}

			allocator->current_free_index += node->index + 1;
			if (allocator->current_free_index > allocator->max_free_index)
				allocator->current_free_index = allocator->max_free_index;

			if (allocator->mutex)
				apr_thread_mutex_unlock(allocator->mutex);

			goto have_node;
		}

		if(allocator->mutex)
			apr_thread_mutex_unlock(allocator->mutex);
	}
	else if (allocator->free[0])
	{
		if (allocator->mutex)
			apr_thread_mutex_lock(allocator->mutex);

		ref = &allocator->free[0];
		while ((node = *ref) != NULL && index > node->index)
		{
			ref = &node->next;
		}

		if (node) {
			*ref = node->next;

			allocator->current_free_index += node->index + 1;
			if (allocator->current_free_index > allocator->max_free_index)
				allocator->current_free_index = allocator->max_free_index;

			if (allocator->mutex)
				apr_thread_mutex_unlock(allocator->mutex);

			goto have_node;
		}

		if (allocator->mutex)
			apr_thread_mutex_unlock(allocator->mutex);
	}

	if ((node = (apr_memnode_t*)malloc(size)) == NULL)
		return NULL;

	node->index = index;
	node->endp = (TCHAR *)node + size;

have_node:
	node->next = NULL;
	node->first_avail = (TCHAR *)node + APR_MEMNODE_T_SIZE;

	APR_VALGRIND_UNDEFINED(node->first_avail, size - APR_MEMNODE_T_SIZE);

	return node;
}

static inline void allocator_free(apr_allocator_t *allocator, apr_memnode_t *node)
{
	apr_memnode_t *next, *freelist = NULL;
	size_t index, max_index;
	size_t max_free_index, current_free_index;

	if (allocator->mutex)
		apr_thread_mutex_lock(allocator->mutex);

	max_index = allocator->max_index;
	max_free_index = allocator->max_free_index;
	current_free_index = allocator->current_free_index;

	do {
		next = node->next;
		index = node->index;

		APR_VALGRIND_NOACCESS((char *)node + APR_MEMNODE_T_SIZE,
			(node->index + 1) << BOUNDARY_INDEX);

		if (max_free_index != APR_ALLOCATOR_MAX_FREE_UNLIMITED
			&& index + 1 > current_free_index) {
			node->next = freelist;
			freelist = node;
		}
		else if (index < MAX_INDEX) {
			if ((node->next = allocator->free[index]) == NULL
				&& index > max_index) {
				max_index = index;
			}
			allocator->free[index] = node;
			if (current_free_index >= index + 1)
				current_free_index -= index + 1;
			else
				current_free_index = 0;
		}
		else {
			node->next = allocator->free[0];
			allocator->free[0] = node;
			if (current_free_index >= index + 1)
				current_free_index -= index + 1;
			else
				current_free_index = 0;
		}
	} while ((node = next) != NULL);

	allocator->max_index = max_index;
	allocator->current_free_index = current_free_index;

	if (allocator->mutex)
		apr_thread_mutex_unlock(allocator->mutex);

	while (freelist != NULL) {
		node = freelist;
		freelist = node->next;
		free(node);
	}
}

apr_memnode_t * apr_allocator_alloc(apr_allocator_t *allocator,
	size_t size)
{
	return allocator_alloc(allocator, size);
}

void apr_allocator_free(apr_allocator_t *allocator,
	apr_memnode_t *node)
{
	allocator_free(allocator, node);
}

struct apr_pool_t
{
	apr_pool_t	*parent;
	apr_pool_t	*child;
	apr_pool_t	*sibling;
	apr_pool_t	**ref;
	apr_allocator_t *allocator;
	apr_memnode_t	*active;
	apr_memnode_t	*self;
	TCHAR			*self_first_avail;
};

#define SIZEOF_POOL_T       APR_ALIGN_DEFAULT(sizeof(apr_pool_t))

static int apr_pools_initialized = 0;
static apr_pool_t *global_pool = NULL;
static apr_allocator_t *global_allocator = NULL;

int apr_pool_initialize(void)
{
	int rv;

	if (apr_pools_initialized++)
		return 0;

	if ((rv = apr_allocator_create(&global_allocator)) != 0) {
		apr_pools_initialized = 0;
		return rv;
	}

	if ((rv = apr_pool_create_ex(&global_pool, NULL, NULL,
		global_allocator)) != 0) {
		apr_allocator_destroy(global_allocator);
		global_allocator = NULL;
		apr_pools_initialized = 0;
		return rv;
	}

	apr_thread_mutex_t *mutex;

	if ((rv = apr_thread_mutex_create(&mutex,
		APR_THREAD_MUTEX_DEFAULT,
		global_pool)) != 0) {
		return rv;
	}

	apr_allocator_mutex_set(global_allocator, mutex);

	apr_allocator_owner_set(global_allocator, global_pool);

	return 0;
}

void apr_pool_terminate(void)
{
	if (!apr_pools_initialized)
		return;

	if (--apr_pools_initialized)
		return;

	apr_pool_destory(global_pool);
	global_pool = NULL;

	global_allocator = NULL;
}

#define list_insert(node, point) do {           \
    node->ref = point->ref;                     \
    *node->ref = node;                          \
    node->next = point;                         \
    point->ref = &node->next;                   \
} while (0)

#define list_remove(node) do {                  \
    *node->ref = node->next;                    \
    node->next->ref = node->ref;                \
} while (0)

#define node_free_space(node_) ((size_t)(node_->endp - node_->first_avail))

void * apr_palloc(apr_pool_t *pool, size_t in_size)
{
	apr_memnode_t *active, *node;
	void *mem;
	size_t size, free_index;

	size = APR_ALIGN_DEFAULT(in_size);

	if (size < in_size) {
		return NULL;
	}
	active = pool->active;

	if (size <= node_free_space(active)) {
		mem = active->first_avail;
		active->first_avail += size;
		goto have_mem;
	}

	node = active->next;
	if (size <= node_free_space(node)) {
		list_remove(node);
	}
	else {
		if ((node = allocator_alloc(pool->allocator, size)) == NULL) {
			return NULL;
		}
	}

	node->free_index = 0;

	mem = node->first_avail;
	node->first_avail += size;

	list_insert(node, active);

	pool->active = node;

	free_index = (APR_ALIGN(active->endp - active->first_avail + 1,
		BOUNDARY_SIZE) - BOUNDARY_SIZE) >> BOUNDARY_INDEX;

	active->free_index = free_index;
	node = active->next;
	if (free_index >= node->free_index)
		goto have_mem;

	do {
		node = node->next;
	} while (free_index < node->free_index);

	list_remove(active);
	list_insert(active, node);

have_mem:
	return mem;
}

#ifdef apr_pcalloc
#undef apr_pcalloc
#endif

void * apr_pcalloc(apr_pool_t *pool, size_t size)
{
	void *mem;

	if ((mem = apr_palloc(pool, size)) != NULL) {
		memset(mem, 0, size);
	}

	return mem;
}

void apr_pool_clear(apr_pool_t *pool)
{
	apr_memnode_t *active;

	while (pool->child)
		apr_pool_destory(pool->child);

	active = pool->active = pool->self;
	active->first_avail = pool->self_first_avail;

	APR_IF_VALGRIND(VALGRIND_MEMPOOL_TRIM(pool, pool, 1));

	if (active->next == active) {
		return;
	}

	*active->ref = NULL;
	allocator_free(pool->allocator, active->next);
	active->next = active;
	active->ref = &active->next;
}

void apr_pool_destory(apr_pool_t *pool)
{
	apr_memnode_t *active;
	apr_allocator_t *allocator;

	while (pool->child)
		apr_pool_destory(pool->child);

	if (pool->parent) {
		apr_thread_mutex_t *mutex;

		if ((mutex = apr_allocator_mutex_get(pool->parent->allocator)) != NULL)
			apr_thread_mutex_lock(mutex);

		if ((*pool->ref = pool->sibling) != NULL)
			pool->sibling->ref = pool->ref;

		if (mutex)
			apr_thread_mutex_unlock(mutex);
	}

	allocator = pool->allocator;
	active = pool->self;
	*active->ref = NULL;

	if (apr_allocator_owner_get(allocator) == pool) {
		apr_allocator_mutex_set(allocator, NULL);
	}

	allocator_free(allocator, active);

	if (apr_allocator_owner_get(allocator) == pool) {
		apr_allocator_destroy(allocator);
	}
	APR_IF_VALGRIND(VALGRIND_DESTROY_MEMPOOL(pool));
}

int apr_pool_create_ex(apr_pool_t **newpool,
	apr_pool_t *parent,
	apr_abortfunc_t abort_fn,
	apr_allocator_t *allocator)
{
	apr_pool_t *pool;
	apr_memnode_t *node;

	*newpool = NULL;

	if (!parent)
		parent = global_pool;

	if (allocator == NULL)
		allocator = parent->allocator;

	if ((node = allocator_alloc(allocator,
		MIN_ALLOC - APR_MEMNODE_T_SIZE)) == NULL) {
		return -1;
	}

	node->next = node;
	node->ref = &node->next;

	pool = (apr_pool_t *)node->first_avail;
	pool->self_first_avail = (TCHAR *)pool + SIZEOF_POOL_T;
	node->first_avail = pool->self_first_avail;

	pool->allocator = allocator;
	pool->active = pool->self = node;
	pool->child = NULL;

	if ((pool->parent = parent) != NULL) {
		apr_thread_mutex_t *mutex;

		if ((mutex = apr_allocator_mutex_get(parent->allocator)) != NULL)
			apr_thread_mutex_lock(mutex);

		if ((pool->sibling = parent->child) != NULL)
			pool->sibling->ref = &pool->sibling;

		parent->child = pool;
		pool->ref = &parent->child;

		if (mutex)
			apr_thread_mutex_unlock(mutex);
	}
	else {
		pool->sibling = NULL;
		pool->ref = NULL;
	}

	*newpool = pool;

	return 0;
}

INT apr_pool_create_core_ex(apr_pool_t **newpool,
	apr_abortfunc_t abort_fn,
	apr_allocator_t *allocator)
{
	return apr_pool_create_unmanaged_ex(newpool, abort_fn, allocator);
}

int apr_pool_create_unmanaged_ex(apr_pool_t **newpool,
	apr_abortfunc_t abort_fn,
	apr_allocator_t *allocator)
{
	apr_pool_t *pool;
	apr_memnode_t *node;
	apr_allocator_t *pool_allocator;

	*newpool = NULL;

	if (!apr_pools_initialized)
		return -1;
	if ((pool_allocator = allocator) == NULL) {
		if ((pool_allocator = (apr_allocator_t*)malloc(SIZEOF_ALLOCATOR_T)) == NULL) {
			return -1;
		}
		memset(pool_allocator, 0, SIZEOF_ALLOCATOR_T);
		pool_allocator->max_free_index = APR_ALLOCATOR_MAX_FREE_UNLIMITED;
	}
	if ((node = allocator_alloc(pool_allocator,
		MIN_ALLOC - APR_MEMNODE_T_SIZE)) == NULL) {
		return -1;
	}

	node->next = node;
	node->ref = &node->next;

	pool = (apr_pool_t *)node->first_avail;
	node->first_avail = pool->self_first_avail = (TCHAR *)pool + SIZEOF_POOL_T;

	pool->allocator = pool_allocator;
	pool->active = pool->self = node;
	pool->child = NULL;
	pool->parent = NULL;
	pool->sibling = NULL;
	pool->ref = NULL;

	if (!allocator)
		pool_allocator->owner = pool;

	*newpool = pool;

	return 0;
}

apr_pool_t * apr_pool_parent_get(apr_pool_t *pool)
{
		return pool->parent;
}

apr_allocator_t * apr_pool_allocator_get(apr_pool_t *pool)
{
	return pool->allocator;
}

int apr_pool_is_ancestor(apr_pool_t *a, apr_pool_t *b)
{
	if (a == NULL)
		return 1;

	while (b) {
		if (a == b)
			return 1;

		b = b->parent;
	}

	return 0;
}