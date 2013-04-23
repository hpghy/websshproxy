/*
 * buffer.c
 * 
 */

#include "buffer.h"
#include "heap.h"
#include "utils.h"


/*
 * Take a string of data and a length and make a new line which can be added
 * to the buffer. The data IS copied, so make sure if you allocated your
 * data buffer on the heap, delete it because you now have TWO copies.
 */
static struct bufline_s *
makenewline(unsigned char *data, size_t length)
{
	struct bufline_s *newline;

	assert(data != NULL);
	assert(length > 0);

	if (!(newline = safemalloc(sizeof(struct bufline_s))))
		return NULL;

	if (!(newline->string = safemalloc(length))) {
		safefree(newline);
		return NULL;
	}

	memcpy(newline->string, data, length);

	newline->next = NULL;
	newline->length = length;

	/* Position our "read" pointer at the beginning of the data */
	newline->pos = 0;

	return newline;
}

/*
 * Free the allocated buffer line
 */
static void
free_line(struct bufline_s *line)
{
	assert(line != NULL);

	if (!line)
		return;

	if (line->string)
		safefree(line->string);

	safefree(line);
}

/*
 * Create a new buffer
 */
struct buffer_s *
new_buffer(void)
{
	struct buffer_s *buffptr;

	if (!(buffptr = safemalloc(sizeof(struct buffer_s))))
		return NULL;

	/*
	 * Since the buffer is initially empty, set the HEAD and TAIL
	 * pointers to NULL since they can't possibly point anywhere at the
	 * moment.
	 */
	BUFFER_HEAD(buffptr) = BUFFER_TAIL(buffptr) = NULL;
	buffptr->size = 0;

	return buffptr;
}

/*
 * Delete all the lines in the buffer and the buffer itself
 */
void
delete_buffer(struct buffer_s *buffptr)
{
	struct bufline_s *next;

	assert(buffptr != NULL);

	while (BUFFER_HEAD(buffptr)) {
		next = BUFFER_HEAD(buffptr)->next;
		free_line(BUFFER_HEAD(buffptr));
		BUFFER_HEAD(buffptr) = next;
	}

	safefree(buffptr);
}

/*
 * Return the current size of the buffer.
 */
size_t buffer_size(struct buffer_s *buffptr)
{
	return buffptr->size;
}

/*
 * Push a new line on to the end of the buffer.
 */
int
add_to_buffer(struct buffer_s *buffptr, char *data, size_t length)
{
	struct bufline_s *newline;

	assert(buffptr != NULL);
	assert(data != NULL);
	assert(length > 0);

	/*
	 * Sanity check here. A buffer with a non-NULL head pointer must
	 * have a size greater than zero, and vice-versa.
	 */
	if (BUFFER_HEAD(buffptr) == NULL)
		assert(buffptr->size == 0);
	else
		assert(buffptr->size > 0);

	/*
	 * Make a new line so we can add it to the buffer.
	 */
	if (!(newline = makenewline(data, length)))
		return -1;

	if (buffptr->size == 0)
		BUFFER_HEAD(buffptr) = BUFFER_TAIL(buffptr) = newline;
	else {
		BUFFER_TAIL(buffptr)->next = newline;
		BUFFER_TAIL(buffptr) = newline;
	}

	buffptr->size += length;

	return 0;
}

/*
 * Remove the first line from the top of the buffer
 */
static struct bufline_s *
remove_from_buffer(struct buffer_s *buffptr)
{
	struct bufline_s *line;

	assert(buffptr != NULL);
	assert(BUFFER_HEAD(buffptr) != NULL);

	line = BUFFER_HEAD(buffptr);
	BUFFER_HEAD(buffptr) = line->next;

	buffptr->size -= line->length;

	return line;
}

/*
 * Reads the bytes from the socket, and adds them to the buffer.
 * Takes a connection and returns the number of bytes read.
 * > 0: read bytes; == 0: eagain; < 0: error, connection closed 
 */
