/*
 * sock.c
 */

#include "sock.h"
#include "common.h"
#include "heap.h"
#include "epoll.h"
#include "buffer.h"


/*
 * addr pointer should be clear before exit
 */
listenfd_t	listenfds[IPMAXCNT];
unsigned int	listenfd_cnt;

/*
 *
 */
conn_t		conns[MAXCONNSLOTS];
conn_t		*free_conn;
unsigned int	fullcnt;

/*
 * Set the socket to non blocking -rjkaes
 */
static int socket_nonblocking(int sock)
{
	int flags;

	assert(sock >= 0);

	flags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

/*
 * Set the socket to blocking -rjkaes
 */
 /*
static int socket_blocking(int sock)
{
	int flags;

	assert(sock >= 0);

	flags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
}*/

/* 
 * hp add 2012/10/18
 */
int open_listening_sockets( config_t *pconfig )
{
	int i, opt=1;
	int fd;
	struct sockaddr_in addr;

	listenfd_cnt = pconfig->bindcnt;
	for ( i = 0; i < listenfd_cnt; ++ i ) {
		if ( strlen( pconfig->ips[i] ) > 15 ) {
			//ipv6
			//...
		}
		else { //ipv4
			fd = socket( AF_INET, SOCK_STREAM, 0 );
			if ( fd < 0 ) {
				//fprintf( stderr, "socket:%s error.\n", pconfig->ips[i] );
				log_message( LOG_ERROR, "socket error:%s.", strerror(errno) );
				return -1;
			}
	
			if ( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int) ) < 0 ) {
				//fprintf( stderr, "setsockopt REUSEADDR error.\n" );
				log_message( LOG_ERROR, "setsockopt SO_REUSEADDR error:%s.", strerror(errno) );
				close( fd );
				return -1;
			}
				
			if ( socket_nonblocking(fd) < 0 ) {
				//fprintf( stderr, "socket_nonblocking errro.\n" );
				log_message( LOG_ERROR, "socket_nonblocking error." );
				close( fd );
				return -1;
			}
			
			bzero( &addr, sizeof(addr) );
			addr.sin_family = AF_INET;
			addr.sin_port = htons( SHELLINABOXPORT );
			if ( inet_pton( AF_INET, pconfig->ips[i], &addr.sin_addr ) < 1 ) {
				//fprintf( stderr, "inet_pton error.\n");
				log_message( LOG_ERROR, "inet_pton %s error:%s.", pconfig->ips[i], strerror(errno) );
				close( fd );
				return -1;
			}

			if ( bind( fd, (struct sockaddr*)&addr, sizeof(addr) ) < 0 ) {
				//fprintf( stderr, "bind error.\n" );
				log_message( LOG_ERROR, "bind error:%s.", strerror(errno) );
				close( fd );
				return -1;
			}

			if ( listen( fd, MAXLISTEN ) < 0 ) {
				//fprintf( stderr, "listen error.\n" );
				log_message( LOG_ERROR, "listen error:%s.", strerror(errno) );
				close( fd );
				return -1;
			}
			listenfds[i].fd = fd;
			//fprintf( stderr, "create listen fd:%d\n", fd );
			log_message( LOG_NOTICE, "create listen socket:%s", pconfig->ips[i] );

			listenfds[i].addr = (struct sockaddr*)safemalloc( sizeof(addr) );
			memcpy( listenfds[i].addr, &addr, sizeof(addr) ); 
			listenfds[i].addrlen = sizeof(addr);	
		}
	}	
	return TRUE;
}

void close_listen_sockets()
{
	int i;
	for ( i = 0; i < listenfd_cnt; ++ i ) {
		close( listenfds[i].fd );
		//fprintf( stderr, "close fd:%d\n", listenfds[i].fd );
		log_message( LOG_WARNING, "close socket %s error.", listenfds[i].fd );
	}
}

/*
 *
 */
