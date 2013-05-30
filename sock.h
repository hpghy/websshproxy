/*
 * sock.h
 */

#ifndef HP_SOCK_H
#define HP_SOCK_H

#include "common.h"
#include "buffer.h"
#include "utils.h"

#define SHELLINABOXPORT		4200
#define MAXLISTEN			1024
#define MAXCONNSLOTS		1024*5

extern int	g_errno;		//0: close read or write; -1: error,close connection; 1: normal
extern int	g_linef;		//1: buffer contains at least one lines;

struct buffer_s;
struct conn_s;
typedef int ( *EPOLLHANDLE )( struct conn_s * );

struct listenfd_s {
	int		fd;
	struct sockaddr_in	addr;
	socklen_t			addrlen;
};
typedef struct listenfd_s listenfd_t;

struct conn_s {
	int		fd;
	EPOLLHANDLE			read_handle;
	EPOLLHANDLE 		write_handle;
	struct buffer_s 	*read_buffer;
	struct buffer_s		*write_buffer;
	int		server_fd;
	enum { C_CLIENT, C_SERVER, C_LISTEN } type;
	struct conn_s		*server_conn;

	/*
     * hp-modified 2013-4-24 
   	 */
	struct sockaddr_in		addr;
	//socklen_t			addrlen;

	//hp add 2013/04/26
	uint16_t	read_closed;
	uint16_t	write_closed;

	void 	*data;
};
typedef struct conn_s	conn_t;

#define CONN_CLOSE_READ(pconn)		\
	if ( 0 == pconn->read_closed && shutdown(pconn->fd,SHUT_RD) < 0 ) {		\
		log_message( LOG_WARNING, "shutdown read fd[%d] error:%s",			\
				pconn->fd, strerror(errno) );								\
	}								\
	log_message( LOG_CONN, "close read of conn[%s:%d]",						\
	inet_ntoa(pconn->addr.sin_addr), ntohs(pconn->addr.sin_port) );			\
	pconn->read_closed = 1;				

#define CONN_CLOSE_WRITE(pconn)		\
	if ( 0 == pconn->write_closed && shutdown(pconn->fd,SHUT_WR ) < 0 ) {	\
		log_message( LOG_WARNING, "shutdown write fd[%d] error:%s",			\
				pconn->fd, strerror(errno) );								\
	}								\
	log_message( LOG_CONN, "close write of conn[%s:%d]",		\
	inet_ntoa(pconn->addr.sin_addr), ntohs(pconn->addr.sin_port) );			\
	pconn->write_closed = 1;

#define ISRDCLOSED(pconn)		( 1 == pconn->read_closed )
#define ISWRCLOSED(pconn)	( 1 == pconn->write_closed )

extern int open_listening_sockets( config_t *pconfig );
extern int open_client_socket( struct sockaddr_in*, const char*, uint16_t);
extern void close_listen_sockets();

extern int init_conns_array( int );
extern conn_t* get_conns_slot();
extern void release_conns_slot( conn_t* );

int accept_handle( conn_t *pconn );
int read_client_handle( conn_t *pconn );
int write_client_handle( conn_t *pconn );
int read_server_handle( conn_t *pconn );
int write_server_handle( conn_t *pconn );
	
#endif
