#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>

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

    while (queue->head != NULL) {
        LLNode* next = atomic_load(&queue->head->next);
        free(queue->head);
        queue->head = next;
    }

    HazardPointer_finalize(&queue->hp);
    free(queue);
}

void LLQueue_push(LLQueue* queue, Value item) {
    LLNode* new_node = LLNode_new(item);
    while (true) {
        LLNode* tail = atomic_load(&queue->tail);
        LLNode* expected_null = NULL;

        // Za każdym razem oczekujemy, że tail->next to będzie null.
        // Jeśli okaże się, że jest inaczej, to znaczy, że ktoś już dodał nowy element do kolejki.
        if (atomic_compare_exchange_strong(&tail->next, &expected_null, new_node)) {

            // W tym miejscu compare_exchange ustawił tail->next na nowy węzeł.
            atomic_compare_exchange_strong(&queue->tail, &tail, new_node); // TODO
            return;
        }
    }
}


Value LLQueue_pop(LLQueue* queue) {

    while (true) {
        LLNode* head = atomic_load(&queue->head);
        LLNode* next = atomic_load(&head->next);

        if (next == NULL) {
            return EMPTY_VALUE;
        } else {
            Value value = next->value;
            // Jak w międzyczasie głowa się zmieniła, to będziemy musieli zacząć ponownie.
            if (atomic_compare_exchange_strong(&queue->head, &head, next)) {
                HazardPointer_retire(&queue->hp, head);
                return value;
            }
        }

    }
}


// Jeśli w kolejce jest tylko strażnik, to jest pusta
bool LLQueue_is_empty(LLQueue* queue) {

    return (atomic_load(&atomic_load(&queue->head)->next) == NULL);
}
