#ifndef __KF_SYS_BASE_NODE_H
#define __KF_SYS_BASE_NODE_H

typedef void* KFNODE;
typedef void (*KFNodeObjectCallback)(void*);

#ifdef __cplusplus
extern "C" {
#endif
    
KFNODE KFNodeCreate(void* data);
KFNODE KFNodeCreateEx(void* data, KFNodeObjectCallback refcb, KFNodeObjectCallback unrefcb);
void   KFNodeSetCallback(KFNODE node, KFNodeObjectCallback refcb, KFNodeObjectCallback unrefcb);

void   KFNodeSetData(KFNODE node, void* data);
void*  KFNodeGetData(KFNODE node);
KFNODE KFNodeLeft(KFNODE node);
KFNODE KFNodeRight(KFNODE node);
    
void KFNodeInsertCenter(KFNODE node, KFNODE left, KFNODE right);
void KFNodeInsertLeft(KFNODE node, KFNODE left);
void KFNodeInsertRight(KFNODE node, KFNODE right);
    
void KFNodeClearLeft(KFNODE node);
void KFNodeClearRight(KFNODE node);
    
void KFNodeDestroy(KFNODE node);
void KFNodeDestroyLeft(KFNODE node);
void KFNodeDestroyRight(KFNODE node);
void KFNodeDestroyAll(KFNODE node);
    
#ifdef __cplusplus
}
#endif

#define KFNodePrev KFNodeLeft
#define KFNodeNext KFNodeRight

#define KFNodeInsertPrev KFNodeInsertLeft
#define KFNodeInsertNext KFNodeInsertRight

#define KFNodeClearPrev KFNodeClearLeft
#define KFNodeClearNext KFNodeClearRight

#define KFNodeDestroyPrev KFNodeDestroyLeft
#define KFNodeDestroyNext KFNodeDestroyRight

#endif //__KF_SYS_BASE_NODE_H