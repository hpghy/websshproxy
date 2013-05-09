#include "utils.h"
#include "common.h"
#include "heap.h"
#include "buffer.h"

static char *syslog_level[] = {
	NULL,
	"LOG_ERROR",
	"LOG_WARNING",
	"LOG_NOTICE",
	"LOG_DEBUG"
};

#define TIME_LENGTH 16
#define STRING_LENGTH 800

/*
 * Global file descriptor for the log file
 */
int log_file_fd = -1;
/*
 * Store the log level setting.
 */
static int log_level = LOG_ERROR;

/*
 * 
 */
static unsigned int created_log_file = FALSE;
static char 	storelogs[STRING_LENGTH];

static int iscomment( char *buff ) {
	int i;
	for ( i = 0; i < strlen(buff) && ' ' == buff[i]; ++ i );
	if ( '#' == buff[i] )
		return TRUE;
	return -1;
}

static void trimfilepath( char * filepath )
{
	if ( '\"' == filepath[0] && '\"' == filepath[strlen(filepath)-1] ) {
		int i;
		for ( i = 0; i < strlen(filepath)-2; ++ i )
			filepath[i] = filepath[i+1];
		filepath[i] = 0;
	}
}

static int create_file_safely( const char *filename, int btruncated )
{
	struct stat statinfo;
	int	fd;

	if ( lstat( filename, &statinfo) < 0) {
		if (errno != ENOENT) {
			//fprintf(stderr,	"Error checking file %s: %s\n", filename, strerror(errno));
			return -EACCES;
		}
		fd = open( filename, O_RDWR | O_CREAT | O_EXCL, 0600);
		if ( fd < 0) {
			//fprintf(stderr, "hp Could not create file %s: %s\n", filename, strerror(errno));
		}
		return fd;
	} 

	int flags = O_RDWR;
	if (!btruncated)
		flags |= O_APPEND;

	if ((fd = open(filename, flags)) < 0) {
		//fprintf(stderr, "Could not open file %s: %s\n", filename, strerror(errno));
	}

	return fd;
}

int open_log_file(const char* log_file_name)
{
	log_file_fd = create_file_safely(log_file_name, TRUE);
	created_log_file = TRUE;
	return log_file_fd;
}

void print_store_logs()
{
	assert(log_file_fd >= 0);
	write( log_file_fd, storelogs, strlen(storelogs) );	
}

void close_log_file(void)
{
	//fsync( log_file_fd );
	close(log_file_fd);
}

int log_file_large() 
{
	struct stat statinfo;
	if ( fstat( log_file_fd, &statinfo ) < 0 ) {
		//fprintf( stderr, "fstat( log_file_fd ) error.\n" );
		return -1;
	}
	if ( statinfo.st_size / 1024 / 1024 > 10 )
		return TRUE;
	return FALSE;
}

void truncate_log_file(void)
{
	lseek(log_file_fd, 0, SEEK_SET);
	ftruncate(log_file_fd, 0);
}

void set_log_level(int level)
{
	log_level = level;
	//fprintf( stderr, "loglevel:%s.\n", syslog_level[level] );
}

void log_message(int level, char *fmt, ...) {

    va_list args;
    time_t nowtime;
    char time_string[TIME_LENGTH];
    char str[STRING_LENGTH];

    if ( level < log_level || log_file_fd > -1 )
        return;

    va_start(args, fmt);

    if ( log_file_fd < 0 ) {
        vsnprintf(str, STRING_LENGTH, fmt, args);
        va_end(args);
        printf( "%s\n", str );
        return;
    }

    nowtime = time(NULL);
    memset( time_string, 0, sizeof(time_string) );
    strftime(time_string, TIME_LENGTH, "%b %d %H:%M:%S",
         localtime(&nowtime));
	 memset( str, 0, sizeof(str) );
    snprintf(str, STRING_LENGTH, "%-9s %s [%ld]: ", syslog_level[level],
         time_string, (long int) getpid());

    assert(log_file_fd >= 0);

    vsnprintf(str+strlen(str), STRING_LENGTH, fmt, args);
    strcat( str, "\n" );
    write(log_file_fd, str, strlen(str) );

    va_end(args);
}