int init_conns_array( int epfd )
{
	int i;
	conn_t 	*pconn;

	for ( i = 1; i < MAXCONNSLOTS; ++ i ) {
		conns[i-1].data = &conns[i];
	}
	conns[i].data = NULL;
	free_conn = &conns[0];
	fullcnt = 0;	

	for ( i = 0; i < MAXCONNSLOTS; ++ i ) {
		conns[i].read_buffer = NULL;
		conns[i].write_buffer = NULL;
	}

	for ( i = 0; i < listenfd_cnt; ++ i ) {
		pconn = get_conns_slot();
		if ( NULL == pconn ) {
			//fprintf( stderr, "conns full.\n" );
			log_message( LOG_WARNING, "conns array full, refusing service." );
			return -1;
		}
		pconn->fd = listenfds[i].fd;
		pconn->read_handle = accept_handle;
		pconn->write_handle = NULL;
		//pconn->addr = listenfds[i].addr;
		//pconn->addrlen = listenfds[i].addrlen; 		

		epoll_add_connection( pconn, EPOLLIN );
	}

	return TRUE;
}

/*
 *
 */
conn_t* get_conns_slot()
{
	if ( NULL == free_conn )
		return NULL;

	conn_t *pconn = free_conn;
	free_conn = free_conn->data;
	++ fullcnt;
	//fprintf( stderr, "fullconns:%d.\n", fullcnt );
	log_message( LOG_NOTICE, "fullcons:%d.", fullcnt );

	return pconn;
}

/*
 *
 */
void release_conns_slot( conn_t *pconn )
{
	if ( NULL == pconn )
		return;

	if ( NULL != pconn->read_buffer ) {
		delete_buffer( pconn->read_buffer );
		pconn->read_buffer = NULL;
	}
	if ( NULL != pconn->write_buffer ) {
		delete_buffer( pconn->write_buffer );
		pconn->write_buffer = NULL;
	}
	if ( NULL != pconn->server_conn ) {
		pconn->server_conn->server_conn = NULL;
		pconn->server_conn = NULL;
	}

	/*
	 * it's very important
	 */
	close( pconn->fd );

	pconn->data = free_conn;
	free_conn = pconn;
	-- fullcnt;
	//fprintf( stderr, "fullconns:%d.\n", fullcnt );
	log_message( LOG_NOTICE, "fullcons:%d.", fullcnt );
}	

/*
 * handle accept event
 */
int accept_handle( conn_t *pconn )
{
	fprintf( stderr, "accept handle.\n" );

	int fd;
	struct sockaddr_in	addr;
	socklen_t addrlen;
	conn_t	*client_conn;
	
	fd = accept( pconn->fd, (struct sockaddr*)&addr, &addrlen );
	if ( fd < 0 ) {
		//fprintf( stderr, "accept error:%s.\n", strerror(errno) );
		log_message( LOG_WARNING, "accept error:%s.", strerror(errno) );
		return -1;
	}
	
	client_conn = get_conns_slot();
	if ( NULL == client_conn ) {
		log_message( LOG_WARNING, "get_conns_slot NULL." );
		return -2;
	}

	client_conn->fd = fd;
	client_conn->read_handle = read_client_handle;
	client_conn->write_handle = write_client_handle;
	client_conn->read_buffer = new_buffer();
	client_conn->write_buffer = new_buffer();
	client_conn->server_conn = NULL;
	//client_conn->addr = safemalloc( addrlen );
	//memcpy( newconn->addr, &addr, addrlen );
	//client_conn->addrlen = addrlen;
	//client_conn->type = C_CLIENT;

	//fprintf( stderr, "create conn:%x fd:%d.\n", client_conn, client_conn->fd );		
	char *ip = NULL;
	ip = inet_ntoa( addr.sin_addr );
	log_message( LOG_DEBUG, "create a new conn[fd:%d] client[%s]--proxy.", ip, client_conn->fd );

	epoll_add_connection( client_conn, EPOLLIN );	
	epoll_mod_connection( pconn, EPOLLIN );	

	return TRUE;
}

/*
 * pconn: client --> proxy, its buffer isn't NULL
 */
