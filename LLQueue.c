#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <assert.h>

#include "HazardPointer.h"
#include "LLQueue.h"

typedef struct LLNode LLNode;
typedef _Atomic(LLNode*) AtomicLLNodePtr;

struct LLNode {
    AtomicLLNodePtr next;
    atomic_int value;
};


LLNode* LLNode_new(Value item) {
    LLNode* node = (LLNode*)malloc(sizeof(LLNode));
    atomic_store(&node->next, NULL);
    node->value = item;
    return node;
}

struct LLQueue {
    AtomicLLNodePtr head;
    AtomicLLNodePtr tail;
    HazardPointer hp;
};


LLQueue* LLQueue_new(void) {
    LLQueue* queue = (LLQueue*)malloc(sizeof(LLQueue));
    LLNode* guard_node = LLNode_new(EMPTY_VALUE);
    atomic_store(&queue->head, guard_node);
    atomic_store(&queue->tail, guard_node);
    HazardPointer_initialize(&queue->hp);
    return queue;
}

void LLQueue_delete(LLQueue* queue) {

//    while (queue->head != NULL) {
//        LLNode* next = atomic_load(&queue->head->next);
//        free(queue->head); // Tutaj free, bo już nie ma wielowątkowości
//        queue->head = next;
//    }
//
//    HazardPointer_finalize(&queue->hp);
//    free(queue);
}

void LLQueue_push(LLQueue* queue, Value item) {

    LLNode* head = LLNode_new(item);
    while (true) {
        LLNode* tail = atomic_load(&queue->tail);
        LLNode* expected_null = NULL;

        // Spodziewamy się, że następny node to null
        if (atomic_compare_exchange_strong(&tail->next, &expected_null, head) == true) {
            atomic_compare_exchange_strong(&queue->tail, &tail, head);
            return;
        } else {
            atomic_compare_exchange_strong(&queue->tail, &tail, tail->next);
        }

    }
}


Value LLQueue_pop(LLQueue* queue) {

    LLNode *head = atomic_load(&queue->head);
    while (true) {

        head = atomic_load(&queue->head);
        if (head->next == NULL) {
            return EMPTY_VALUE;
        }

        if (atomic_compare_exchange_strong(&queue->head, &head, head->next) == true) {
            return atomic_load(&head->next->value);
        }
    }
}


// Jeśli w kolejce jest tylko strażnik, to jest pusta
bool LLQueue_is_empty(LLQueue* queue) {

    return atomic_load(&atomic_load(&queue->head)->next) == NULL;
}
