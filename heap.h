
#ifndef HP_HEAP_H
#define HP_HEAP_H

#include "common.h" 

/*
 * The following is to allow for better memory checking.
 */
#ifndef NDEBUG

extern void *debugging_calloc(size_t nmemb, size_t size, const char *file,
			      unsigned long line);
extern void *debugging_malloc(size_t size, const char *file,
			      unsigned long line);
extern void debugging_free(void *ptr, const char *file, unsigned long line);
extern void *debugging_realloc(void *ptr, size_t size, const char *file,
			       unsigned long line);
extern char *debugging_strdup(const char* s, const char* file,
			      unsigned long line);

#  define safecalloc(x, y) debugging_calloc(x, y, __FILE__, __LINE__)
#  define safemalloc(x) debugging_malloc(x, __FILE__, __LINE__)
#  define saferealloc(x, y) debugging_realloc(x, y, __FILE__, __LINE__)
#  define safestrdup(x) debugging_strdup(x, __FILE__, __LINE__)
#  define safefree(x) do { \
void **__safefree_tmp = (void *)&(x); \
debugging_free(*__safefree_tmp, __FILE__, __LINE__); \
*__safefree_tmp = NULL; \
} while (0)
#else
#  define safecalloc(x, y) calloc(x, y)
#  define safemalloc(x) malloc(x)
#  define saferealloc(x, y) realloc(x, y)
#  define safefree(x) do { \
void **__safefree_tmp = (void *)&(x); \
free(*__safefree_tmp); \
*__safefree_tmp = NULL; \
} while (0)
#  define safestrdup(x) strdup(x)
#endif

/*
 * Allocate memory from the "shared" region of memory.
 */
extern void* malloc_shared_memory(size_t size);
extern void* calloc_shared_memory(size_t nmemb, size_t size);

extern size_t strlcpy( char *dst, const char *src, size_t size );

#endif
