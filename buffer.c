#include "buffer.h"
#include "heap.h"
#include "utils.h"

/**
 *  全局变量
 */
int		g_errno;	// 1:normal, 0: closed, -1: error
int		g_linef = 0;		//1: buffer contains at least one lines;

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

buffer_t*	new_buffer() {
	buffer_t	*pbuf;
	block_t		*pb;

	pbuf = (buffer_t*)safemalloc( sizeof(buffer_t) );
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
	pbuf->blks = 1;

	return pbuf;
}

void	delete_buffer( buffer_t *pbuf ) {
	block_t		*pb;

	pb = pbuf->head;
	while ( NULL != pb ) {
		pbuf->head = pb->next;

		pb->next = NULL;
		safefree(pb);
		pb = pbuf->head;
	}
	pbuf->head = pbuf->tail = NULL;
	pbuf->size = 0;
	pbuf->blks = 0;
}

int	add_block( buffer_t *pbuf ) {
	block_t		*npb;

	npb = (block_t*)safemalloc( sizeof(block_t) );
	if ( NULL == npb ) {
		log_message( LOG_ERROR, "safemalloc return NULL" );
		return -1;
	}
	npb->next = NULL;
	npb->pos = npb->end = 0;

	if ( NULL == pbuf->tail ) {
		pbuf->tail = pbuf->head = npb;
	}
	else {
		pbuf->tail->next = npb;
		pbuf->tail = npb;
	}
	++ pbuf->blks;

	return 0;
}

/**
 * 需要优化，可以把空闲的块插入pbuf末尾
 */
void delete_head( buffer_t *pbuf ) {
	block_t		*pb = pbuf->head;
	if ( NULL == pb )
		return;

	pb->pos = pb->end = 0;
	if ( NULL == pb->next ) {		//最后一个块
		pbuf->size = 0;
	}
	else {
		pbuf->head = pb->next;
		pbuf->size -= BLOCK_SENDATA(pb);
		pb->next = NULL;
		if ( pbuf->blks < BLOCKMAXCNT ) {
			pbuf->tail->next = pb;
			pbuf->tail = pb;
		}
		else {
			safefree(pb);
			-- pbuf->blks;
		}
	}
}

/**
 * 非阻塞读取数据
 * return 读取的总数据,错误设置在g_errorno上
 */
ssize_t read_buffer( int fd, buffer_t *pbuf ) {

	assert(fd >= 0);
	assert( pbuf != NULL);

	block_t		*pb = NULL;
	int			n, total, size;

	total = 0;
	g_errno = 1;				//无错误

NONREAD:
	pb = pbuf->tail;
	if ( NULL == pb || BLOCK_FULL(pb) ) {
		if ( add_block( pbuf ) < 0 ) {
			return -1;
		}
		pb = pbuf->tail;
	}
	size = BLOCK_REMAIN(pb);	//空闲位置是 [end, BLOCK_DATA]
	n = read( fd, BLOCK_READADDR(pb), size );
	log_message( LOG_DEBUG, "read %d bytes from fd[%d]", n, fd );

	if ( n > 0 ) {
		g_errno = 1;
		total += n;
		pb->end += n;
		pbuf->size += n;		//待发送数据大小

		if ( n == size ) {
			goto NONREAD;		//epoll的ET mode
		}
	}
	else if ( 0 == n ) {		//已经关闭读
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
			g_errno = 1;		//不是错误，本次读取结束，等待下次事件
			break;

		default:
			g_errno = -1;
			log_message( LOG_ERROR,	"readbuff: recv() error \"%s\" on file descriptor %d",  strerror(errno), fd);
		}
	}

	return total;
}

/**
 * 非阻塞写,从第一个block到pos位置
 * return 发送的总数据量
 */
