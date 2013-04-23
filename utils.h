/*
 * utils.h
 *
 */

#ifndef HP_UTILS_H
#define HP_UTILS_H

#define IPMAXCNT	10

#define LOG_ERROR		1
#define LOG_WARNING		2
#define LOG_NOTICE		3	
#define LOG_DEBUG		4	
	
struct config_s {
	char 	*configfile;
	char	*logfile;
	char	*pidfile;
	
	unsigned int	maxclients;
	unsigned int	works;
	unsigned int	loglevel;
	unsigned int	bindcnt;
	char	*ips[IPMAXCNT];
};	
typedef struct config_s config_t;

typedef void (*SIGHANDLETYPE)(int);

extern int read_config_file( config_t * );

extern int open_log_file( const char * );
extern void print_store_logs();
extern void close_log_file();
extern int log_file_large();
extern void truncate_log_file();
extern void set_log_level(int level); 
extern void log_message( int level, char *fmt, ... );

extern SIGHANDLETYPE set_signal_handle( int signo, SIGHANDLETYPE func ); 

extern int pidfile_create(const char *filename);

extern void makedaemon();

#endif
