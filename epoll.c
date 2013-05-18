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
	ev.data.ptr = pconn;
	ev.events = flag | EPOLLET | EPOLLERR | EPOLLHUP;

	if ( epoll_ctl( epfd, EPOLL_CTL_ADD, pconn->fd, &ev ) < 0 ) {
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
	ev.events = flag | EPOLLET | EPOLLERR | EPOLLHUP;
	
	if ( epoll_ctl( epfd, EPOLL_CTL_MOD, pconn->fd, &ev ) < 0 ) {
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
		log_message( LOG_ERROR, "epoll_wait error." );
		return -1;
	}
	if ( 0 == events ) {
		log_message( LOG_WARNING, "epoll_wait timeout." );
		return 0;
	}


	for ( i = 0; i < events; ++ i ) {
		
		pconn = (conn_t*)event_list[i].data.ptr;

		if ( event_list[i].events & EPOLLIN  ) {			//read event
	
			if ( NULL == pconn->read_handle )
				continue;
			ret = pconn->read_handle( pconn );

			//hp modified 2013/05/14
			//如果accept_handler出错，不需要删除
			if ( ret < 0 && pconn->type != C_LISTEN ) {		// error
				log_message( LOG_DEBUG, "release connection fd:%d.", pconn->fd );
				epoll_del_connection( pconn );
				release_conns_slot( pconn );
			}
		}

		else if ( event_list[i].events & EPOLLOUT ) {		//write event
		
			if ( NULL == pconn->write_handle )
				continue;
			ret = pconn->write_handle( pconn );

			if ( ret < 0 && pconn->type != C_LISTEN ) {		//error
				log_message( LOG_DEBUG, "release connection fd:%d.", pconn->fd );
				epoll_del_connection( pconn );
				release_conns_slot( pconn );
			}
		}

		else if ( event_list[i].events & EPOLLHUP || event_list[i].events & EPOLLERR ) {
			log_message( LOG_WARNING, "epoll receive EPOLLHUP or EPOLLERR." );
		}
		else {
			log_message( LOG_WARNING, "epoll receive unknown epoll events." );
		}
	}
	return TRUE;
}