ssize_t write_buffer( int fd, buffer_t *pbuf ) {
	
	assert(fd >= 0);
	assert( pbuf != NULL);

	block_t		*pb = NULL;
	int			n, total, size;
	
	total = 0;
	g_errno = 1;

NONWRITE:
	if ( 0 == pbuf->size )			//结束发送
		return total;
	pb = BUFFER_HEAD(pbuf);
	if ( NULL == pb ) {
		log_message( LOG_ERROR, "head = NULL, but remain size:%d", pbuf->size );
		return total;		
	}
	if ( 0 == BLOCK_SENDATA(pb) ) {	//块没有数据发了
		delete_head( pbuf );		
		goto NONWRITE;
	}
	size = BLOCK_SENDATA(pb);
	n = send( fd, BLOCK_SENDADDR(pb), size, 0 );
	log_message( LOG_DEBUG, "send %d bytes to fd[%d]", n, fd );

	if ( n >= 0 ) {
		g_errno = 1;
		pbuf->size -= n;
		pb->pos += n;
		total += n;
		
		if ( n == size ) {			//块数据发送完,其实不应该删除，判断有没有满
			delete_head( pbuf );		
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
			g_errno = 1;			//没有错误，等待下一次事件
			break;

		case ENOBUFS:
		case ENOMEM:
			log_message(LOG_ERROR,  "writebuff: write() error [NOBUFS/NOMEM] \"%s\" on file descriptor %d",  strerror(errno), fd);
		default:
			log_message(LOG_ERROR, "writebuff: write() error \"%s\" on file descriptor %d", strerror(errno), fd); 
			g_errno = -1;
		}
	}

	return total;
}

/**
 * 把start开始的n个字符删除
 * 字符总长度为size
 */
void strremove( char *buffer, int start, int n, int size )
{
	int i;
	for ( i = start+n; i < size; ++ i ) {
		buffer[i-n] = buffer[i];
	}
	buffer[i-n] = 0;
}

