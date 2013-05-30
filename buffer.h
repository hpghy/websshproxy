/**
 *  buffer.h
 *  设计一个新的缓冲区数据结构
 *  支持寻找\n,遍历等功能
 *  hpghy
 *  2013/5/14
 */

#ifndef HPBUFFER_H_
#define HPBUFFER_H_

#include "common.h"

#define BLOCK_MAXDATA	(1024*4)		//block的数据区大小
#define BLOCKMAXCNT		10		
#define MAXBUFFSIZE		(1024*96)

struct block_s {
	struct block_s		*next;
	size_t				pos;	//有待处理数据的起点,
	size_t				end;	//空闲位置起点
	char				data[BLOCK_MAXDATA];
};
typedef struct block_s		block_t;

struct buffer_s {
	block_t		*head;
	block_t		*tail;
	size_t				size;		//待发送的数据总大小
	size_t				blks;		//块个数
};
typedef struct buffer_s	buffer_t;

#define buffer_size(x) (x)->size
#define BUFFER_HEAD(x) (x)->head
#define BUFFER_TAIL(x) (x)->tail

#define BLOCK_FULL( pblock )	( pblock->end == BLOCK_MAXDATA )
#define BLOCK_REMAIN( pblock )	( sizeof(pblock->data) - pblock->end )	//块的剩余空闲大小
#define BLOCK_READADDR( pblock )	( pblock->data + pblock->end )		//得到读取数据的起点
#define BLOCK_SENDADDR( pblock )	( pblock->data + pblock->pos )		//发送数据地点
#define BLOCK_SENDATA( pblock )	( pblock->end - pblock->pos )			//待发送数据的大小

extern ssize_t read_buffer( int fd, buffer_t *buffptr);
extern ssize_t write_buffer( int fd, buffer_t *buffptr);

extern buffer_t*	new_buffer();
extern void	delete_buffer( buffer_t* );
extern int	add_block( buffer_t *buffptr );
extern int extract_ip_buffer( buffer_t *pbuf, char *ip, size_t size, uint16_t *port );

extern void strremove( char *, int, int, int );
extern char *strnstr( char*, size_t, char*, size_t );

extern int send_error_html( struct buffer_s *pbuf );
extern int send_slot_full( struct buffer_s *pbuf );

#endif
