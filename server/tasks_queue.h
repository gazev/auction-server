#ifndef __TASKS_QUEUE_H__
#define __TASKS_QUEUE_H__

#include <pthread.h>

#include "tcp.h"

typedef struct tcp_client task_t;
typedef struct tasks_queue tasks_queue;

int init_queue(tasks_queue **q);
int enqueue(tasks_queue *q, task_t *task);
int dequeue(tasks_queue *q, task_t *task);

#endif