char* strnstr( char *src, size_t size1, char *pattern, size_t size2 ) {
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
 *  调用时已经确保pblk中包含请求行,可以不含有HTTP/1.1 
 *  0: 表示ip抽取成功
 *  <0: 表示抽取失败
 */
int __extract_ip( buffer_t *pbuf, block_t *pblk, char *ip, size_t size, uint16_t *port ) {
	char	*pS, *p, *tp;
	int		n;
	pS = strnstr( BLOCK_SENDADDR(pblk), BLOCK_SENDATA(pblk), "GET", 3 );
	if ( NULL == pS )
		pS = strnstr( BLOCK_SENDADDR(pblk), BLOCK_SENDATA(pblk), "POST", 4 );
	if ( NULL == pS )
		return -1;

	p = memchr( pS, '/', BLOCK_SENDATA(pblk)-(int)(pS-BLOCK_SENDADDR(pblk)) );
	if ( NULL == p )
		return -2;
	tp = memchr( p, ':', BLOCK_SENDATA(pblk)-(int)(p-BLOCK_SENDADDR(pblk)) );
	if ( NULL == tp )
		return -2;

	memset( ip, 0, size );
	strncpy( ip, p+1, (tp-p)-1 );

	//判断:之前的是否是http的字段还是ip
	if ( NULL != strstr(ip, "HTTP") ) {
		//http 的字段,没有vm:port	
		*port = 0;
		memset( ip, 0, sizeof(ip) );
		return 1; 
	}

	if ( NULL != port ) {
		*port = 0;
		for ( ++tp; *tp <= '9' && *tp >= '0'; ++ tp ) {
			*port = *port * 10 + ( *tp - '0' );
		}
	}
		
	//rewrite ...
	if ( '/' == *tp )
		++ tp;
	n = tp-p-1;
	strremove( pblk->data, p+1-pblk->data, n, pblk->end );	//数据向前移动n位
	log_message( LOG_DEBUG, "remove vm:port %d bytes", n );
	pblk->end -= n;
	pbuf->size -=n;


	return 0;
}

/**
 *	从url中抽取ip和端口,有各种情况需要分析
 *  0表示抽取成功, 
 *  -1表示不包含请求行
 *  -2表示出错(含有HTTP)
 */
int extract_ip_buffer( buffer_t *pbuf, char *ip, size_t size, uint16_t *port ) {
	block_t	*pb;
	int		bytes;
	char	*pE;
	block_t		*pnb;
	pb = BUFFER_HEAD(pbuf);

	//第一个块含有请求行
	if (  NULL != strnstr( BLOCK_SENDADDR(pb), BLOCK_SENDATA(pb), "HTTP", 4 ) ) {
		return __extract_ip( pbuf, pb, ip, size, port );	
	}
	else {

		pnb = pb->next;
		if ( NULL == pnb )
			return -1;
		if ( NULL != (pE=strnstr( BLOCK_SENDADDR(pnb), BLOCK_SENDATA(pnb), "HTTP", 4))){
			char	*p1, *p2;
			p1 = memchr( BLOCK_SENDADDR(pnb), '/', pE - BLOCK_SENDADDR(pnb) );

			//如果vm:port有可能横跨两个block
			if ( NULL == p1 ) {
				goto MERGEBLOCK;
			}
			p2 = memchr( p1, ':', pE-p1 );
			if ( NULL == p2 ) {
				goto MERGEBLOCK;
			}

			//现在可以确定，vm:port在pnb块里面
			return  __extract_ip( pbuf, pnb, ip, size, port );
		}

		int last = pb->end;
		//请求行只可能在第一快或是第二个快间.(1024的大小)
		if ( ( 'H' == pb->data[last-1] && 0 == strncmp( BLOCK_SENDADDR(pnb), "TTP", 3 ) ) 
			|| ( 'H' == pb->data[last-2] && 'T' == pb->data[last-1] && 
				0 == strncmp( BLOCK_SENDADDR(pnb), "TP", 2) )
			|| ( 0 == strncmp( pb->data+last-3, "HTT", 3 ) && 'P' == pnb->data[pnb->pos] )
		   ) {
			return __extract_ip( pbuf, pb, ip, size, port );
		}

		return -1;
	}

MERGEBLOCK:
	log_message( LOG_DEBUG, "in mergeblock" );
	bytes = BLOCK_SENDATA(pb)+ pE - BLOCK_SENDADDR(pnb);
	if ( bytes <= BLOCK_MAXDATA ) {
		block_t		*newb = (block_t*)safemalloc( sizeof(block_t) );
		newb->pos = 0;
		memcpy( newb->data, BLOCK_SENDADDR(pb), BLOCK_SENDATA(pb) );
		newb->end = BLOCK_SENDATA(pb);

		memcpy( BLOCK_READADDR(newb), BLOCK_SENDADDR(pnb), pE-BLOCK_SENDADDR(pnb) );
		newb->end += pE-BLOCK_SENDADDR(pnb);
		pnb->pos += pE-BLOCK_SENDADDR(pnb);
					
		pbuf->head = newb;
		newb->next = pnb;
		safefree( pb );
					
		return __extract_ip( pbuf, newb, ip, size, port );
	}
	else {
		log_message( LOG_ERROR, "vm:port between two block!" );
		return -1;
	}
}

/**
 *	为了简单，直接写死在代码里面好了
 *  
 */
int send_error_html( struct buffer_s *pbuf ) {
	int	 len, rem;
	block_t		*pb;

	if ( NULL == BUFFER_TAIL(pbuf) ) {
		if ( add_block(pbuf) < 0 ) {
			log_message( LOG_ERROR, "send_error_html add_block error" );
			return -1;
		}
	}

	pb = pbuf->tail;
	rem = BLOCK_REMAIN(pb);		
	len = strlen(errorhtml);

	if ( rem >= len ) {
		memcpy( BLOCK_READADDR(pb), errorhtml, len );
		pb->end += len;
		pbuf->size += len;
	}
	else {
		memcpy( BLOCK_READADDR(pb), errorhtml, rem );
		pb->end += rem;
		pbuf->size += rem;
		
		add_block( pbuf );
		pb = pbuf->tail;
		memcpy( BLOCK_READADDR(pb), errorhtml+rem, len-rem );
		pb->end += len-rem;
		pbuf->size += len-rem;
	}

	return 0;
}

int send_slot_full( struct buffer_s *pbuf ) {
	int	 len, rem;
	block_t		*pb;

	if ( NULL == BUFFER_TAIL(pbuf) ) {
		if ( add_block(pbuf) < 0 ) {
			log_message( LOG_ERROR, "send_error_html add_block error" );
			return -1;
		}
	}

	pb = pbuf->tail;
	rem = BLOCK_REMAIN(pb);
	len = strlen(slothtml);

	if ( rem >= len ) {
		memcpy( BLOCK_READADDR(pb), slothtml, len );
		pb->end += len;
		pbuf->size += len;
	}
	else {
		memcpy( BLOCK_READADDR(pb), slothtml, rem );
		pb->end += rem;
		pbuf->size += rem;
		
		add_block( pbuf );
		pb = pbuf->tail;
		memcpy( BLOCK_READADDR(pb), slothtml+rem, len-rem );
		pb->end += len-rem;
		pbuf->size += len-rem;
	}
	return 0;
}
