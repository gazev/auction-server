#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "../utils/config.h"
#include "../utils/logging.h"

#include "tasks_queue.h"

struct tasks_queue {
    task_t tasks[THREAD_POOL_SZ * 2]; // this might be good enough since the workload is not too heavy 
    size_t size;
    int head;
    int tail;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
};

// initialize queue
int init_queue(tasks_queue **q) {
    if ((*q = malloc(sizeof(tasks_queue))) == NULL) {
        LOG_DEBUG("malloc: %s", strerror(errno));
        return -1;
    };

    if (pthread_mutex_init(&(*q)->mutex, NULL) != 0) {
        LOG_DEBUG("pthread_mutex_init: %s", strerror(errno));
        return -1;
    }

    if (pthread_cond_init(&(*q)->not_full, NULL) != 0) {
        LOG_DEBUG("pthread_cond_init: %s", strerror(errno));
        return -1;
    }

    if (pthread_cond_init(&(*q)->not_empty, NULL) != 0) {
        LOG_DEBUG("pthread_cond_init: %s", strerror(errno));
        return -1;
    }

    (*q)->size = 0;
    (*q)->head = 0;
    (*q)->tail = 0;

    return 0;
}

int destroy_queue(tasks_queue *q) {
    if (pthread_mutex_destroy(&q->mutex) != 0) {
        LOG_DEBUG("pthread_cond_init: %s", strerror(errno));
        return -1;
    }

    if (pthread_cond_destroy(&q->not_full) != 0) {
        LOG_DEBUG("pthread_cond_destroy: %s", strerror(errno));
        return -1;
    }

    if (pthread_cond_destroy(&q->not_empty) != 0) {
        LOG_DEBUG("pthread_cond_destroy: %s", strerror(errno));
        return -1;
    }
 
    free(q);

    return 0;
}

/**
* Enqueue an item into the queue. If an error occurs -1 is returned else, 0 is
* returned.
*/
int enqueue(tasks_queue *q, task_t *task) {
    if (pthread_mutex_lock(&q->mutex) != 0) {
        LOG_DEBUG("pthread_mutex_lock: %s", strerror(errno));
        return -1;
    }

    while (q->size == THREAD_POOL_SZ) {
        LOG_DEBUG("Producer waiting");
        if (pthread_cond_wait(&q->not_full, &q->mutex) != 0) {
            LOG_DEBUG("pthread_cond_wait: %s", strerror(errno));
            return -1;
        }
    }

    memcpy(&q->tasks[q->head], task, sizeof(task_t));
    q->head = (q->head + 1) % THREAD_POOL_SZ;
    q->size++;

    if (pthread_mutex_unlock(&q->mutex) != 0) {
        LOG_DEBUG("pthread_mutex_unlock: %s", strerror(errno));
        return -1;
    }

    if (pthread_cond_signal(&q->not_empty) != 0) {
        LOG_DEBUG("pthread_cond_signal: %s", strerror(errno));
        return -1;
    }

    return 0;
}

/**
* Retrieves an item from the queue. Returns 0 on success and -1 if an error occurs 
*/
int dequeue(tasks_queue *q, task_t *task) {
    if (pthread_mutex_lock(&q->mutex) != 0) {
        LOG_DEBUG("pthread_mutex_lock: %s", strerror(errno));
        return -1;
    }

    while (q->size == 0) {
        if (pthread_cond_wait(&q->not_empty, &q->mutex) != 0) {
            LOG_DEBUG("pthread_cond_wait: %s", strerror(errno));
            return -1;
        }
    }

    memcpy(task, &q->tasks[q->tail], sizeof (task_t));
    q->tail = (q->tail + 1) % THREAD_POOL_SZ;
    q->size--;

    if (pthread_mutex_unlock(&q->mutex) != 0) {
        LOG_DEBUG("pthread_mutex_unlock: %s", strerror(errno));
        return -1;
    }

    if (pthread_cond_signal(&q->not_full) != 0) {
        LOG_DEBUG("pthread_cond_signal: %s", strerror(errno));
        return -1;
    };

    return 0;
}