int read_client_handle( conn_t *pconn )
{
	int ret;
	char ip[50];
	uint16_t port;

	ret = read_buffer( pconn->fd, pconn->read_buffer );
	if ( ret < 0 ) {
		//log_message( LOG_ERROR, "read_client_handle client [%s:%d] error." );
		return -1;
	}
	fprintf( stderr, "read %d bytes from client.\n", ret );
	log_message( LOG_DEBUG, "read %d bytes from client.", ret );

	//process client http request
	if ( NULL == pconn->server_conn || 
	/*
     * the fd connection to server has been reused by another client.
	 * maybe has some potential problem.
	 */
		pconn->server_conn->server_conn != pconn ) {

		if ( extract_ip_buffer(pconn->read_buffer,ip, sizeof(ip),&port) < 0 ) {
			//fprintf( stderr, "extract_ip_buffer error.\n" );
			log_message( LOG_ERROR, "extract_ip_buffer error." );
			return -1;
		}
		log_message( LOG_DEBUG, "new conn--ip:%s, port:%d\n", ip, port );
	
		int fd;	
		struct sockaddr_in	addr;
		bzero( &addr, sizeof(addr) );
		addr.sin_family = AF_INET;
		addr.sin_port = htons( port );
		if ( inet_pton( AF_INET, ip, &addr.sin_addr ) < 0 ) {
			//fprintf( stderr, "inet_pton error %s.\n", strerror(errno) );
			log_message( LOG_ERROR, "inet_pton error %s.\n", strerror(errno) );
			return -1;
		}
				
		fd = socket( AF_INET, SOCK_STREAM, 0 );
		if ( fd < 0 ) {
			//fprintf( stderr, "socket error:%s.\n" );
			log_message( LOG_ERROR, "socket error:%s.\n", strerror(errno) );
			return -1;
		}
		
		if ( connect( fd, (struct sockaddr*)&addr, sizeof(addr) ) < 0 ) {
			//fprintf( stderr, "connect error.\n" );
			log_message( LOG_ERROR, "connect error:%s.\n", strerror(errno) );
			return -1;
		}
		
		conn_t * server_conn;
		server_conn = get_conns_slot();
		if ( NULL == server_conn ) {
			//fprintf( stderr, "get_conns_slot() error\n" );
			return -1;
		}
		server_conn->fd = fd;
		server_conn->read_handle = read_server_handle;
		server_conn->write_handle = write_server_handle;
		server_conn->type = C_SERVER;
		server_conn->server_conn = pconn;
		server_conn->read_buffer = NULL;
		server_conn->write_buffer = NULL;

		pconn->server_conn = server_conn;	
		//fprintf( stderr, "create conn:%x, fd:%d\n", server_conn, server_conn->fd );
		log_message( LOG_NOTICE, "create conn:%x, fd:%d\n", server_conn, server_conn->fd );

		//level trigged
		epoll_add_connection( server_conn, EPOLLOUT );
	}
	
	else {
		/*
		 * It's important too!
		 * chrome using keep-alive connection, maybe!
		 * that's why this proxy cann't dealing with requests from chrome
		 */
		 //rewrite URL in request
		extract_ip_buffer(pconn->read_buffer, ip, sizeof(ip), &port);
		fprintf( stderr, "keep-alive conn\n" );
	}
	
	/*
	 * it's very important
	 */
	uint32_t clientf = 0, serverf = 0;
	if ( buffer_size( pconn->read_buffer ) > 0 )
		serverf |= EPOLLOUT;
	if ( buffer_size( pconn->write_buffer ) < MAXBUFFSIZE )
		serverf |= EPOLLIN;
	epoll_mod_connection( pconn->server_conn, serverf );
	
	if ( buffer_size( pconn->write_buffer ) > 0 )
		clientf |= EPOLLOUT;
	if ( buffer_size( pconn->read_buffer ) < MAXBUFFSIZE )
		clientf |= EPOLLIN;
	epoll_mod_connection( pconn, clientf );

	return TRUE;
}

/*
 * pconn: client --> proxy, its buffer isn't NULL
 */
