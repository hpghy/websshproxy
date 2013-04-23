/*
 * common.h
 * 
 */

#ifndef HP_COMMON_H
#define HP_COMMON_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>

#define TRUE 1
#define FALSE 0

#define MAX(a,b)	(a)>(b)?(a):(b)
#define MIN(a,b)	(a)>(b)?(b):(a)

#endif
