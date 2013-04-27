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
				log_message( LOG_ERROR, "socket error:%s.", strerror(errno) );
				return -1;
			}
	
			if ( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int) ) < 0 ) {
				log_message( LOG_ERROR, "setsockopt SO_REUSEADDR error:%s.", strerror(errno) );
				close( fd );
				return -1;
			}
				
			if ( socket_nonblocking(fd) < 0 ) {
				log_message( LOG_ERROR, "socket_nonblocking error." );
				close( fd );
				return -1;
			}
			
			bzero( &addr, sizeof(addr) );
			addr.sin_family = AF_INET;
			addr.sin_port = htons( SHELLINABOXPORT );
			if ( inet_pton( AF_INET, pconfig->ips[i], &addr.sin_addr ) < 1 ) {
				log_message( LOG_ERROR, "inet_pton %s error:%s.", pconfig->ips[i], strerror(errno) );
				close( fd );
				return -1;
			}

			if ( bind( fd, (struct sockaddr*)&addr, sizeof(addr) ) < 0 ) {
				log_message( LOG_ERROR, "bind error:%s.", strerror(errno) );
				close( fd );
				return -1;
			}

			if ( listen( fd, MAXLISTEN ) < 0 ) {
				log_message( LOG_ERROR, "listen error:%s.", strerror(errno) );
				close( fd );
				return -1;
			}
			listenfds[i].fd = fd;
			log_message( LOG_NOTICE, "create listen socket:[%s:%d]", pconfig->ips[i], SHELLINABOXPORT );

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
		conns[i].read_closed = conns[i].write_closed = 0;
	}

	for ( i = 0; i < listenfd_cnt; ++ i ) {
		pconn = get_conns_slot();
		if ( NULL == pconn ) {
			log_message( LOG_WARNING, "conns array full, refusing service." );
			return -1;
		}
		pconn->fd = listenfds[i].fd;
		pconn->read_handle = accept_handle;
		pconn->write_handle = NULL;
		pconn->read_buffer = pconn->write_buffer = NULL;
		pconn->type = C_LISTEN;

		epoll_add_connection( pconn, EPOLLIN );

		log_message( LOG_NOTICE, "init_conns_array: add listenfd into epoll." );
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

	//只有两个conn都删除时才会delete buffer
	if ( NULL == pconn->server_conn ) {		//read_buffer write是client_conn server_conn共用的
		if ( NULL != pconn->read_buffer ) {
			delete_buffer( pconn->read_buffer );
		}
		if ( NULL != pconn->write_buffer ) {
			delete_buffer( pconn->write_buffer );
		}
	}

	pconn->read_buffer = pconn->write_buffer = NULL;
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
	log_message( LOG_NOTICE, "fullcons:%d.", fullcnt );
}	

/*
 * handle accept event
 */
