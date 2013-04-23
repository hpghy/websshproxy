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
#define MAXCONNSLOTS		1024*2

struct buffer_s;
struct conn_s;
typedef int ( *EPOLLHANDLE )( struct conn_s * );

struct listenfd_s {
	int		fd;
	struct sockaddr		*addr;
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
     * have no effection
   	 */
	struct sockaddr		*addr;
	socklen_t			addrlen;

	void 	*data;
};
typedef struct conn_s	conn_t;


extern int open_listening_sockets( config_t *pconfig );
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
