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
    SimpleQueueNode* foo_node = SimpleQueueNode_new(EMPTY_VALUE); // Strażnik
    queue->head = foo_node;
    queue->tail = foo_node;

    return queue;
}

void SimpleQueue_delete(SimpleQueue* queue)
{
    pthread_mutex_destroy(&queue->head_mtx);
    pthread_mutex_destroy(&queue->tail_mtx);

    while (queue->head != NULL) {
        SimpleQueueNode* next = queue->head->next;
        free(queue->head);
        queue->head = next;
    }

    free(queue);
}

void SimpleQueue_push(SimpleQueue* queue, Value item)
{
    SimpleQueueNode* new_node = SimpleQueueNode_new(item);
    pthread_mutex_lock(&queue->tail_mtx);
    queue->tail->next = new_node;
    queue->tail = new_node;
    pthread_mutex_unlock(&queue->tail_mtx);
}

Value SimpleQueue_pop(SimpleQueue* queue)
{

    pthread_mutex_lock(&queue->head_mtx);
    // Czy kolejka jest pusta
    if (queue->head->next == NULL) {
        pthread_mutex_unlock(&queue->head_mtx);
        return EMPTY_VALUE;
    }

    SimpleQueueNode* foo_node = queue->head;
    SimpleQueueNode* to_remove = queue->head->next;

    // Węzeł do usunięcia zastępujemy przez foo_node
    Value value = to_remove->item;
    to_remove->item = EMPTY_VALUE;
    queue->head = to_remove;

    pthread_mutex_unlock(&queue->head_mtx);
    free(foo_node);

    return value;
}

bool SimpleQueue_is_empty(SimpleQueue* queue)
{
    pthread_mutex_lock(&queue->head_mtx);
    bool result = (queue->head->next == NULL ? true : false);
    pthread_mutex_unlock(&queue->head_mtx);

    return result;
}
