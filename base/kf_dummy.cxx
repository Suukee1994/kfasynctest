#include <base/kf_base.hxx>

class DummyObject : public IKFDummyObject
{
    KF_IMPL_DECL_REFCOUNT;
public:
    DummyObject() throw() : _ref_count(1), _dummyId(0) {}
    explicit DummyObject(KF_UINT32 dummyId) : _ref_count(1), _dummyId(dummyId) {}
    virtual ~DummyObject() throw() {}

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_DUMMY_OBJECT)) {
            *ppv = static_cast<IKFDummyObject*>(this);
            Retain();
            return KF_OK;
        }
        return KF_NO_INTERFACE;
    }

    virtual KREF Retain()
    { KF_IMPL_RETAIN_FUNC(_ref_count); }
    virtual KREF Recycle()
    { KF_IMPL_RECYCLE_FUNC(_ref_count); }

public:
    virtual KF_UINT32 GetDummyTypeId() { return _dummyId; }

private:
    KF_UINT32 _dummyId;
};

// ***************

KF_RESULT KFAPI KFCreateDummyObject(KF_UINT32 dummyId, IKFDummyObject** dummyObject)
{
    if (dummyObject == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) DummyObject(dummyId);
    if (result == nullptr)
        return KF_OUT_OF_MEMORY;

    *dummyObject = result;
    return KF_OK;
}

KF_RESULT KFAPI KFIsDummyObject(IKFBaseObject* object)
{
	if (object == nullptr)
		return KF_INVALID_ARG;

	IKFDummyObject* dummy = nullptr;
    auto result = KFBaseGetInterface(object, _KF_INTERFACE_ID_DUMMY_OBJECT, &dummy);
	KF_SAFE_RELEASE(dummy);

	return result;
}