int accept_handle( conn_t *pconn )
{
	int fd;
	struct sockaddr_in	addr;
	socklen_t addrlen;
	conn_t	*client_conn;
	
	fd = accept( pconn->fd, (struct sockaddr*)&addr, &addrlen );
	if ( fd < 0 ) {
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
	client_conn->addr = addr;
	client_conn->read_closed = client_conn->write_closed = 0;
	client_conn->type = C_CLIENT;

	char *ip = NULL;
	ip = inet_ntoa( addr.sin_addr );
	log_message( LOG_DEBUG, "create a new conn[fd:%d] client[%s:%d]--proxy.", ip, client_conn->fd, ntohs(addr.sin_port) );

	epoll_add_connection( client_conn, EPOLLIN );	
	epoll_mod_connection( pconn, EPOLLIN );			//重新把监听套接子添加到epoll中

	return TRUE;
}

/*
 * pconn: client --> proxy, its buffer isn't NULL
 */
int read_client_handle( conn_t *pconn )
{
	int ret;
	char ip[50];
	uint16_t	port;

	ret = read_buffer( pconn->fd, pconn->read_buffer );
	log_message( LOG_NOTICE, "read %d bytes from client.", ret );

	/**
	 *  处理读取的数据
	 */
	if ( NULL == pconn->server_conn || 
      //the fd connection to server has been reused by another client.
	  //maybe has some potential problem.
		pconn->server_conn->server_conn != pconn ) {

		//创建新的链接套接字，连接虚拟机
		memset( ip, 0, sizeof(ip) );
		//抽取VM IP:port，并重写url
		//需要再次修改这段代码
		if ( extract_ip_buffer(pconn->read_buffer, ip, sizeof(ip), &port) < 0 ) {
			log_message( LOG_ERROR, "extract_ip_buffer error." );
			return -1;
		}
		log_message( LOG_DEBUG, "new conn to Vm--ip:%s, port:%d\n", ip, port );

		int fd;	
		struct sockaddr_in	addr;
		bzero( &addr, sizeof(addr) );
		addr.sin_family = AF_INET;
		addr.sin_port = htons( port );
		if ( inet_pton( AF_INET, ip, &addr.sin_addr ) < 0 ) {
			log_message( LOG_ERROR, "inet_pton error %s.\n", strerror(errno) );
			return -1;
		}
		fd = socket( AF_INET, SOCK_STREAM, 0 );
		if ( fd < 0 ) {
			log_message( LOG_ERROR, "socket error:%s.\n", strerror(errno) );
			return -1;
		}
		if ( connect( fd, (struct sockaddr*)&addr, sizeof(addr) ) < 0 ) {
			log_message( LOG_ERROR, "connect error:%s.\n", strerror(errno) );
			return -1;
		}
		
		conn_t * server_conn;
		server_conn = get_conns_slot();
		if ( NULL == server_conn ) {
			return -1;
		}
		server_conn->fd = fd;
		server_conn->read_handle = read_server_handle;
		server_conn->write_handle = write_server_handle;
		server_conn->type = C_SERVER;
		server_conn->server_conn = pconn;
		server_conn->read_buffer = NULL;
		server_conn->write_buffer = NULL;
		server_conn->addr = addr;

		pconn->server_conn = server_conn;	

		log_message( LOG_DEBUG, "create conn to Vm[%s:%d]\n", ip, port );

		//edge trigged
		epoll_add_connection( server_conn, EPOLLOUT );
	}
	else {
		 //rewrite URL in request
		//需要再次修改这段代码
		if ( extract_ip_buffer(pconn->read_buffer, ip, sizeof(ip), &port) < 0 ) {
			log_message( LOG_ERROR, "extract_ip_buffer error, just rewrite url." );
			return -1;
		}
	}

	if ( 0 == ret  || ret < 0 ) {		//关闭或是出错
		pconn->read_closed = 1;
		if ( 1 == pconn->write_closed ) {
			return -1;		//需要删除这个连接
		}
	}

	/*
	 * it's very important
	 */
	uint32_t clientf = 0, serverf = 0;
	if ( NULL != pconn->server_conn ) {
		conn_t	*pserv_conn = pconn->server_conn;
		if ( 0 == pserv_conn->write_closed && buffer_size( pserv_conn->write_buffer ) > 0 )
			serverf |= EPOLLOUT;
		if ( 0 == pserv_conn->read_closed && buffer_size( pserv_conn->read_buffer ) < MAXBUFFSIZE )
			serverf |= EPOLLIN;
		epoll_mod_connection( pserv_conn, serverf );
	}

	if ( 0 == pconn->write_closed && buffer_size( pconn->write_buffer ) > 0 )
		clientf |= EPOLLOUT;
	if ( 0 == pconn->read_closed && buffer_size( pconn->read_buffer ) < MAXBUFFSIZE )
		clientf |= EPOLLIN;
	epoll_mod_connection( pconn, clientf );

	return 0;
}

/*
 * pconn: client --> proxy, its buffer isn't NULL
 */
int write_client_handle( conn_t *pconn )
{
	int bytes;
	bytes = write_buffer( pconn->fd, pconn->write_buffer );
	log_message( LOG_NOTICE, "write %d bytes to client.",  bytes );

	if ( bytes < 0 ) {		//写出错
		pconn->write_closed = 1;
		if ( 1 == pconn->read_closed )
			return -1;
	}
	else {
		if ( 0==buffer_size(pconn->write_buffer) &&			//发送完缓冲区中的数据
				//并且server_fd已经关闭写了
			( NULL==pconn->server_conn || 
				( NULL!=pconn->server_conn && 1 == pconn->server_conn->read_closed )) )  {
			//关闭写
			pconn->write_closed = 1;
			if ( 1 == pconn->read_closed )
				return -1;
		}
	}

	/*
	 * it's very important
	 */
	uint32_t clientf = 0, serverf = 0;
	if ( NULL != pconn->server_conn ) {
		conn_t	*pserv_conn = pconn->server_conn;
		if ( 0 == pserv_conn->write_closed && buffer_size( pserv_conn->write_buffer ) > 0 )
			serverf |= EPOLLOUT;
		if ( 0 == pserv_conn->read_closed && buffer_size( pserv_conn->read_buffer ) < MAXBUFFSIZE )
			serverf |= EPOLLIN;
		epoll_mod_connection( pserv_conn, serverf );
	}

	if ( 0 == pconn->write_closed && buffer_size( pconn->write_buffer ) > 0 )
		clientf |= EPOLLOUT;
	if ( 0 == pconn->read_closed && buffer_size( pconn->read_buffer ) < MAXBUFFSIZE )
		clientf |= EPOLLIN;
	epoll_mod_connection( pconn, clientf );

	return 0;
}

/*
 * pconn is proxy --> server, and has no buffer.
 */
int read_server_handle( conn_t *pconn )
{
	if ( NULL == pconn->server_conn	||
		pconn->server_conn->server_conn != pconn ) {
		//如果client_conn关闭了，继续读没有意义
		log_message( LOG_WARNING, "read_server_handle, but client has closed connection." );
		return -1;
	}

	if ( NULL == pconn->read_buffer ) {
		log_message( LOG_WARNING, "read_server_handle: read_buffer is null." );
		return -1;
	}

	int bytes;
	bytes = read_buffer( pconn->fd, pconn->read_buffer );
	log_message( LOG_NOTICE, "read %d bytes from server.\n", bytes );

	if ( 0 == bytes || bytes < 0 ) {
		pconn->read_closed = 1;
		if ( 1 == pconn->write_closed ) {
			return -1;		//需要删除这个连接
		}
	}

	/*
	 * it's very important
	 */
	uint32_t clientf = 0, serverf = 0;
	//写缓存中还有数据
	if ( 0 == pconn->write_closed && buffer_size( pconn->write_buffer ) > 0 )
		serverf |= EPOLLOUT;
	//读缓存未满并且未关闭读
	if ( 0 == pconn->read_closed && buffer_size( pconn->read_buffer ) < MAXBUFFSIZE )	
		serverf |= EPOLLIN;			//继续侦听读事件
	epoll_mod_connection( pconn, serverf );

	conn_t	*pclie_conn;
	if ( NULL != ( pclie_conn = pconn->server_conn ) ) {
		if ( 0 == pclie_conn->write_closed && buffer_size( pclie_conn->write_buffer ) > 0 )
			clientf |= EPOLLOUT;
		if ( 0 == pclie_conn->read_closed && buffer_size( pclie_conn->read_buffer ) < MAXBUFFSIZE )
			clientf |= EPOLLIN;
		epoll_mod_connection( pclie_conn, clientf );
	}

	return 0;
}

/*
 * pconn is proxy --> server, and has no buffer.
 */
int write_server_handle( conn_t *pconn )
{
	//如果client_conn已经关闭，还是需要把剩余数据发送给server端

	if ( NULL == pconn->write_buffer ) {
		log_message( LOG_ERROR, "write_server_handle: write_buffer is NULL." );
		return -1;
	}

	int bytes;
	bytes = write_buffer( pconn->fd, pconn->write_buffer );
	log_message( LOG_NOTICE, "send to server %d bytes.\n",  bytes );

	if ( bytes < 0 ) {
		//log_message( LOG_ERROR, "write_server_handle server [%s:%d] error." );
		return -1;
	}

	/*
	 * it's very important
	 */
	uint32_t clientf = 0, serverf = 0;
	//写缓存中还有数据
	if ( 0 == pconn->write_closed && buffer_size( pconn->write_buffer ) > 0 )
		serverf |= EPOLLOUT;
	//读缓存未满并且未关闭读
	if ( 0 == pconn->read_closed && buffer_size( pconn->read_buffer ) < MAXBUFFSIZE )	
		serverf |= EPOLLIN;			//继续侦听读事件
	epoll_mod_connection( pconn, serverf );

	conn_t	*pclie_conn;
	if ( NULL != ( pclie_conn = pconn->server_conn ) ) {
		if ( 0 == pclie_conn->write_closed && buffer_size( pclie_conn->write_buffer ) > 0 )
			clientf |= EPOLLOUT;
		if ( 0 == pclie_conn->read_closed && buffer_size( pclie_conn->read_buffer ) < MAXBUFFSIZE )
			clientf |= EPOLLIN;
		epoll_mod_connection( pclie_conn, clientf );
	}

	return TRUE;
}