int read_config_file( config_t * pconfig )
{
	FILE *fcin;
	char buff[150], key[50], value[100];
	unsigned int size, i;
	
	fcin = fopen( pconfig->configfile, "r" );
	if ( NULL == fcin ) {
		fprintf( stderr, "fopen configfile:%s error:%s.\n", pconfig->configfile, strerror(errno) );
		return -1;
	}
	
	while ( fscanf( fcin, " %[^\n]", buff) != EOF ) {
		//fprintf( stderr, "%s\n", buff );
		if ( TRUE == iscomment(buff) ) {
			continue;
		}
		sscanf( buff, "%s%s", key, value );
		if ( strcasecmp( key, "logfile" ) == 0 ) {
			trimfilepath( value );
			pconfig->logfile = safestrdup( value );	
		}
		
		else if ( strcasecmp( key, "pidfile" ) == 0 ) {
			trimfilepath( value );
			pconfig->pidfile = safestrdup( value );
		}

		else if ( strcasecmp( key, "loglevel" ) == 0 ) {
			for ( i = LOG_ERROR; i <= LOG_NOTICE; ++ i ) {
				if ( 0 == strcmp( value, syslog_level[i] ) ) {
					pconfig->loglevel = i;
				}
			}
		}

		else if ( strcasecmp( key, "maxclients" ) == 0 ) {
			size = atoi(value);
			pconfig->maxclients = size;
		}

		else if ( strcasecmp( key, "works" ) == 0 ) {
			size = atoi(value);
			pconfig->works = size;
		}

		else if ( strcasecmp( key, "bind" ) == 0 ) {
			pconfig->ips[pconfig->bindcnt++] = safestrdup( value );
		}
		
		else {
			fprintf( stderr, "unkown configuration string:%s.\n", key );
			log_message( LOG_WARNING, "unkown configuration string:%s", key );
			return -1;
		}
	}
	
	return TRUE;
}

/*
 * Pass a signal number and a signal handling function into this function
 * to handle signals sent to the process.
 */
SIGHANDLETYPE set_signal_handle( int signo, SIGHANDLETYPE func )
{
	struct sigaction act, oact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (signo == SIGALRM) {
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;	/* SunOS 4.x */
#endif
	} else {
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;	/* SVR4, 4.4BSD */
#endif
	}

	if (sigaction(signo, &act, &oact) < 0)
		return SIG_ERR;

	return oact.sa_handler;
}

/*
 * Write the PID of the program to the specified file.
 */
int pidfile_create(const char *filename)
{
	int fildes;
	FILE *fd;

	/*
	 * Create a new file
	 */
	if ((fildes = create_file_safely(filename, TRUE)) < 0)
		return fildes;

	/*
	 * Open a stdio file over the low-level one.
	 */
	if ((fd = fdopen(fildes, "w")) == NULL) {
		fprintf(stderr,	"Could not write PID file %s: %s.",filename, strerror(errno));
		close(fildes);
		unlink(filename);
		return -EIO;
	}

	fprintf(fd, "%ld\n", (long) getpid());
	fclose(fd);
	return 0;
}

/*
 * Fork a child process and then kill the parent so make the calling
 * program a daemon process.
 */
void makedaemon(void)
{
	if (fork() != 0)
		exit(0);

	setsid();
	set_signal_handle(SIGHUP, SIG_IGN);

	if (fork() != 0)
		exit(0);

	chdir("/");
	umask(077);

	/*
	struct rlimit	rl;
	if ( getrlimit( RLIMIT_NOFILE, &rl ) < 0 ) {
		fprintf( stderr, "getrlimit error.\n" );
		return;
	}
	
	if ( rl.rlim_max == RLIM_INFINITY )
		rl.rlim_max = 1024;
	int i;
	for ( i = 0; i < rl.rlim_max; ++ i )
		close(i);
	*/
	
	close(0);
	close(1);
	close(2);

	int fd0, fd1, fd2;

	fd0 = open( "/dev/null", O_RDWR );
	fd1 = dup(0);
	fd2 = dup(0);
	if ( fd0 != 0 || fd1 != 1 || fd2 != 2 ) {
		fprintf( stderr, "unexpected file descriptors in makedaemon.\n" );
	}

/*
#if NDEBUG
	close(0);
	close(1);
	close(2);
#endif
*/

}

