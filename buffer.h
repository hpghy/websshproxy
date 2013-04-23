/*
 * buffer.h
 *
 */

#ifndef HP_BUFFER_H
#define HP_BUFFER_H

#include "common.h"

#define MAXBUFFSIZE	(1024*96)
#define BUFFER_HEAD(x) (x)->head
#define BUFFER_TAIL(x) (x)->tail

struct bufline_s {
	unsigned char *string;	/* the actual string of data */
	struct bufline_s *next;	/* pointer to next in linked list */
	size_t length;		/* length of the string of data */
	size_t pos;		/* start sending from this offset */
};

/*
 * The buffer structure points to the beginning and end of the buffer list
 * (and includes the total size)
 */
struct buffer_s {
	struct bufline_s *head;	/* top of the buffer */
	struct bufline_s *tail;	/* bottom of the buffer */
	size_t size;		/* total size of the buffer */
};

extern struct buffer_s *new_buffer(void);
extern void delete_buffer(struct buffer_s *buffptr);
extern size_t buffer_size(struct buffer_s *buffptr);
extern int add_to_buffer(struct buffer_s *buffptr, char *data, size_t length);

extern ssize_t read_buffer(int fd, struct buffer_s *buffptr);
extern ssize_t write_buffer(int fd, struct buffer_s *buffptr);

extern int extract_ip_buffer( struct buffer_s *bufferptr, 
			char *ip, ssize_t length, uint16_t *port );


#endif				/* __BUFFER_H_ */
