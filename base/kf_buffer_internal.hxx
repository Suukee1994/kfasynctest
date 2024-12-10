#ifndef __KF_BASE__BUFFER_INTERNAL_H
#define __KF_BASE__BUFFER_INTERNAL_H

#include <base/kf_buffer.hxx>

#ifndef KF_INTERFACE_ID_USE_GUID
#define _INTERNAL_KF_INTERFACE_ID_BUFFER "__kf_iid_buffer"
#else
#define _INTERNAL_KF_INTERFACE_ID_BUFFER "1D512C7CDA4C4CE3B3F6F75167182478"
#endif
struct IKFBuffer_I : public IKFBuffer
{
    virtual void SetPlatformType(int type) = 0;
    virtual void SetReleaseCallback(KFBufferReleaseCallback callback, void* userdata) = 0;
};

#endif //__KF_BASE__BUFFER_INTERNAL_H