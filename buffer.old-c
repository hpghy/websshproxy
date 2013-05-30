/*
 * buffer.c
 * 
 */

#include "buffer.h"
#include "heap.h"
#include "utils.h"

static char errorhtml[] = "<html>\n\
								<head>\n\
								Warning Message:\n\
								</head>\n\
								<body>\n\
		指定的虚拟机出错，连接失败，请检查url中的虚拟机IP是否有效，或与管理员联系！</p>\n\
		谢谢使用本系统。</p>\n\
		浙大CCNT实验室中国云项目组.</p>\n\
								</body>\n\
						   </html>";
static char slothtml[] = "<html>\n\
							<head>\n\
								Warning Message:\n\
							</head>\n\
							<body>\n\
					当前用户过多，请等待片刻再次连接.</p>\n\
					谢谢使用本系统。</p>\n\
					浙大CCNT实验室中国云项目组.</p>\n\
							</body>\n\
					   </html>";

 /**
 *	全局变量
 */
int	g_errno = 0;		//0: close read or write; -1: error,close connection; 1: normal
int	g_linef = 0;		//1: buffer contains at least one lines;

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
add_to_buffer(struct buffer_s *buffptr, unsigned char *data, size_t length)
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
	if ( NULL == (newline = makenewline((unsigned char*)data, length)) )
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
 * 返回读取的数据大小, g_errno设置错误提示
 */
ssize_t read_buffer(int fd, struct buffer_s * buffptr)
{
	ssize_t total;
	ssize_t bytesin;
	unsigned char buffer[READ_BUFFER_SIZE];

	assert(fd >= 0);
	assert(buffptr != NULL);

	g_errno = 1;		//没有错误 
	/*
	 * Don't allow the buffer to grow larger than MAXBUFFSIZE
	 */
	if (buffptr->size >= MAXBUFFSIZE)
		return 0;
	total = 0;
	memset( buffer, 0, sizeof(buffer) );

NONREAD:
	bytesin = read(fd, buffer, READ_BUFFER_SIZE);

	if (bytesin > 0) {
		if (add_to_buffer(buffptr, buffer, bytesin) < 0) {
			log_message( LOG_ERROR, "readbuff, add_to_buffer error." );
			g_errno = -1;
			return 0;
		}
		total += bytesin;
		if ( bytesin == READ_BUFFER_SIZE ) {		//缓冲区中还有数据
			goto NONREAD;			
	 	}	
	} 
	else {
		if (bytesin == 0) {
			/* connection was closed by client */
			log_message( LOG_ERROR, "read 0 from fd[%d].", fd );
			g_errno = 0;
		} 
		else {
			switch (errno) {
#ifdef EWOULDBLOCK
			case EWOULDBLOCK:
#else
#  ifdef EAGAIN
			case EAGAIN:
#  endif
#endif
			case EINTR:
				g_errno = 1;		//不算是错误

			default:
				log_message(LOG_ERROR, "readbuff: recv() error \"%s\" on file descriptor %d", strerror(errno), fd);
				g_errno = -2;
			}
		}
	}

	return total;
}

/*
 *  返回发送的总数据大小,错误设置在g_errno中
 */
