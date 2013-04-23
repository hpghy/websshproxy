/*
 *
 */

#include "heap.h"
#include "epoll.h"
#include "common.h"
#include "sock.h"

#define EVENTLISTSIZE	1024
static int	epfd;
static struct epoll_event *event_list;

int init_epoll() 
{
	epfd = epoll_create( MAXCONNSLOTS );
	if ( epfd < 0 )
		return -1;

	//allocate evetn_list;
	//hp: change future
	event_list = (struct epoll_event*)safemalloc( sizeof(struct epoll_event)*EVENTLISTSIZE );
	if ( NULL == event_list ) {
		log_message( LOG_ERROR, "malloc event_list error." );
		return -1;
 	}

	return epfd;
}

int epoll_add_connection( conn_t *pconn, uint32_t flag )
{
	struct epoll_event ev;

	/*
   	 * my experience: just set one!
	 */
	//ev.data.fd = pconn->fd;
	ev.data.ptr = pconn;
	ev.events = flag | EPOLLERR | EPOLLHUP;

	if ( epoll_ctl( epfd, EPOLL_CTL_ADD, pconn->fd, &ev ) < 0 ) {
		//fprintf( stderr, "epoll_add error.\n" );
		log_message( LOG_ERROR, "epoll_ctl [fd:%d] error.", pconn->fd );
		return -1;
	}

	return TRUE;
}

int epoll_mod_connection( conn_t *pconn, uint32_t flag )
{
	struct epoll_event ev;

	/*
   	 * my experience: just set one!
	 */
	ev.data.ptr = pconn;
	//ev.data.fd = pconn->fd;
	ev.events = flag | EPOLLERR | EPOLLHUP;
	
	if ( epoll_ctl( epfd, EPOLL_CTL_MOD, pconn->fd, &ev ) < 0 ) {
		//fprintf( stderr, "epoll_mod_connection error.\n");
		log_message( LOG_ERROR, "epoll_ctl mod [fd:%d] error.", pconn->fd );
		return -1;
	}

	return TRUE;
}

int epoll_del_connection( conn_t *pconn )
{
	struct epoll_event ev;
	ev.events = 0;
	ev.data.ptr = NULL;
	
	if ( epoll_ctl( epfd, EPOLL_CTL_DEL, pconn->fd, &ev ) < 0 ) {
		//fprintf( stderr, "epoll_del_connection error.\n");
		log_message( LOG_ERROR, "epoll_ctl del [fd:%d] error.", pconn->fd );
		return -1;
	}
	return TRUE;
} 

int epoll_process_event()
{
	int 	events;
	int		i;
	int 	ret;
	conn_t 	*pconn;

	events = epoll_wait( epfd, event_list, EVENTLISTSIZE, -1 );
	if ( events < 0 ) {
		//fprintf( stderr, "epoll_wait error.\n" );
		log_message( LOG_ERROR, "epoll_wait error." );
		return -1;
	}
	if ( 0 == events ) {
		//fprintf( stderr, "epoll_wait timeout.\n" );
		log_message( LOG_WARNING, "epoll_wait timeout." );
		return 0;
	}


	for ( i = 0; i < events; ++ i ) {
		
		pconn = (conn_t*)event_list[i].data.ptr;

		if ( event_list[i].events & EPOLLIN  ) {			//read event
	
			if ( NULL == pconn->read_handle )
				continue;
			ret = pconn->read_handle( pconn );

			if ( ret < 0 ) {
				epoll_del_connection( pconn );
				release_conns_slot( pconn );
				//fprintf( stderr, "release conn fd:%d.\n", pconn->fd );
				log_message( LOG_NOTICE, "release connection fd:%d.", pconn->fd );
			}
		}

		else if ( event_list[i].events & EPOLLOUT ) {		//write event
		
			if ( NULL == pconn->write_handle )
				continue;
			ret = pconn->write_handle( pconn );

			if ( ret < 0 ) {
				epoll_del_connection( pconn );
				release_conns_slot( pconn );
				//fprintf( stderr, "release conn fd:%d.\n", pconn->fd );
				log_message( LOG_WARNING, "release connection fd:%d.", pconn->fd );
			}
		}

		else if ( event_list[i].events & EPOLLHUP || event_list[i].events & EPOLLERR ) {
			//fprintf( stderr, "EPOLLHUP, EPOLLERR.\n" );
			log_message( LOG_WARNING, "epoll receive EPOLLHUP or EPOLLERR." );
		}
		else {
			//fprintf( stderr, "other epoll events.\n" );	
			log_message( LOG_WARNING, "epoll receive unknown epoll events." );
		}
	}

	return TRUE;
}