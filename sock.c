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
 * 主进程调用
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
			log_message( LOG_DEBUG, "create listen socket:[%s:%d]", pconfig->ips[i], SHELLINABOXPORT );

			//listenfds[i].addr = (struct sockaddr*)safemalloc( sizeof(addr) );
			memcpy( &listenfds[i].addr, &addr, sizeof(addr) ); 
			listenfds[i].addrlen = sizeof(addr);	
		}
	}	
	return TRUE;
}

int open_client_socket( struct sockaddr_in* paddr, const char* ip, uint16_t port ) {
	int fd;
	bzero( paddr, sizeof(struct sockaddr_in) );
	paddr->sin_family = AF_INET;
	paddr->sin_port = htons( port );
	if ( inet_pton( AF_INET, ip, &paddr->sin_addr ) < 0 ) {
		log_message( LOG_ERROR, "inet_pton error %s.\n", strerror(errno) );
		return -1;
	}
	fd = socket( AF_INET, SOCK_STREAM, 0 );
	if ( fd < 0 ) {
		log_message( LOG_ERROR, "socket error:%s.\n", strerror(errno) );
		return -1;
	}
	if ( connect( fd, (struct sockaddr*)paddr, sizeof(struct sockaddr_in) ) < 0 ) {
		log_message( LOG_ERROR, "connect error:%s.\n", strerror(errno) );
		return -1;
	}

	return fd;
}

void close_listen_sockets()
{
	int i;
	for ( i = 0; i < listenfd_cnt; ++ i ) {
		close( listenfds[i].fd );
		log_message( LOG_WARNING, "close socket %d error.", listenfds[i].fd );
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
	conns[i-1].data = NULL;
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

		log_message( LOG_DEBUG, "init_conns_array: add listenfd into epoll." );
	}

	return TRUE;
}

/*
 *
 */
conn_t* get_conns_slot()
{
	if ( NULL == free_conn ) {
		log_message( LOG_WARNING, "fullcons:%d is full.", fullcnt );
		return NULL;
	}

	conn_t *pconn = free_conn;
	pconn->read_closed = pconn->write_closed = 0;
	free_conn = free_conn->data;
	++ fullcnt;
	log_message( LOG_DEBUG, "fullcons:%d.", fullcnt );

	if ( NULL != pconn->read_buffer ) {
		log_message( LOG_ERROR, "read_buffer not release" );
		delete_buffer( pconn->read_buffer);
		pconn->read_buffer = NULL;
	}
	if ( NULL != pconn->write_buffer ) {
		log_message( LOG_ERROR, "write_buffer not release" );
		delete_buffer( pconn->write_buffer);
		pconn->write_buffer = NULL;
	}

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
	close( pconn->fd );		//必定不是监听套接字,只有一个进程使用这个fd

	pconn->data = free_conn;
	free_conn = pconn;
	-- fullcnt;

	log_message( LOG_CONN, "release conn[%s:%d:%d], fullcons:%d.", inet_ntoa(pconn->addr.sin_addr),
			ntohs(pconn->addr.sin_port), pconn->fd, fullcnt );
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

	bzero( &addr, sizeof(struct sockaddr_in) );
	addrlen = sizeof(struct sockaddr_in);
	fd = accept( pconn->fd, (struct sockaddr*)&addr, &addrlen );
	epoll_mod_connection( pconn, EPOLLIN );			//重新把监听套接子添加到epoll中

	if ( fd < 0 ) {
		log_message( LOG_WARNING, "accept error:%s.", strerror(errno) );
		return -1;		//惊群现象
	}
	
	client_conn = get_conns_slot();
	if ( NULL == client_conn ) {
		log_message( LOG_WARNING, "get_conns_slot NULL." );
		return -2;
	}

	client_conn->fd = fd;
	client_conn->read_handle = read_client_handle;
	client_conn->write_handle = write_client_handle;
	client_conn->read_buffer = new_buffer();		//这里有内存泄漏
	client_conn->write_buffer = new_buffer();		//这里有内存泄漏
	client_conn->server_conn = NULL;
	client_conn->addr = addr;
	client_conn->read_closed = client_conn->write_closed = 0;
	client_conn->type = C_CLIENT;

	char *ip = NULL;
	ip = inet_ntoa( addr.sin_addr );
	log_message( LOG_CONN, "create a new conn[fd:%d] client[%s:%d]--proxy.", client_conn->fd, ip, ntohs(addr.sin_port) );

	epoll_add_connection( client_conn, EPOLLIN );	
	return TRUE;
}

/*
 * pconn: client --> proxy, its buffer isn't NULL
 */
