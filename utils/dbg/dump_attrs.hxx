#ifndef __KF_UTIL__DBGTOOL_DUMP_ATTRS_H
#define __KF_UTIL__DBGTOOL_DUMP_ATTRS_H

#include <base/kf_attr.hxx>
#include <base/kf_ptr.hxx>
#include <base/kf_log.hxx>

void PrintAllKFAttributes(IKFAttributes* attr, bool outputType)
{
    int count = attr->GetItemCount();
    KFLOG("Attributes Count: %d", count);
    KFLOG("%s", "---------------------------------");
    for (int i = 0; i < count; i++) {
        KFPtr<IKFBuffer> name;
        attr->GetItemName(i, &name);
        if (outputType) {
            switch (attr->GetItemType(KFGetBufferAddress<const char*>(name.Get()))) {
            case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT32:
                KFLOG(" - %s (%s)", KFGetBufferAddress<const char*>(name.Get()), "UINT32");
                break;
            case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT64:
                KFLOG(" - %s (%s)", KFGetBufferAddress<const char*>(name.Get()), "UINT64");
                break;
            case KF_ATTRIBUTE_TYPE::KF_ATTR_DOUBLE:
                KFLOG(" - %s (%s)", KFGetBufferAddress<const char*>(name.Get()), "DOUBLE");
                break;
            case KF_ATTRIBUTE_TYPE::KF_ATTR_STRING:
                KFLOG(" - %s (%s)", KFGetBufferAddress<const char*>(name.Get()), "STRING");
                break;
            case KF_ATTRIBUTE_TYPE::KF_ATTR_BINARY:
                KFLOG(" - %s (%s)", KFGetBufferAddress<const char*>(name.Get()), "BINARY");
                break;
            case KF_ATTRIBUTE_TYPE::KF_ATTR_OBJECT:
                KFLOG(" - %s (%s)", KFGetBufferAddress<const char*>(name.Get()), "OBJECT");
                break;
            default:
                KFLOG(" - %s", KFGetBufferAddress<const char*>(name.Get()));
            }
        }else{
            KFLOG(" - %s", KFGetBufferAddress<const char*>(name.Get()));
        }
    }
    KFLOG("%s", "---------------------------------");
}

#endif //__KF_UTIL__DBGTOOL_DUMP_ATTRS_H