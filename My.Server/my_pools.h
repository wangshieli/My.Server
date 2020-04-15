#ifndef MY_POOLS_H
#define MY_POOLS_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

	typedef struct apr_pool_t apr_pool_t;
	typedef int(*apr_abortfunc_t)(int retcode);

	int apr_pool_initialize(void);
	void apr_pool_terminate(void);

#include "my_allocator.h"

	int apr_pool_create_ex(apr_pool_t **newpool,
		apr_pool_t *parent,
		apr_abortfunc_t abort_fn,
		apr_allocator_t *allocator);
	int apr_pool_create_core_ex(apr_pool_t **newpool,
		apr_abortfunc_t abort_fn,
		apr_allocator_t *allocator);
	int apr_pool_create_unmanaged_ex(apr_pool_t **newpool,
		apr_abortfunc_t abort_fn,
		apr_allocator_t *allocator);

#define apr_pool_create(newpool, parent) \
    apr_pool_create_ex(newpool, parent, NULL, NULL)
#define apr_pool_create_core(newpool) \
    apr_pool_create_core_ex(newpool, NULL, NULL)
#define apr_pool_create_unmanaged(newpool) \
    apr_pool_create_unmanaged_ex(newpool, NULL, NULL)

	apr_allocator_t * apr_pool_allocator_get(apr_pool_t *pool);
	void apr_pool_clear(apr_pool_t *p);
	void apr_pool_destory(apr_pool_t *p);

	void* apr_palloc(apr_pool_t *p, size_t size);
#define apr_pcalloc(p, size) memset(apr_palloc(p, size), 0, size)

	apr_pool_t* apr_pool_parent_get(apr_pool_t *pool);

	int apr_pool_is_ancestor(apr_pool_t *a, apr_pool_t *b);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !MY_POOLS_H

