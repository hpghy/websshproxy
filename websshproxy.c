/*
 * websshproxy.c
 *
 */

#include "common.h"
#include "sock.h"
#include "works.h"
#include "utils.h"
#include "heap.h"

#define  DEFAULT_CONFIG_FILE 	"/root/hp/etc/websshproxy.conf"
#define  VERSION	"2.0-beta hp 2013.05.15"

unsigned int	receive_sighup = FALSE;		//clear log file
unsigned int	receive_sigterm = FALSE;	//termination
unsigned int	receive_sigint = FALSE;		//termination

config_t config;

static void sig_handle( int signal )
{
	pid_t pid;
	int status;

	switch ( signal ) {
	case SIGHUP:
		receive_sighup = TRUE;
		break;

	case SIGTERM:
		receive_sigterm = TRUE;
		break;

	case SIGINT:
		receive_sigint = TRUE;
		break;

	case SIGCHLD:		//子进程已经消亡,问题在于，我不知道为什么会消亡 
		pid = wait( &status );
		work_terminated( pid );
		log_message( LOG_DEBUG, "work:%d terminated.", pid );
		break;
	}
	return;
}

static void display_license()
{
	printf( "Copyright huangpeng 790042744@qq.com ccntgrid zju.\n" );
}
static void display_version()
{
	printf( "%s.\n", VERSION );
}
static void display_usage()
{
	printf("Usage: websshproxy [options]\n");
	printf("\
Options:\n\
  -d		Operate in DEBUG mode.\n\
  -c FILE	Use an alternate configuration file.\n\
  -h		Display this usage information.\n\
  -l            Display the license.\n\
  -v            Display the version number.\n");
}

static void main_loop()
{
	while ( TRUE ) {
		if ( receive_sigterm == TRUE || receive_sigint == TRUE )
			return;

		//manage works process.
		monitor_workers();

		if ( receive_sighup == TRUE || log_file_large() == TRUE ) {
			truncate_log_file();
			receive_sighup = FALSE;
			log_message( LOG_DEBUG, "truncate log file." );
		}
		sleep(20);
	}
	log_message( LOG_DEBUG, "master process received exit signal." );
}

int main( int argc, char **argv )
{
	//process option
	int optch;
	unsigned int	daemon = TRUE;

	memset( &config, 0, sizeof(config_t) );
	config.configfile = DEFAULT_CONFIG_FILE;	
	config.works = 1;
	config.loglevel = 2;
	 
	while ( ( optch = getopt( argc, argv, "c:vldh")) != EOF ) {
		switch (optch) {
		case 'v':
			display_version();
			return 0;
		case 'l':
			display_license();
			return 0;
		case 'd':
			daemon = FALSE;
			break;
		case 'c':
			config.configfile = safestrdup(optarg);
			if (!config.configfile) {
				fprintf( stderr, "Could not allocate memory.\n" );
				return 0;
			}
			break;
		case 'h':
		default:
			display_usage();
			return 0;
		}
	}

	/*
   	 * parse configuration file
	 */
	if ( read_config_file( &config ) < 0 ) {
		fprintf( stderr, "read config file error.\n" );
		return 0;
	}

	if ( TRUE == daemon ) {
		//create log file and initializing log system
		set_log_level( config.loglevel );
		if ( config.logfile ) {
			if ( open_log_file( config.logfile ) < 0 ) {
				fprintf( stderr, "Could not create log file.\n" );
				return 0;
			}
		}
		makedaemon();
	}
	else {
		//debug mode
		set_log_level( LOG_CONN );
		fprintf( stderr, "debug mode...\n" );
	}

	//create pidfile
	if ( config.pidfile ) {
		if ( pidfile_create( config.pidfile ) < 0 ) {
			log_message( LOG_ERROR, "create pidfile:%s failly.", config.pidfile );
			return 0;
		}
	}

	//sigpipe: send to disconnected socket.
	if ( set_signal_handle( SIGPIPE, SIG_IGN ) == SIG_ERR ) {
		log_message( LOG_ERROR , "Couldnot set SIGPIPE signal.");
		return 0;
	}

	//open listen socket.
	if ( open_listening_sockets( &config ) < 0 ) {
		log_message( LOG_ERROR, "open_listening_sockets error." );
		return 0;
	}
	log_message( LOG_DEBUG, "open_listening_sockets OK." );


	//create works processes
	if ( create_works_processes( config.works ) < 0 ) {
		log_message( LOG_ERROR, "Create works processes error." );
		return 0;
	}
	log_message( LOG_DEBUG, "create_works_processes OK." );


	//signal handle only for master process
	if ( set_signal_handle( SIGCHLD, sig_handle ) == SIG_ERR ) {
		return 0;
	}
	if ( set_signal_handle( SIGTERM, sig_handle ) == SIG_ERR ) {
		return 0;
	}
	if ( set_signal_handle( SIGINT, sig_handle ) == SIG_ERR ) {
		return 0;
	}
	if ( set_signal_handle( SIGHUP, sig_handle ) == SIG_ERR ) {
		return 0;
	}

	//main_loop
	main_loop();

	//kill children
	kill_works_processes();

	close_listen_sockets();

	//remove pid file
	if ( unlink( config.pidfile ) < 0 ) {
		log_message( LOG_DEBUG, "Could not remove pid file." );
	}
	
	//close log file
	log_message( LOG_DEBUG, "master process going to dead." );

	close_log_file();

	//clear memory

	return 0;
}