ssize_t write_buffer(int fd, struct buffer_s * buffptr)
{
	ssize_t total, n,  bytessent;
	struct bufline_s *line;

	assert(fd >= 0);
	assert(buffptr != NULL);

	g_errno = 1;
	if (buffptr->size == 0)
		return 0;
	total = 0;

NONWRITE:
	/* Sanity check. It would be bad to be using a NULL pointer! */
	line = BUFFER_HEAD(buffptr);
	if ( NULL == line ) {
		return total;
	}
	
	n = line->length - line->pos;
	bytessent = send(fd, line->string + line->pos, n, MSG_NOSIGNAL);

	if (bytessent >= 0) {
		/* bytes sent, adjust buffer */
		line->pos += bytessent;
		total += bytessent;
		if (line->pos == line->length) {
			free_line(remove_from_buffer(buffptr));
			if ( BUFFER_HEAD(buffptr) != NULL )		//如果还有数据继续发送
				goto NONWRITE;
		}
		g_errno = 1;
	} 
	else {
		switch (errno) {
#ifdef EWOULDBLOCK
		case EWOULDBLOCK:
#else
#  ifdef EAGAIN
		case EAGAIN:
#  endif
#endif
		case EINTR:
			g_errno = 1;

		case ENOBUFS:
		case ENOMEM:
			log_message(LOG_ERROR,
				    "writebuff: write() error [NOBUFS/NOMEM] \"%s\" on file descriptor %d",
				    strerror(errno), fd);
		default:
			log_message(LOG_ERROR,
				    "writebuff: write() error \"%s\" on file descriptor %d",
				    strerror(errno), fd); 
			g_errno = -2;
		}
	}

	return total;
}

static void strremove( char *buffer, int start, int n, int size )
{
	int i;
	for ( i = start+n; i < size; ++ i ) {
		buffer[i-n] = buffer[i];
	}
	buffer[i-n] = 0;
}

static const char* strnstr( const char *src, size_t size1, const char *pattern, size_t size2 ) {
	if ( size2 > size1 )
		return NULL;
	int i;
	for ( i = 0; i <= size1 - size2; ++ i ) {
		if ( 0 == strncmp( src+i, pattern, size2 ) )
			return src+i;
	}
	return NULL;
}

/**
 * 判断缓存中是否有一行数据,都是http请求才调用这个函数
 */
int is_contain_line( struct buffer_s *pbuf ) {
	struct bufline_s	*pline;
	pline = BUFFER_HEAD(pbuf);
	while ( NULL != pline ) {
		if ( NULL != memchr( pline->string+pline->pos, '\n', pline->length ) )
			return 1;
		pline = pline->next;
	}
	return 0;
}

/**
  * 这段代码一直有一个问题，我一直没有修正
  * 需要对buffer_s重新封装一下，方便遍历没一个字符
  * return 1: ok, 0: not contain a http line, -1: format error
  */
int extract_ip_buffer( struct buffer_s *bufferptr, char *ip, ssize_t length, uint16_t *port )
{
	struct bufline_s * lines;
	const char *pGet, *pPost, *pHttp, *p, *q;

	// 2kb size, i think it's enough to contain a http request
	// test!
	lines = BUFFER_TAIL( bufferptr );
	//pGet = strstr( (char*)lines->string, "GET" );
	pGet = strnstr( (char*)lines->string, lines->length, "GET", 3 );
	if ( NULL == pGet ) {
		//pPost = strstr( (char*)lines->string, "POST" );
		pPost = strnstr( (char*)lines->string, lines->length, "POST", 4 );
		if ( NULL == pPost ) {
			return 0;
		}
		pGet = pPost;
	}

	//pHttp = strstr( (char*)lines->string, "HTTP" );
	pHttp = strnstr( (char*)lines->string, lines->length, "HTTP", 4 );
	if ( NULL == pHttp ) {
		return 0;
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
	int n, start;
	n = q-p-1;
	if ( *q == '/' )
		++ n;
	start = (unsigned char*)p - lines->string + 1;
	strremove( (char*)lines->string, start, n, lines->length );
	lines->length -= n;
	bufferptr->size -= n;

	return TRUE;
}	

/**
 *	为了简单，直接写死在代码里面好了
 */
int send_error_html( struct buffer_s *pbuf ) {
	if (add_to_buffer( pbuf, (unsigned char*)errorhtml, strlen(errorhtml) ) < 0)  {
		log_message( LOG_ERROR, "send_error_html error." );
		return -1;
	}
	return 0;
}

int send_slot_full( struct buffer_s *pbuf ) {
	if (add_to_buffer( pbuf, (unsigned char*)slothtml, strlen(slothtml) ) < 0)  {
		log_message( LOG_ERROR, "send_slot_full error." );
		return -1;
	}
	return 0;
}
