#include "heap.h"
#include "utils.h"


size_t strlcpy( char *dst, const char *src, size_t size )
{
	size_t	len = strlen(src);
	size_t 	ret = len;

	if ( len >= size )
		len = size-1;
	memcpy( dst, src, len );
	dst[len] = 0;

	return ret;
}

void *
debugging_calloc(size_t nmemb, size_t size, const char *file,
		 unsigned long line)
{
	void *ptr;

	assert(nmemb > 0);
	assert(size > 0);

	ptr = calloc(nmemb, size);
	//log_message( LOG_DEBUG, "{calloc: %p:%u x %u} %s:%lu", ptr, nmemb, size, file, line);

	return ptr;
}

void *
debugging_malloc(size_t size, const char *file, unsigned long line)
{
	void *ptr;

	assert(size > 0);

	ptr = malloc(size);
	//log_message( LOG_DEBUG, "{malloc: %p:%u} %s:%lu", ptr, size, file, line);

	return ptr;
}

void *
debugging_realloc(void *ptr, size_t size, const char *file, unsigned long line)
{
	void *newptr;
	
	assert(size > 0);
	
	newptr = realloc(ptr, size);
	//log_message( LOG_DEBUG, "{realloc: %p -> %p:%u} %s:%lu", ptr, newptr, size, file, line);
	return newptr;
}

void
debugging_free(void *ptr, const char *file, unsigned long line)
{
	//log_message( LOG_DEBUG, "{free: %p} %s:%lu", ptr, file, line);

	if (ptr != NULL)
		free(ptr);

	return;
}

char*
debugging_strdup(const char* s, const char* file, unsigned long line)
{
	char* ptr;
	size_t len;

	assert(s != NULL);

	len = strlen(s) + 1;
	ptr = malloc(len);
	if (!ptr)
		return NULL;
	memcpy(ptr, s, len);

	//log_message( LOG_DEBUG, "{strdup: %p:%u} %s:%lu", ptr, len, file, line);
	return ptr;
}

/*
 * Allocate a block of memory in the "shared" memory region.
 *
 * FIXME: This uses the most basic (and slowest) means of creating a
 * shared memory location.  It requires the use of a temporary file.  We might
 * want to look into something like MM (Shared Memory Library) for a better
 * solution.
 */
void*
malloc_shared_memory(size_t size)
{
	int fd;
	void* ptr;
	char buffer[32];

	static char* shared_file = "/tmp/tinyproxy.shared.XXXXXX";

	assert(size > 0);

	strlcpy(buffer, shared_file, sizeof(buffer));

	if ((fd = mkstemp(buffer)) == -1)
		return (void *)MAP_FAILED;
	unlink(buffer);

	if (ftruncate(fd, size) == -1)
		return (void *)MAP_FAILED;
	ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	return ptr;
}

/*
 * Allocate a block of memory from the "shared" region an initialize it to
 * zero.
 */
void*
calloc_shared_memory(size_t nmemb, size_t size)
{
	void* ptr;
	long length;

	assert(nmemb > 0);
	assert(size > 0);

	length = nmemb * size;

	ptr = malloc_shared_memory(length);
	if (ptr == MAP_FAILED)
		return ptr;

	memset(ptr, 0, length);

	return ptr;
}