#define READ_BUFFER_SIZE (1024 * 5)
ssize_t
read_buffer(int fd, struct buffer_s * buffptr)
{
	ssize_t total;
	ssize_t bytesin;
	unsigned char buffer[READ_BUFFER_SIZE];

	assert(fd >= 0);
	assert(buffptr != NULL);

	/*
	 * Don't allow the buffer to grow larger than MAXBUFFSIZE
	 */
	if (buffptr->size >= MAXBUFFSIZE)
		return 0;
	total = 0;

NONREAD:
	bytesin = read(fd, buffer, READ_BUFFER_SIZE);

	if (bytesin > 0) {
		if (add_to_buffer(buffptr, buffer, bytesin) < 0) {
			//fprintf( stderr, "readbuff: add_to_buffer() error\n" );
			log_message( LOG_ERROR, "readbuff, add_to_buffer error." );
			return -1;
		}
		
		total += bytesin;
		/*
		if ( bytesin == READ_BUFFER_SIZE ) {
			goto NONREAD;			
	 	}*/	
		return total;

	} else {
		if (bytesin == 0) {
			/* connection was closed by client */
			//hp:
			//fprintf( stderr, "readbuffer: conn closed by client\n" );
			log_message( LOG_NOTICE, "connection [fd:%d] closed by client.", fd );
			return -1;
		} else {
			switch (errno) {
#ifdef EWOULDBLOCK
			case EWOULDBLOCK:
#else
#  ifdef EAGAIN
			case EAGAIN:
				fprintf( stderr, "stderr, EAGAIN signal.\n" );
#  endif
#endif
			case EINTR:
				return 0;
			default:
				//log_message(LOG_ERR,
				//	    "readbuff: recv() error \"%s\" on file descriptor %d",
				//	    strerror(errno), fd);
				//hp:
				//fprintf( stderr, 	
				//	    "readbuff: recv() error \"%s\" on file descriptor %d",
				//	    strerror(errno), fd);
				return -1;
			}
		}
	}
}

/*
 * Write the bytes in the buffer to the socket.
 * Takes a connection and returns the number of bytes written.
 */
ssize_t
write_buffer(int fd, struct buffer_s * buffptr)
{
	ssize_t total, n,  bytessent;
	struct bufline_s *line;

	assert(fd >= 0);
	assert(buffptr != NULL);

	if (buffptr->size == 0)
		return 0;
	total = 0;

NONWRITE:
	/* Sanity check. It would be bad to be using a NULL pointer! */
	assert(BUFFER_HEAD(buffptr) != NULL);
	line = BUFFER_HEAD(buffptr);
	
	n = line->length - line->pos;
	bytessent = send(fd, line->string + line->pos, n, MSG_NOSIGNAL);

	if (bytessent >= 0) {
		/* bytes sent, adjust buffer */
		line->pos += bytessent;
		total += bytessent;
		if (line->pos == line->length) {
			free_line(remove_from_buffer(buffptr));
			
			/*
			if ( BUFFER_HEAD(buffptr) != NULL )
				goto NONWRITE;
			*/
		}
		return total;

	} else {
		switch (errno) {
#ifdef EWOULDBLOCK
		case EWOULDBLOCK:
#else
#  ifdef EAGAIN
		case EAGAIN:
#  endif
#endif
		case EINTR:
			fprintf( stderr, "write_buffer eagain.\n" );
			return 0;
		case ENOBUFS:
		case ENOMEM:
			/*
			log_message(LOG_ERR,
				    "writebuff: write() error [NOBUFS/NOMEM] \"%s\" on file descriptor %d",
				    strerror(errno), fd); */
			return 0;
		default:
			/*
			log_message(LOG_ERR,
				    "writebuff: write() error \"%s\" on file descriptor %d",
				    strerror(errno), fd); */
			return -1;
		}
	}
}

static void strremove( char *buffer, int start, int n, int size )
{
	int i;
	
	for ( i = start+n; i < size; ++ i ) {
		buffer[i-n] = buffer[i];
	}
	buffer[i-n] = 0;
}
/*
 * extract IP:port from http request
 * a potential problem: maybe not a complete http request!
 */
int extract_ip_buffer( struct buffer_s *bufferptr, char *ip, ssize_t length, uint16_t *port )
{
	struct bufline_s * lines;
	char *pGet, *pPost, *pHttp, *p, *q;

	// 2kb size, i think it's enough to contain a http request
	// test!
	lines = BUFFER_TAIL( bufferptr );
	pGet = strstr( lines->string, "GET" );
	if ( NULL == pGet ) {
		pPost = strstr( lines->string, "POST" );
		if ( NULL == pPost ) {
			return -1;
		}
		pGet = pPost;
	}

	pHttp = strstr( lines->string, "HTTP" );
	if ( NULL == pHttp ) {
		return -1;
	} 
	
	p = strchr( pGet, '/' );	
	q = strchr( p, ':' );
	if ( NULL == p || NULL == q )
		return -1;

	strncpy( ip, p+1, q-p-1 );
	ip[ q-p-1 ] = 0;	

	if ( NULL == port )
		return TRUE;
	*port = 0;
	for ( ++q; *q <= '9' && *q >= '0'; ++ q ) {
		*port = *port * 10 + ( *q - '0' );
	}

	//rewrite url
	//fprintf( stderr, "before: bytes:%d,%s\n", lines->length, lines->string );
	int n, start;
	n = q-p-1;
	if ( *q == '/' )
		++ n;
	start = (unsigned char*)p - lines->string + 1;
	strremove( lines->string, start, n, lines->length );
	lines->length -= n;
	bufferptr->size -= n;
	//fprintf( stderr, "after: bytes:%d,%s\n", lines->length, lines->string );

	return TRUE;
}	
