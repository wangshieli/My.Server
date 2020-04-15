#ifndef MY_ALLOCATOR_H
#define MY_ALLOCATOR_H

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus

	typedef struct apr_allocator_t apr_allocator_t;
	typedef struct apr_memnode_t apr_memnode_t;

	struct apr_memnode_t {
		apr_memnode_t	*next;
		apr_memnode_t	**ref;
		unsigned int	index;
		unsigned int	free_index;
		TCHAR			*first_avail;
		TCHAR			*endp;
	};

#define APR_MEMNODE_T_SIZE APR_ALIGN_DEFAULT(sizeof(apr_memnode_t))

#define APR_ALLOCATOR_MAX_FREE_UNLIMITED 0

	int apr_allocator_create(apr_allocator_t **allocator);
	void apr_allocator_destroy(apr_allocator_t *allocator);
	apr_memnode_t * apr_allocator_alloc(apr_allocator_t *allocator, size_t size);
	void apr_allocator_free(apr_allocator_t *allocator, apr_memnode_t *memnode);
	size_t apr_allocator_align(apr_allocator_t *allocator, size_t size);

#include "my_pools.h"

	void apr_allocator_owner_set(apr_allocator_t *allocator, apr_pool_t *pool);
	apr_pool_t * apr_allocator_owner_get(apr_allocator_t *allocator);
	void apr_allocator_max_free_set(apr_allocator_t *allocator, size_t size);

#include "my_thread_mutex.h"

	void apr_allocator_mutex_set(apr_allocator_t *allocator, apr_thread_mutex_t *mutex);
	apr_thread_mutex_t * apr_allocator_mutex_get(apr_allocator_t *allocator);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !MY_ALLOCATOR_H