int read_client_handle( conn_t *pconn )
{
	log_message( LOG_DEBUG, "in read_client_handle" );

	int ret;
	char ip[100];				//有可能不是ip
	uint16_t	port;

	if ( 1 == pconn->read_closed ) {		//因为某种原因不想读取client数据了
		log_message( LOG_WARNING, "try reading from client[%s], but it closed read", inet_ntoa( pconn->addr.sin_addr ) );
		return -1;							
	}

	ret = read_buffer( pconn->fd, pconn->read_buffer );
	log_message( LOG_DEBUG, "read %d bytes from client[%s:%d].", ret, inet_ntoa(pconn->addr.sin_addr), ntohs(pconn->addr.sin_port) );

	if ( g_errno <= 0 ) {		//read occur error, or closed
		log_message( LOG_CONN, "read occur error, close read from client[%s:%d].", inet_ntoa(pconn->addr.sin_addr), ntohs(pconn->addr.sin_port) );
		CONN_CLOSE_READ( pconn );

		/**
		 *  client关闭写，并且buffer没有数据了，proxy也要关闭server的写
		 *  write_server的事情
		 *  发现问题了，这里要判断是否删除pconn->server_conn
		 */
		if ( NULL != pconn->server_conn && 0 == buffer_size( pconn->read_buffer ) ) {
			CONN_CLOSE_WRITE( pconn->server_conn );

			if ( 1 == pconn->server_conn->read_closed ) {		//very important
				epoll_del_connection( pconn->server_conn );
				release_conns_slot( pconn->server_conn );
			}
		}
		/**
		 *  当客户端关闭写，并且没有VM，(或者VM关闭写并且buffer空了，这时就该关闭连接)--write_client_handle有处理
		 */
		if ( 1 == pconn->write_closed || NULL == pconn->server_conn )
			return -1;			//删除pconn
	}

	/**
	 * 这个很重要，不然容易导致工作进程突然死掉
	 */
	if ( /*1 == pconn->read_closed &&*/ 0 == buffer_size( pconn->read_buffer ) ) {
		//return 0;		//没有数据，不需要进入下一步
		goto CLIENT_EPOLLSET;
	}

	/**
	 *  处理读取的数据
	 */

	if ( NULL == pconn->server_conn || 
      //the fd connection to server has been reused by another client.
	  //maybe has some potential problem.
		pconn->server_conn->server_conn != pconn ) {

		memset( ip, 0, sizeof(ip) );

		//抽取VM IP:port，并重写url 需要再次修改这段代码
		ret = extract_ip_buffer(pconn->read_buffer, ip, sizeof(ip), &port);
		if ( ret < 0 ) {
			log_message( LOG_WARNING, "extract_ip_buffer not find vm ip:port" );
			//return 0;				//等待新的数据到来
			goto CLIENT_EPOLLSET;
		}

		int fd;	
		struct sockaddr_in	addr;

NEWCONN:
		fd = open_client_socket( &addr, ip, port );
		if ( fd < 0 ) {
			//发送error.html
			log_message( LOG_WARNING, "conn to Vm[%s:%d] failed.", ip, port );

			if ( '\0' == ip[0] || 0 == port ) {			//just for test
				char tmp[1024];
				memset( tmp, 0, sizeof(tmp) );
				memcpy( tmp, BLOCK_SENDADDR(pconn->read_buffer->head), 100 );
				log_message( LOG_ERROR, "ip error:%s", tmp );
			}

			if ( send_error_html( pconn->write_buffer ) < 0 )
				return -1;				//需要修改epll
			goto CLIENT_EPOLLSET;
		}
		
		conn_t * server_conn;
		server_conn = get_conns_slot();
		if ( NULL == server_conn ) {
			log_message( LOG_WARNING, "conn slot full." );
			if ( send_slot_full( pconn->write_buffer ) < 0 )
				return -1;				//需要修改epll
			goto CLIENT_EPOLLSET;
		}
		server_conn->fd = fd;
		server_conn->read_handle = read_server_handle;
		server_conn->write_handle = write_server_handle;
		server_conn->type = C_SERVER;
		server_conn->server_conn = pconn;
		server_conn->read_buffer = pconn->write_buffer;
		server_conn->write_buffer = pconn->read_buffer;
		server_conn->addr = addr;
		pconn->server_conn = server_conn;	

		log_message( LOG_CONN, "client[%s:%d] conn to Vm[%s:%d:%d]\n", inet_ntoa(pconn->addr.sin_addr), ntohs(pconn->addr.sin_port), ip, port, fd );

		epoll_add_connection( server_conn, EPOLLIN|EPOLLOUT );
	}
	else {			//这里假设http请求可以一次读完,以后需要再次修改

		 //rewrite URL in request
		ret = extract_ip_buffer(pconn->read_buffer, ip, sizeof(ip), &port);
		if ( ret < 0 ) {		//表示没有请求行或是格式出错
		
			epoll_mod_connection( pconn->server_conn, EPOLLIN );	//等待server读事件
			log_message( LOG_ERROR, "extract_ip_buffer error, just rewrite url." );
			//return 0;			//没有加入epollout中，因为需要等待更多数据的到来
			goto CLIENT_EPOLLSET;
		}

		//判断当前server是否与url中的vm匹配，如果不匹配，则需要把整个连接删除
		// ip=0, 表示符合格式，但是没有包含vm:port，有可能已经删除过了
		if ( '\0' != ip[0] && 0 != strcasecmp( ip, 
					inet_ntoa(pconn->server_conn->addr.sin_addr) ) ) {
			epoll_del_connection( pconn->server_conn );
			release_conns_slot( pconn->server_conn );
			pconn->server_conn = NULL;
			//log_message( LOG_ERROR, "hp, goto NEWCOM" );
			goto NEWCONN;		//与新的vm创建连接
		}

		epoll_mod_connection( pconn->server_conn, EPOLLIN|EPOLLOUT );
	}

	/*
	 * it's very important
	 */
	uint32_t clientf = 0;
CLIENT_EPOLLSET:
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
	log_message( LOG_DEBUG, "in write_client_handle" );

	if ( 1 == pconn->write_closed ) {
		log_message( LOG_WARNING, "try writing to client[%s:%d], but it closed write", inet_ntoa(pconn->addr.sin_addr), ntohs(pconn->addr.sin_port) );
		return -1;
	}

	int bytes;
	bytes = write_buffer( pconn->fd, pconn->write_buffer );
	log_message( LOG_DEBUG, "write %d bytes to client[%s:%d].", bytes, inet_ntoa(pconn->addr.sin_addr), ntohs(pconn->addr.sin_port) );

	if ( g_errno < 0 ) {		//写出错
		CONN_CLOSE_WRITE( pconn );
		if ( 1 == pconn->read_closed )
			return -1;
	}
	else {
		if ( 0==buffer_size(pconn->write_buffer) &&			//发送完缓冲区中的数据
			( NULL==pconn->server_conn ||					//并且server已经不再写
				( NULL!=pconn->server_conn && 1 == pconn->server_conn->read_closed )) )  {
			CONN_CLOSE_WRITE( pconn );
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
	log_message( LOG_DEBUG, "in read_server_handle" );

	if ( 1 == pconn->read_closed ) {
		log_message( LOG_WARNING, "try reading from server[%s], but it closed read", inet_ntoa(pconn->addr.sin_addr) );
		return -1;
	}
	if ( NULL == pconn->server_conn	||				//如果client_conn关闭了，继续读没有意义
		pconn->server_conn->server_conn != pconn ) {
		log_message( LOG_WARNING, "read_server_handle, but client has closed connection." );
		CONN_CLOSE_READ( pconn );
		return -1;
	}
	if ( NULL == pconn->read_buffer ) {
		log_message( LOG_WARNING, "read_server_handle: read_buffer is null." );
		CONN_CLOSE_READ( pconn );
		return -1;
	}

	int bytes;
	bytes = read_buffer( pconn->fd, pconn->read_buffer );
	log_message( LOG_DEBUG, "read %d bytes from server[%s:%d].\n", bytes, inet_ntoa(pconn->addr.sin_addr), ntohs(pconn->addr.sin_port) );

	if ( g_errno <= 0 ) {
		CONN_CLOSE_READ( pconn );
		/**
		 *  server关闭写，并且buffer没有数据了，proxy也要关闭对client的写
		 *  好像是write_client的工作
		 *  同read_client_handle
		 */
		if ( NULL != pconn->server_conn && 0 == buffer_size( pconn->read_buffer ) ) {
			CONN_CLOSE_WRITE( pconn->server_conn );

			if ( 1 == pconn->server_conn->read_closed ) {		//very important
				epoll_del_connection( pconn->server_conn );
				release_conns_slot( pconn->server_conn );
			}
		}

		/**
		 *	如果server关闭写，并且client已经没有了，(或者client也关闭写并且buffer空了)--write_server_handle
		 *  需要删除这个连接
		 */
		if ( 1 == pconn->write_closed || NULL == pconn->server_conn ) {
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
	log_message( LOG_DEBUG, "in write_server_handle" );

	//如果client_conn已经关闭，还是需要把剩余数据发送给server端
	if ( NULL == pconn->write_buffer ) {
		log_message( LOG_ERROR, "write_server_handle: write_buffer is NULL." );
		return -1;
	}
	if ( 1 == pconn->write_closed ) {
		log_message( LOG_WARNING, "try writing to server[%s] fd[%d], but it closed write", 
				inet_ntoa(pconn->addr.sin_addr), pconn->fd );
		return -1;
	}

	int bytes;
	bytes = write_buffer( pconn->fd, pconn->write_buffer );
	log_message( LOG_DEBUG, "send to server[%s:%d] %d bytes.\n",  inet_ntoa(pconn->addr.sin_addr), ntohs(pconn->addr.sin_port), bytes );

	if ( g_errno < 0 ) {					//写出错
		CONN_CLOSE_WRITE( pconn );
		if ( 1 == pconn->read_closed )
			return -1;
	}
	else {
		if ( 0==buffer_size(pconn->write_buffer) &&			//发送完缓冲区中的数据
			( NULL==pconn->server_conn ||					//并且客户端不会再有数据过来
				1 == pconn->server_conn->read_closed ) )  {
			CONN_CLOSE_WRITE( pconn );
			if ( 1 == pconn->read_closed )
				return -1;
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

	return TRUE;
}
