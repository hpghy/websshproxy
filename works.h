/*
 * works.h
 *
 */

#ifndef HP_WORKS_H
#define HP_WORKS_H

#include "common.h"

//extern void work_terminated( pid_t pid );
extern int create_works_processes( unsigned int );
extern void kill_works_processes();
extern void monitor_workers();
extern void work_terminated( pid_t pid );

#endif
