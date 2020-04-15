#include "pch.h"
#include "my_thread_mutex.h"

struct apr_thread_mutex_t {
	apr_pool_t       *pool;
	thread_mutex_type type;
	HANDLE            handle;
	CRITICAL_SECTION  section;
};

static int thread_mutex_cleanup(void *data)
{
    apr_thread_mutex_t *lock = (apr_thread_mutex_t*)data;

    if (lock->type == thread_mutex_critical_section) {
        lock->type = (thread_mutex_type)-1;
        DeleteCriticalSection(&lock->section);
    }
    else {
        if (!CloseHandle(lock->handle)) {
			return GetLastError();
        }
    }
    return 0;
}

int apr_thread_mutex_create(apr_thread_mutex_t **mutex,
                                                  unsigned int flags,
                                                  apr_pool_t *pool)
{
    (*mutex) = (apr_thread_mutex_t *)apr_palloc(pool, sizeof(**mutex));

    (*mutex)->pool = pool;

    if (flags & APR_THREAD_MUTEX_UNNESTED) {
        (*mutex)->type = thread_mutex_unnested_event;
        (*mutex)->handle = CreateEvent(NULL, FALSE, TRUE, NULL);
    }
    else if (flags & APR_THREAD_MUTEX_TIMED) {
        (*mutex)->type = thread_mutex_nested_mutex;
        (*mutex)->handle = CreateMutex(NULL, FALSE, NULL);
    }
    else {
		InitializeCriticalSection(&(*mutex)->section);
        (*mutex)->type = thread_mutex_critical_section;
        (*mutex)->handle = NULL;
    }

    return 0;
}

int apr_thread_mutex_lock(apr_thread_mutex_t *mutex)
{
    if (mutex->type == thread_mutex_critical_section) {
        EnterCriticalSection(&mutex->section);
    }
    else {
        DWORD rv = WaitForSingleObject(mutex->handle, INFINITE);
        if ((rv != WAIT_OBJECT_0) && (rv != WAIT_ABANDONED)) {
            return (rv == WAIT_TIMEOUT) ? -1 : GetLastError();
        }
    }        
    return 0;
}

int apr_thread_mutex_unlock(apr_thread_mutex_t *mutex)
{
    if (mutex->type == thread_mutex_critical_section) {
        LeaveCriticalSection(&mutex->section);
    }
    else if (mutex->type == thread_mutex_unnested_event) {
        if (!SetEvent(mutex->handle)) {
            return GetLastError();
        }
    }
    else if (mutex->type == thread_mutex_nested_mutex) {
        if (!ReleaseMutex(mutex->handle)) {
            return GetLastError();
        }
    }
    return 0;
}

int apr_thread_mutex_destroy(apr_thread_mutex_t *mutex)
{
	return thread_mutex_cleanup(mutex);
}