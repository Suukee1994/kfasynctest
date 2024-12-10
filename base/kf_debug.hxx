#ifndef __KF_BASE__DEBUG_H
#define __KF_BASE__DEBUG_H

#include <base/kf_base.hxx>

#define KF_LOG_LEVEL_VERBOSE 0
#define KF_LOG_LEVEL_INFO    1
#define KF_LOG_LEVEL_WARNING 2
#define KF_LOG_LEVEL_ERROR   3
#define KF_LOG_LEVEL_I_TRACE 4
#define KF_LOG_LEVEL_I_DEBUG 5

typedef void (*KFLogCallback)(const char* log);

void KFAPI KFDbgLogSetCallback(KFLogCallback callback);
void KFAPI KFDbgLogSetLevel(int level);
void KFAPI KFDbgLogOn();
void KFAPI KFDbgLogOff();

#endif //__KF_BASE__DEBUG_H