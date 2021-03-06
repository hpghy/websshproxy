
#include "works.h"
#include "utils.h"
#include "sock.h"
#include "heap.h"
#include "epoll.h"

struct wprocess_s {
	pid_t	pid;
	enum { T_EMPTY, T_WAITING, T_WORKING } status;
};
typedef struct wprocess_s wprocess_t;

/*
 * each entry stands for a work process
 */
static wprocess_t 	*wprocess_ptr;
static unsigned int wprocesses;

/*
 * current work process index in wprocess_ptr array
 */
//static unsigned int work_idx;

static void worker_signal( int signo )
{
	switch ( signo ) {
	case SIGHUP:
		log_message( LOG_DEBUG, "worker:%d receive SIGHUP.", getpid() );
		break;

	case SIGTERM:
		log_message( LOG_DEBUG, "worker:%d receive SIGTERM.", getpid() );
		break;

	case SIGINT:
		log_message( LOG_DEBUG, "worker:%d receive SIGHUP.", getpid() );
		break;

	case SIGPIPE:
		log_message( LOG_DEBUG, "worker:%d receive SIGPIPE.", getpid() );
		break;

	default:
		log_message( LOG_DEBUG, "worker:%d receive default signal:%d.", getpid(), signo );
		return;
	}

	log_message( LOG_DEBUG, "worker:%d exit after signal_handle", getpid() );
	exit(0);			//退出，但是没有清理内存
}

/*
 * work process main loop
 *
 */
static void work_main( unsigned int index )		//index: no use
{
	//work_idx = index;		//
	int	epfd;
	int ret;

	//清理父进程分配的动态空间
	//子进程继承了父进程的wprocess_ptr
	safefree( wprocess_ptr );

	//init epoll
	if ( ( epfd = init_epoll() ) < 0 ) {
		log_message( LOG_ERROR, "epoll_create error." );
		return;
	}

	//add fd into epoll
	if ( init_conns_array( epfd ) < 0 ) {
		log_message( LOG_ERROR, "init_conns_array() error." );
		return;
	}
	
	while ( TRUE ) {
		ret = epoll_process_event();
		if ( ret < 0 ) {
			log_message( LOG_ERROR, "epoll_process_event error." );
		}
	}

	//清理所有连接的read_buffer/write_buffer

	log_message( LOG_ERROR, "work:%d going to dead.\n", getpid() );
}


/*
 *
 */
void work_terminated( pid_t pid )
{
	int i;
	for ( i = 0; i < wprocesses; ++ i ) {
		if ( wprocess_ptr[i].pid == pid ) {
			wprocess_ptr[i].status = T_EMPTY;
			break;
		}
	}
}


/*
 * invokes by master process
 */
static pid_t make_process( unsigned int index )
{
	pid_t 	pid;

	if ( (pid=fork()) > 0 )
		return pid;		//master process

	//work process...
	/*
	set_signal_handle( SIGCHLD, SIG_DFL );
	set_signal_handle( SIGTERM, SIG_DFL );
	set_signal_handle( SIGINT, SIG_DFL );
	set_signal_handle( SIGHUP, SIG_DFL );
	*/
	set_signal_handle( SIGCHLD, worker_signal );
	set_signal_handle( SIGTERM, worker_signal );
	set_signal_handle( SIGINT, worker_signal );
	set_signal_handle( SIGHUP, worker_signal );
	set_signal_handle( SIGPIPE, worker_signal );
	
	work_main( index );	//never return.
	return -1;
}

/*
 *
 */
void monitor_workers()
{
	int i;
	for ( i = 0; i < wprocesses; ++ i ) {
		if ( wprocess_ptr[i].status == T_EMPTY ) {
			wprocess_ptr[i].pid = make_process(i);
			
			if ( wprocess_ptr[i].pid < 0 ) {
				log_message( LOG_ERROR, "create process %d error.", i );
				return;
			}
			log_message( LOG_DEBUG, "create worker:%d.\n", wprocess_ptr[i].pid );
			wprocess_ptr[i].status = T_WAITING;
		}
	}
}

/*
 *  invoke by master process
 */
int create_works_processes( unsigned int works )
{
   	/*
   	 * each work process has a copy of wprocess_ptr
   	 */
	wprocess_ptr = (wprocess_t*)safemalloc( sizeof(wprocess_t)*works );
	wprocesses = works;
	if ( NULL == wprocess_ptr ) {
		//fprintf( stderr, "Could not malloc memory for works processes.\n" );
		log_message( LOG_WARNING, "Could not malloc memory for works processes.\n" );
		return -1;
	} 
	int i;
	for ( i = 0; i < works; ++ i ) {
		wprocess_ptr[i].status = T_WAITING;
		wprocess_ptr[i].pid = make_process( i );
		if ( wprocess_ptr[i].pid < 0 ) {
			log_message( LOG_ERROR, "create process %d error.\n", i );
			return -1;
		}
		log_message( LOG_DEBUG, "create worker:%d.\n", wprocess_ptr[i].pid );
	}

	return TRUE;
}

/**
 * invoked by master process
 */
void kill_works_processes()
{
	int i;
	for ( i = 0; i < wprocesses; ++ i ) {
		if ( wprocess_ptr[i].status != T_EMPTY ) 
			kill( wprocess_ptr[i].pid, SIGTERM );
	}
	wprocesses = 0;
	safefree( wprocess_ptr );		//free 
}
