/**
 *  hpbuffer.h
 *  设计一个新的缓冲区数据结构
 *  支持寻找\n,遍历等功能
 *  hpghy
 *  2013/5/14
 */

#ifndef HPBUFFER_H_
#define HPBUFFER_H_

#include "common.h"

#define BLOCK_MAXDATA	1024		//block的数据区大小

struct block_s {
	struct block_s		*next;
	size_t				pos;	//有待处理数据的起点
	size_t				end;	//空闲位置起点
	char				data[BLOCK_MAXDATA];
};
typedef struct block_s		block_t;

struct hpbuffer_s {
	block_t		*head;
	block_t		*tail;
	size_t				size;		//待发送的数据总大小
};
typedef struct hpbuffer_s	hpbuffer_t;

#define BUFFER_HEAD(x) (x)->head
#define BUFFER_TAIL(x) (x)->tail

#define BLOCK_FULL( pblock )	( pblock->end == BLOCK_MAXDATA )
#define BLOCK_REMAIN( pblock )	( sizeof(pblock->data) - pblock->end )	//块的剩余空闲大小
#define BLOCK_READADDR( pblock )	( pblock->data + pblock->end )		//得到读取数据的起点
#define BLOCK_SENDADDR( pblock )	( pblock->data + pblock->pos )		//发送数据地点
#define BLOCK_SENDATA( pblock )	( pblock->end - pblock->pos )			//待发送数据的大小

extern ssize_t read_buffer( int fd, hpbuffer_t *buffptr);
extern ssize_t write_buffer( int fd, hpbuffer_t *buffptr);

extern hpbuffer_t*	create_buffer();
extern void	destroy_buffer( hpbuffer_t* );
extern int	add_block( hpbuffer_t *buffptr );

#endif