int write_client_handle( conn_t *pconn )
{
	int bytes;
	bytes = write_buffer( pconn->fd, pconn->write_buffer );
	if ( bytes < 0 ) {
		//log_message( LOG_ERROR, "write_client_handle client [%s:%d] error." );
		return -1;
	}
	fprintf( stderr, "write %d bytes to client.\n",  bytes );
	log_message( LOG_DEBUG, "write %d bytes to client.",  bytes );
		
	/*
	 * it's very important
	 */
	//epoll_mod_connection( pconn, EPOLLIN | EPOLLOUT );
	uint32_t clientf = 0, serverf = 0;
	if ( buffer_size( pconn->read_buffer ) > 0 )
		serverf |= EPOLLOUT;
	if ( buffer_size( pconn->write_buffer ) < MAXBUFFSIZE )
		serverf |= EPOLLIN;
	if ( NULL != pconn->server_conn )
		epoll_mod_connection( pconn->server_conn, serverf );
	
	if ( buffer_size( pconn->write_buffer ) > 0 )
		clientf |= EPOLLOUT;
	if ( buffer_size( pconn->read_buffer ) < MAXBUFFSIZE )
		clientf |= EPOLLIN;
	epoll_mod_connection( pconn, clientf );

	return TRUE;
}

/*
 * pconn is proxy --> server, and has no buffer.
 */
int read_server_handle( conn_t *pconn )
{
	if ( NULL == pconn->server_conn	||
		pconn->server_conn->server_conn != pconn ) {
		log_message( LOG_WARNING, "read fromserver , but client has closed connection." );
		return -1;
	}

	int bytes;
	bytes = read_buffer( pconn->fd, pconn->server_conn->write_buffer );
	if ( bytes < 0 ) {
		//log_message( LOG_ERROR, "read_server_handle server [%s:%d] error." );
		return -1;
	}
	fprintf( stderr, "read %d bytes from server.\n", bytes );
	log_message( LOG_DEBUG, "read %d bytes from server.\n", bytes );

	/*
	 * it's very important
	 */
	//epoll_mod_connection( pconn, EPOLLIN );
	//epoll_mod_connection( pconn->server_conn, EPOLLOUT );

	uint32_t clientf = 0, serverf = 0;
	if ( buffer_size( pconn->server_conn->read_buffer ) > 0 )
		serverf |= EPOLLOUT;
	if ( buffer_size( pconn->server_conn->write_buffer ) < MAXBUFFSIZE )
		serverf |= EPOLLIN;
	epoll_mod_connection( pconn, serverf );
	
	if ( buffer_size( pconn->server_conn->write_buffer ) > 0 )
		clientf |= EPOLLOUT;
	if ( buffer_size( pconn->server_conn->read_buffer ) < MAXBUFFSIZE )
		clientf |= EPOLLIN;
	epoll_mod_connection( pconn->server_conn, clientf );

	return TRUE;
}

/*
 * pconn is proxy --> server, and has no buffer.
 */
int write_server_handle( conn_t *pconn )
{
	if ( NULL == pconn->server_conn	||
		pconn->server_conn->server_conn != pconn ) {
		log_message( LOG_WARNING, "write to server [%s:%d], but client [%s:%d] has closed connection." );
		return -1;
	}

	int bytes;
	bytes = write_buffer( pconn->fd, pconn->server_conn->read_buffer );
	if ( bytes < 0 ) {
		//log_message( LOG_ERROR, "write_server_handle server [%s:%d] error." );
		return -1;
	}
	fprintf( stderr, "send to server %d bytes.\n",  bytes );
	log_message( LOG_DEBUG, "send to server %d bytes.\n",  bytes );

	/*
	 * it's very important
	 */
	//epoll_mod_connection( pconn, EPOLLIN | EPOLLOUT );
	uint32_t clientf = 0, serverf = 0;
	if ( buffer_size( pconn->server_conn->read_buffer ) > 0 )
		serverf |= EPOLLOUT;
	if ( buffer_size( pconn->server_conn->write_buffer ) < MAXBUFFSIZE )
		serverf |= EPOLLIN;
	epoll_mod_connection( pconn, serverf );
	
	if ( buffer_size( pconn->server_conn->write_buffer ) > 0 )
		clientf |= EPOLLOUT;
	if ( buffer_size( pconn->server_conn->read_buffer ) < MAXBUFFSIZE )
		clientf |= EPOLLIN;
	epoll_mod_connection( pconn->server_conn, clientf );

	return TRUE;
}
