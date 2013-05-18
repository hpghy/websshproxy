#include "hpbuffer.h"
#include "heap.h"
#include "utils.h"

/**
 *  全局变量
 */
int		g_errorno;	// 1:normal, 0: closed, -1: error


hpbuffer_t*	create_buffer() {
	hpbuffer_t	*pbuf;
	block_t		*pb;

	pbuf = (hpbuffer_t*)safemalloc( sizeof(hpbuffer_t) );
	if ( NULL == pbuf ) {
		log_message( LOG_ERROR, "safemalloc return NULL" );
		return NULL;
	}
	pb = (block_t*)safemalloc( sizeof(block_t) );
	if ( NULL == pb ) {
		log_message( LOG_ERROR, "safemalloc return NULL" );
		return NULL;
	}
	pb->next = NULL;
	pb->pos = pb->end = 0;

	pbuf->head = pbuf->tail = pb;
	pbuf->size = 0;

	return pbuf;
}

void	destroy_buffer( hpbuffer_t *pbuf ) {
	block_t		*pb;

	pb = pbuf->head;
	while ( NULL != pb ) {
		pbuf->head = pb->next;

		pb->next = NULL;
		safefree(pb);
		pb = pbuf->head;
	}
}

int	add_block( hpbuffer_t *pbuf ) {
	block_t		*npb;

	npb = (block_t*)safemalloc( sizeof(block_t) );
	if ( NULL == npb ) {
		log_message( LOG_ERROR, "safemalloc return NULL" );
		return -1;
	}
	npb->next = NULL;
	npb->pos = npb->end = 0;

	pbuf->tail->next = npb;
	pbuf->tail = npb;
}

void delete_head( hpbuffer_t *pbuf ) {
	block_t		*pb = pbuf->head;
	if ( NULL != pb ) {
		pbuf->head = pb->next;
		pbuf->size -= BLOCK_DATA(pb);
		safefree(pb);
	}
}

/**
 * 非阻塞读取数据
 * return 读取的总数据,错误设置在g_errorno上
 */
ssize_t read_buffer( int fd, hpbuffer_t *pbuf ) {

	assert(fd >= 0);
	assert( pbuf != NULL);

	block_t		*pb = NULL;
	int			n, total, size;

	total = 0;
	g_errorno = 1;				//无错误

NONREAD:
	pb = pbuf->tail;
	if ( BLOCK_FULL(pb) ) {
		if ( add_block( pbuf ) < 0 ) {
			return -1;
		}
	}
	pb = pbuf->tail;
	size = BLOCK_REMAIN(pb);	//空闲位置是 [end, BLOCK_DATA]
	n = read( fd, BLOCK_READADDR(pb), size );

	if ( n > 0 ) {
		g_errorno = 1;
		total += n;
		pb->end += n;
		pbuf->size += n;		//待发送数据大小

		if ( n == size ) {
			goto NONREAD;		//epoll的ET mode
		}
	}
	else if ( 0 == n ) {		//已经关闭读
		g_errorno = 0;
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
			g_errorno = 1;		//不是错误，本次读取结束，等待下次事件
			break;

		default:
			g_errorno = -1;
			log_message( LOG_ERROR,	"readbuff: recv() error \"%s\" on file descriptor %d",  strerror(errno), fd);
		}
	}

	return total;
}

/**
 * 非阻塞写,从第一个block到pos位置
 * return 发送的总数据量
 */
ssize_t write_buffer( int fd, hpbuffer_t *pbuf ) {
	
	assert(fd >= 0);
	assert( pbuf != NULL);

	block_t		*pb = NULL;
	int			n, total, size;
	
	total = 0;
	g_errorno = 1;
	if ( 0 == pbuf->size )
		return 0;

NONWRITE:
	pb = BUFFER_HEAD(pbuf);
	if ( 0 == BLOCK_SENDDATA(pb) ) {
		delete_head( pbuf );
	}
	pb = BUFFER_HEAD(pbuf);
	if ( NULL == pb || 0 = ( size = BLOCK_SENDDATA(pb) ) ) {	//没有数据发了
		return total;
	}
	n = send( fd, BLOCK_SENDADDR(pb), size );
	if ( n >= 0 ) {
		g_errorno = 1;
		pbuf->size -= n;
		pb->pos += n;
		total += n;
		
		if ( n == size ) {
			goto NONWRITE;			//et mode
		}
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
			g_errorno = 1;			//没有错误，等待下一次事件
			break;

		case ENOBUFS:
		case ENOMEM:
			log_message(LOG_ERROR,  "writebuff: write() error [NOBUFS/NOMEM] \"%s\" on file descriptor %d",  strerror(errno), fd);
		default:
			log_message(LOG_ERROR, "writebuff: write() error \"%s\" on file descriptor %d", strerror(errno), fd); 
			g_errorno = -1;
	}

	return total;
}

/**
 *	从url中抽取ip和端口
 *  0表示抽取成功, 
 *  -1表示不包含请求行
 *  -2表示出错
 */
int extract_ip( hpbuffer_t *pbuf, char *ip, size_t size, uint16_t *port ) {

}
