/*
 * 主要是用来测试buffer_t 以及extract_ip的正确性
 */

#include <stdio.h>
#include "buffer.h"

char http[] = "fdafdafa\nfdsafafa\nGET /faxxx.icon HTTP/1.1\n\nkeep-alive:close";
char http2[] = "00/ HTTP/1.1\n\nkeep-alive:close\nfdasfdsafa\nfdsafdsaf\n";

int main() 
{
	buffer_t	*pbuf;
	block_t		*pb;
	char		ip[50];
	uint16_t	port;
	char		tmp[2048];
	int			n;

	pbuf = new_buffer();

	pb = pbuf->tail;
	memcpy( pb->data, http, strlen(http) );
	pbuf->size += strlen(http);
	pb->end += strlen(http);

	add_block( pbuf );
	pb = pbuf->tail;
	memcpy( pb->data, http2, strlen(http2) );
	pbuf->size += strlen(http2);
	pb->end += strlen(http2);

	printf("pbuf old size:%u\n", pbuf->size );

	extract_ip_buffer( pbuf, ip, sizeof(ip), &port );
	printf ("ip:%s, port:%d\n", ip, port );
	pb = pbuf->head;
	printf( "total size:%u\n", pbuf->size );
	while ( NULL != pb ) {
		n = BLOCK_SENDATA(pb);
		memcpy( tmp, BLOCK_SENDADDR(pb), n );
		tmp[n] = '\0';
		printf("%u:%s\n", n, tmp);
		pb = pb->next;
	}
	return 0;
}
