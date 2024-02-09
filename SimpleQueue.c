#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>

#include "SimpleQueue.h"

struct SimpleQueueNode;
typedef struct SimpleQueueNode SimpleQueueNode;

struct SimpleQueueNode {
    _Atomic(SimpleQueueNode*) next;
    Value item;
};

SimpleQueueNode* SimpleQueueNode_new(Value item)
{
    SimpleQueueNode* node = (SimpleQueueNode*)malloc(sizeof(SimpleQueueNode));
    atomic_init(&node->next, NULL);
    node->item = item;
    return node;
}

struct SimpleQueue {
    SimpleQueueNode* head;
    SimpleQueueNode* tail;
    pthread_mutex_t head_mtx;
    pthread_mutex_t tail_mtx;
};

SimpleQueue* SimpleQueue_new(void)
{
    SimpleQueue* queue = (SimpleQueue*)malloc(sizeof(SimpleQueue));
    pthread_mutex_init(&queue->head_mtx, NULL);
    pthread_mutex_init(&queue->tail_mtx, NULL);
    SimpleQueueNode* guard_node = SimpleQueueNode_new(EMPTY_VALUE); // Strażnik
    queue->head = guard_node;
    queue->tail = guard_node;

    return queue;
}

void SimpleQueue_delete(SimpleQueue* queue)
{
    pthread_mutex_destroy(&queue->head_mtx);
    pthread_mutex_destroy(&queue->tail_mtx);

    while (queue->head != NULL) {
        SimpleQueueNode* next = atomic_load(&queue->head->next);
        free(queue->head);
        queue->head = next;
    }

    free(queue);
}

void SimpleQueue_push(SimpleQueue* queue, Value item)
{
    SimpleQueueNode* new_node = SimpleQueueNode_new(item);
    pthread_mutex_lock(&queue->tail_mtx);
    atomic_store(&atomic_load(&queue->tail)->next, new_node);
    queue->tail = new_node;
    pthread_mutex_unlock(&queue->tail_mtx);
}

Value SimpleQueue_pop(SimpleQueue* queue)
{
    pthread_mutex_lock(&queue->head_mtx);
    // Czy kolejka jest pusta
    if (atomic_load(&queue->head->next) == NULL) {
        pthread_mutex_unlock(&queue->head_mtx);
        return EMPTY_VALUE;
    }

    SimpleQueueNode* guard_node = queue->head;
    SimpleQueueNode* to_remove = atomic_load(&guard_node->next);

    // Węzeł do usunięcia zastępujemy przez guard_node
    Value value = to_remove->item;
    to_remove->item = EMPTY_VALUE;
    queue->head = to_remove;

    pthread_mutex_unlock(&queue->head_mtx);
    free(guard_node);

    return value;
}

bool SimpleQueue_is_empty(SimpleQueue* queue)
{
    pthread_mutex_lock(&queue->head_mtx);
    // Jeśli w kolejce jest tylko strażnik, to jest pusta
    bool result = (atomic_load(&queue->head->next) == NULL ? true : false);
    pthread_mutex_unlock(&queue->head_mtx);

    return result;
}
