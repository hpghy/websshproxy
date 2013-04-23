/*
 * epoll.h
 */

#ifndef HP_EPOLL_H
#define HP_EPOLL_H

#include "sock.h"

#define EPOLLMAX	1024*2

extern int init_epoll();
extern int epoll_add_connection( conn_t *pconn, uint32_t );
extern int epoll_del_connection( conn_t *pconn );
extern int epoll_mod_connection( conn_t *pconn, uint32_t );
extern int epoll_process_event();

#endif
