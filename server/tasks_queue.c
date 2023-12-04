#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "tasks_queue.h"

struct tasks_queue {
    task_t tasks[100];
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
        perror("malloc");
        return -1;
    };

    if (pthread_mutex_init(&(*q)->mutex, NULL) != 0) {
        perror("pthread_mutex_init");
        return -1;
    }

    if (pthread_cond_init(&(*q)->not_full, NULL) != 0) {
        perror("pthread_cond_init");
        return -1;
    }

    if (pthread_cond_init(&(*q)->not_empty, NULL) != 0) {
        perror("pthread_cond_init");
        return -1;
    }

    (*q)->size = 0;
    (*q)->head = 0;
    (*q)->tail = 0;

    return 0;
}

int destroy_queue(tasks_queue *q) {
    if (pthread_mutex_destroy(&q->mutex) != 0) {
        perror("pthread_mutex_destroy");
        return -1;
    }

    if (pthread_cond_destroy(&q->not_full) != 0) {
        perror("pthread_cond_destroy");
        return -1;
    }

    if (pthread_cond_destroy(&q->not_empty) != 0) {
        perror("pthread_cond_destroy");
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
        perror("pthread_mutex_lock");
        return -1;
    }

    while (q->size == 100) {
        printf("PRODUCER WAITING");
        if (pthread_cond_wait(&q->not_full, &q->mutex) != 0) {
            perror("pthread_cond_wait");
            return -1;
        }
    }

    memcpy(&q->tasks[q->head], task, sizeof(task_t));
    q->head = (q->head + 1) % 100;
    q->size++;

    if (pthread_mutex_unlock(&q->mutex) != 0) {
        perror("pthread_mutex_unlock");
        return -1;
    }

    if (pthread_cond_signal(&q->not_empty) != 0) {
        perror("pthread_cond_signal");
        return -1;
    }

    return 0;
}

/**
* Retrieves an item from the queue. Returns 0 on success and -1 if an error occurs 
*/
int dequeue(tasks_queue *q, task_t *arg) {
    if (pthread_mutex_lock(&q->mutex) != 0) {
        perror("pthread_mutex_lock");
        return -1;
    }

    while (q->size == 0) {
        if (pthread_cond_wait(&q->not_empty, &q->mutex) != 0) {
            perror("pthread_cond_wait");
            return -1;
        }
    }

    memcpy(arg, &q->tasks[q->tail], sizeof (task_t));
    q->tail = (q->tail + 1) % 100;
    q->size--;

    if (pthread_mutex_unlock(&q->mutex) != 0) {
        perror("pthread_mutex_unlock");
        return -1;
    }

    if (pthread_cond_signal(&q->not_full) != 0) {
        perror("pthread_cond_signal");
        return -1;
    };

    return 0;
}
