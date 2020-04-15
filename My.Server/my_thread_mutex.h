#ifndef MY_THREAD_MUTEX_H
#define MY_THREAD_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

	typedef enum thread_mutex_type {
		thread_mutex_critical_section,
		thread_mutex_unnested_event,
		thread_mutex_nested_mutex
	} thread_mutex_type;

	typedef struct apr_thread_mutex_t apr_thread_mutex_t;

#define APR_THREAD_MUTEX_DEFAULT  0x0   /**< platform-optimal lock behavior */
#define APR_THREAD_MUTEX_NESTED   0x1   /**< enable nested (recursive) locks */
#define APR_THREAD_MUTEX_UNNESTED 0x2   /**< disable nested locks */
#define APR_THREAD_MUTEX_TIMED    0x4   /**< enable timed locks */

#include "my_allocator.h"

	int apr_thread_mutex_create(apr_thread_mutex_t **mutex,
		unsigned int flags,
		apr_pool_t *pool);
	int apr_thread_mutex_lock(apr_thread_mutex_t *mutex);
	int apr_thread_mutex_unlock(apr_thread_mutex_t *mutex);
	int apr_thread_mutex_destroy(apr_thread_mutex_t *mutex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // !MY_THREAD_MUTEX_H

