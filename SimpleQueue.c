#include <malloc.h>
#include <pthread.h>

#include "HazardPointer.h"
#include "SimpleQueue.h"

struct SimpleQueueNode;
typedef struct SimpleQueueNode SimpleQueueNode;

struct SimpleQueueNode {
    SimpleQueueNode* next;
    Value item;
};

SimpleQueueNode* SimpleQueueNode_new(Value item)
{
    SimpleQueueNode* node = (SimpleQueueNode*)malloc(sizeof(SimpleQueueNode));
    node->next = NULL;
    node->item = item;
    // TODO
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
    // TODO
    SimpleQueueNode* foo_node = SimpleQueueNode_new(EMPTY_VALUE);
    queue->head = foo_node;
    queue->tail = foo_node;
    pthread_mutex_init(&queue->head_mtx, NULL);
    pthread_mutex_init(&queue->tail_mtx, NULL);

    return queue;
}

void SimpleQueue_delete(SimpleQueue* queue)
{
    // TODO
    free(queue->head); // foo_node
    pthread_mutex_destroy(&queue->head_mtx);
    pthread_mutex_destroy(&queue->tail_mtx);

    free(queue);
}

void SimpleQueue_push(SimpleQueue* queue, Value item)
{
    // TODO
    SimpleQueueNode* new_node = SimpleQueueNode_new(item);
    pthread_mutex_lock(&queue->tail_mtx);
    queue->tail->next = new_node;
    queue->tail = new_node;
    pthread_mutex_unlock(&queue->tail_mtx);
}

Value SimpleQueue_pop(SimpleQueue* queue)
{
    // TODO
    pthread_mutex_lock(&queue->head_mtx);
    SimpleQueueNode* oldHead = queue->head;
    SimpleQueueNode* newHead = oldHead->next;

    if (newHead == NULL) {
        pthread_mutex_unlock(&queue->head_mtx);
        return EMPTY_VALUE; // Queue is empty
    }

    Value value = newHead->item;
    queue->head = newHead;

    pthread_mutex_unlock(&queue->head_mtx);
    free(oldHead);

    return value;
}

bool SimpleQueue_is_empty(SimpleQueue* queue)
{
    // TODO
    pthread_mutex_lock(&queue->head_mtx);
    bool isEmpty = (queue->head->next == NULL);
    pthread_mutex_unlock(&queue->head_mtx);

    return isEmpty;
}
