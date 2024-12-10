#include <stdio.h>
#include <stdlib.h>
#include "kf_sys_base_node.h"

typedef struct Node {
    void* data;
    KFNodeObjectCallback refCallback, unrefCallback;
    struct Node *prev, *next;
} Node;

// ***************

KFNODE KFNodeCreate(void* data) {
    Node* node = (Node*)malloc(sizeof(Node));
    if (node) {
        node->data = data;
        node->refCallback = NULL;
        node->unrefCallback = NULL;
        node->prev = node->next = NULL;
    }
    return node;
}

KFNODE KFNodeCreateEx(void* data, KFNodeObjectCallback refcb, KFNodeObjectCallback unrefcb) {
    Node* node = (Node*)KFNodeCreate(data);
    if (node) {
        node->refCallback = refcb;
        node->unrefCallback = unrefcb;
        if (data) {
            if (node->refCallback)
                node->refCallback(data);
        }
    }
    return node;
}

void KFNodeSetCallback(KFNODE node, KFNodeObjectCallback refcb, KFNodeObjectCallback unrefcb) {
    if (node) {
        Node* n = (Node*)node;
        n->refCallback = refcb;
        n->unrefCallback = unrefcb;
        if (n->data)
            n->refCallback(n->data);
    }
}

void KFNodeSetData(KFNODE node, void* data) {
    if (node) {
        Node* n = (Node*)node;
        if (n->data) {
            if (n->unrefCallback)
                n->unrefCallback(n->data);
        }
        if (data) {
            n->data = data;
            if (n->refCallback)
                n->refCallback(data);
        }
    }
}

void* KFNodeGetData(KFNODE node) {
    if (node)
        return ((Node*)node)->data;
    return NULL;
}

KFNODE KFNodeLeft(KFNODE node) {
    if (node)
        return ((Node*)node)->prev;
    return NULL;
}

KFNODE KFNodeRight(KFNODE node) {
    if (node)
        return ((Node*)node)->next;
    return NULL;
}

void KFNodeInsertCenter(KFNODE node, KFNODE left, KFNODE right) {
    KFNODE prev = left;
    KFNODE next = right;
    ((Node*)node)->prev = prev;
    ((Node*)node)->next = next;
    ((Node*)prev)->next = node;
    ((Node*)next)->prev = node;
}

void KFNodeInsertLeft(KFNODE node, KFNODE left) {
    Node* n = (Node*)node;
    Node* f = (Node*)left;
    n->prev = f->prev;
    n->next = f;
    f->prev = n;
    if (n->prev)
        n->prev->next = n;
}

void KFNodeInsertRight(KFNODE node, KFNODE right) {
    Node* n = (Node*)node;
    Node* b = (Node*)right;
    n->next = b->next;
    n->prev = b;
    b->next = n;
    if (n->next)
        n->next->prev = n;
}

void KFNodeClearLeft(KFNODE node) {
    Node* n = (Node*)node;
    Node* t = n->prev;
    n->prev = NULL;
    if (t)
        t->next = NULL;
}

void KFNodeClearRight(KFNODE node) {
    Node* n = (Node*)node;
    Node* t = n->next;
    n->next = NULL;
    if (t)
        t->prev = NULL;
}

void KFNodeDestroy(KFNODE node) {
    if (node) {
        Node* n = (Node*)node;
        if (n->data) {
            if (n->unrefCallback)
                n->unrefCallback(n->data);
        }
        
        if (n->prev)
            n->prev->next = n->next;
        if (n->next)
            n->next->prev = n->prev;
        
        free(node);
    }
}

void KFNodeDestroyLeft(KFNODE node) {
    if (node) {
        Node* n = (Node*)node;
        while (n) {
            Node* prev = n->prev;
            if (n->data) {
                if (n->unrefCallback)
                    n->unrefCallback(n->data);
            }
            
            free(n);
            n = prev;
        }
    }
}

void KFNodeDestroyRight(KFNODE node) {
    if (node) {
        Node* n = (Node*)node;
        while (n) {
            Node* next = n->next;
            if (n->data) {
                if (n->unrefCallback)
                    n->unrefCallback(n->data);
            }
            
            free(n);
            n = next;
        }
    }
}

void KFNodeDestroyAll(KFNODE node) {
    if (node) {
        Node* n = (Node*)node;
        KFNodeDestroyLeft(n->prev);
        KFNodeDestroyRight(n->next);
        KFNodeDestroy(node);
    }
}