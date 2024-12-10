#include <base/kf_ptr.hxx>
#include <base/kf_attr.hxx>
#include <base/kf_array_list.hxx>
#include <base/kf_buffer.hxx>
#include <utils/async_callback_router.hxx>
#include "kf_jp_curl_downloader.hxx"
#include <curl/curl.h>

#if defined(_MSC_VER) && defined(GetObject)
#undef GetObject //fuck Microsoft...
#endif

#define _URL "url"
#define _USER_AGENT "ua"
#define _COOKIE "cookie"
#define _TIMEOUT "timeout"

#define _FORM_FIELD_NAME          "name"
#define _FORM_FIELD_FILENAME      "fname"
#define _FORM_FIELD_CONTENTTYPE   "ct"
#define _FORM_FIELD_DATA          "data"

#define _DEF_TIMEOUT 30
#define _DEF_MAX_REDIRECT_COUNT    16
#define _DEF_REUSE_TCP_KEEPIDLE   120
#define _DEF_REUSE_TCP_KEEPINTVL  30

#define _CURL_ENABLE 1L
#define _CURL_DISABLE 0L

class JPCurlDownloader : public IKFJPCurlDownloader {
	KF_IMPL_DECL_REFCOUNT;
public:
	JPCurlDownloader() noexcept : _ref_count(1) {
        _info = nullptr;
        _callback = nullptr;
        _callback_data = nullptr;
        _curl = NULL;
        _headers = NULL;
        _options.enableReuse = false;
        _options.reuse_TcpKeepIdle = _DEF_REUSE_TCP_KEEPIDLE;
        _options.reuse_TcpKeepIntvl = _DEF_REUSE_TCP_KEEPINTVL;
        _options.useTcpFastOpen = false;
        _options.disableFollow301_302 = false;
        _options.maxRedirectCount = _DEF_MAX_REDIRECT_COUNT;
    }
	virtual ~JPCurlDownloader() noexcept {
        FreeCURL();
        if (_headers != NULL)
            curl_slist_free_all(_headers);
    }

    KF_RESULT Prepare() {
        auto r = KFCreateAttributesWithoutObserver(&_info);
        _KF_FAILED_RET(r);
        r = KFCreateObjectArrayList(&_formDataList);
        _KF_FAILED_RET(r);
        _networkTask.SetCallback(this, &JPCurlDownloader::OnInvoke);
        return KF_OK;
    }

public: //IKFBaseObject
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv) {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_JP_CURL_DOWNLOADER)) {
            *ppv = static_cast<IKFJPCurlDownloader*>(this);
            Retain();
            return KF_OK;
        }
        return KF_NO_INTERFACE;
    }

    virtual KREF Retain() {
        KF_IMPL_RETAIN_FUNC(_ref_count);
    }
    virtual KREF Recycle() {
        KF_IMPL_RECYCLE_FUNC(_ref_count);
    }

public: //IKFJPCurlDownloader
    virtual KF_RESULT SetUrl(const char* url) {
        return _info->SetString(_URL, url);
    }

    virtual KF_RESULT SetUserAgent(const char* userAgent) {
        return _info->SetString(_USER_AGENT, userAgent);
    }

    virtual KF_RESULT SetCookie(const char* cookie) {
        if (cookie == nullptr) {
            return _info->DeleteItem(_COOKIE);
        }
        return _info->SetString(_COOKIE, cookie);
    }

    virtual KF_RESULT SetTimeout(int timeout_sec) {
        return _info->SetUINT32(_TIMEOUT, (KF_UINT32)timeout_sec);
    }

    virtual KF_RESULT SetPostData(IKFBuffer* postData) {
        return KFCreateMemoryBufferCopy(postData, postData->GetCurrentLength(), _postData.ResetAndGetAddressOf());
    }

    virtual KF_RESULT SetHeaders(IKFAttributes* headers) {
        if (_headers != NULL) {
            curl_slist_free_all(_headers);
            _headers = NULL;
        }
        if (headers != NULL) {
            struct curl_slist* slist = NULL;
            int count = headers->GetItemCount();
            for (int i = 0; i < count; i++) {
                KFPtr<IKFBuffer> nameBuf;
                headers->GetItemName(i, &nameBuf);
                auto name = KFGetBufferAddress<const char*>(nameBuf.Get());
                auto type = headers->GetItemType(name);
                if (type == KF_ATTRIBUTE_TYPE::KF_ATTR_STRING) {
                    KFPtr<IKFBuffer> valueBuf;
                    headers->GetStringAlloc(name, &valueBuf);
                    auto value = KFGetBufferAddress<const char*>(valueBuf.Get());
                    auto header = (char*)malloc(strlen(name) + strlen(value) + 16);
                    if (header != nullptr) {
                        sprintf(header, "%s: %s", name, value);
                        slist = curl_slist_append(slist, header);
                        free(header);
                    }
                }
            }
            _headers = slist;
        }
        return KF_OK;
    }

    virtual KF_RESULT SetOptions(IKFAttributes* options) {
        if (options == nullptr)
            return KF_INVALID_ARG;

        _options.enableReuse = KFGetAttributeBool(options, KFJP_CURL_OPT_ENABLE_REUSE);
        _options.reuse_TcpKeepIdle = (int)KFGetAttributeUINT32(options, KFJP_CURL_OPT_REUSE_TCP_KEEPIDLE, _DEF_REUSE_TCP_KEEPIDLE);
        _options.reuse_TcpKeepIntvl = (int)KFGetAttributeUINT32(options, KFJP_CURL_OPT_REUSE_TCP_KEEPINTVL, _DEF_REUSE_TCP_KEEPINTVL);
        _options.useTcpFastOpen = KFGetAttributeBool(options, KFJP_CURL_OPT_USE_TCP_FASTOPEN);
        _options.disableFollow301_302 = KFGetAttributeBool(options, KFJP_CURL_OPT_DISABLE_FOLLOW301_302);
        _options.maxRedirectCount = (int)KFGetAttributeUINT32(options, KFJP_CURL_OPT_MAX_REDIRECT_COUNT, _DEF_MAX_REDIRECT_COUNT);
        options->GetStringAlloc(KFJP_CURL_OPT_REFERER, &_options.referer);
        return KF_OK;
    }

    virtual KF_RESULT AddPostFile(const char* fieldName, const char* fileName, const char* contentType, IKFBuffer* data) {
        if (fieldName == nullptr || data == nullptr)
            return KF_INVALID_ARG;

        KFPtr<IKFAttributes> form;
        auto r = KFCreateAttributesWithoutObserver(&form);
        _KF_FAILED_RET(r);
        
        form->SetString(_FORM_FIELD_NAME, fieldName);
        form->SetObject(_FORM_FIELD_DATA, data);
        form->SetString(_FORM_FIELD_FILENAME, fileName);
        if (contentType) form->SetString(_FORM_FIELD_CONTENTTYPE, contentType);

        r = _formDataList->AddElement(form.Get()) ? KF_OK : KF_OUT_OF_MEMORY;
        return r;
    }

    virtual KF_RESULT SetLegacyCallback(LegacyCurlCallback cb, void* state) {
        _callback = cb;
        _callback_data = state;
        return KF_OK;
    }

    virtual KF_RESULT StartTask(KASYNCOBJECT worker, IKFBaseObject* state, IKFAsyncCallback* callback) {
        if (worker == nullptr || callback == nullptr)
            return KF_INVALID_ARG;
        if (_taskResult != nullptr)
            return KF_RE_ENTRY;

        KFPtr<IKFAsyncResult> externalResult;
        auto r = KFAsyncCreateResult(callback, state, nullptr, &externalResult);
        _KF_FAILED_RET(r);
        r = KFCreateAttributesWithoutObserver(_taskResult.ResetAndGetAddressOf());
        _KF_FAILED_RET(r);
        r = KFCreateObjectArrayListFromInitCount(_dataList.ResetAndGetAddressOf(), 5);
        _KF_FAILED_RET(r);
        r = KFCreateObjectArrayListFromInitCount(_headerList.ResetAndGetAddressOf(), 5);
        _KF_FAILED_RET(r);
        r = KFAsyncPutWorkItem(worker, &_networkTask, externalResult.Get());
        return r;
    }

    virtual KF_RESULT CloseTask() {
        if (_taskResult.Get() == nullptr)
            return KF_NOT_INITIALIZED;
        _taskResult.Reset();
        _dataList.Reset();
        _headerList.Reset();
        _postData.Reset();
        _formDataList->RemoveAllElements();
        return KF_OK;
    }

private:
    CURL* GetCURL() {
        if (_curl != NULL) {
            if (_options.enableReuse) {
                return _curl;
            } else {
                curl_easy_cleanup(_curl);
                _curl = NULL;
            }
        }

        auto curl = curl_easy_init();
        if (curl != NULL) {
            _curl = curl;
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, _CURL_ENABLE);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, _CURL_DISABLE);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, _CURL_DISABLE);
            if (_options.enableReuse) {
                curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, _CURL_DISABLE);
                curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, _CURL_ENABLE);
                curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, _options.reuse_TcpKeepIdle);
                curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, _options.reuse_TcpKeepIntvl);
            }
#ifdef HAS_TCP_FASTOPEN
            if (_options.useTcpFastOpen) curl_easy_setopt(curl, CURLOPT_TCP_FASTOPEN, _CURL_ENABLE);
#endif
            curl_easy_setopt(curl, CURLOPT_HEADER, _CURL_DISABLE);
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &JPCurlDownloader::_curl_HeaderCallback);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &JPCurlDownloader::_curl_WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        }
        return _curl;
    }

    void FreeCURL(bool force = false) {
        if (_curl != NULL && !_options.enableReuse && !force) {
            curl_easy_cleanup(_curl);
            _curl = NULL;
        }
    }

    void OnInvoke(IKFAsyncResult* result) {
        KFPtr<IKFAsyncResult> externalResult;
        auto r = KFBaseGetInterface(result->GetStateNoRef(), _KF_INTERFACE_ID_ASYNC_RESULT, &externalResult);
        if (KF_SUCCEEDED(r)) {
            CURL* curl = NULL;
            r = _info->HasItem(_URL);
            if (KF_FAILED(r))
                r = KF_INVALID_INPUT;

            if (KF_SUCCEEDED(r)) {
                curl = GetCURL();
                if (curl == NULL)
                    r = KF_INIT_ERROR;
            }

            struct curl_httppost* post = NULL;
            struct curl_httppost* postLast = NULL;

            if (KF_SUCCEEDED(r)) {
                int formCount = _formDataList->GetElementCount();
                if (formCount > 0) {
                    for (int i = 0; i < formCount; i++) {
                        KFPtr<IKFAttributes> form;
                        KFGetArrayListElement<IKFAttributes>(_formDataList.Get(), i, _KF_INTERFACE_ID_ATTRIBUTES, &form);
                        if (form == nullptr) continue;
                        AddFormDataFromAttributes(form.Get(), &post, &postLast);
                    }
                }
            }

            if (KF_SUCCEEDED(r)) {
                KFPtr<IKFBuffer> url;
                r = _info->GetStringAlloc(_URL, &url);
                if (KF_SUCCEEDED(r) && curl_easy_setopt(curl, CURLOPT_URL, KFGetBufferAddress<const char*>(url.Get())) != CURLE_OK)
                    r = KF_INVALID_DATA;
            }
            if (KF_SUCCEEDED(r) && KF_SUCCEEDED(_info->HasItem(_USER_AGENT))) {
                KFPtr<IKFBuffer> userAgent;
                r = _info->GetStringAlloc(_USER_AGENT, &userAgent);
                if (KF_SUCCEEDED(r) && curl_easy_setopt(curl, CURLOPT_USERAGENT, KFGetBufferAddress<const char*>(userAgent.Get())) != CURLE_OK)
                    r = KF_INVALID_DATA;
            }
            if (KF_SUCCEEDED(r) && KF_SUCCEEDED(_info->HasItem(_COOKIE))) {
                KFPtr<IKFBuffer> cookie;
                r = _info->GetStringAlloc(_COOKIE, &cookie);
                if (KF_SUCCEEDED(r) && curl_easy_setopt(curl, CURLOPT_COOKIE, KFGetBufferAddress<const char*>(cookie.Get())) != CURLE_OK)
                    r = KF_INVALID_DATA;
            }

            if (KF_SUCCEEDED(r)) {
                if (_postData != nullptr && _postData->GetCurrentLength() > 0) {
                    curl_easy_setopt(curl, CURLOPT_POST, _CURL_ENABLE);
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, KFGetBufferAddress<const char*>(_postData));
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (size_t)_postData->GetCurrentLength());
                }
            }

            if (KF_SUCCEEDED(r)) {
                if (post != NULL)
                    curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
            }

            if (KF_SUCCEEDED(r)) {
                int timeout = _DEF_TIMEOUT;
                if (KF_SUCCEEDED(_info->HasItem(_TIMEOUT))) {
                    KF_UINT32 t = 0;
                    _info->GetUINT32(_TIMEOUT, &t);
                    if (t > 3)
                        timeout = (int)t;
                }
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
            }

            if (KF_SUCCEEDED(r)) {
                if (_headers != NULL)
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, _headers);

                if (!_options.disableFollow301_302) {
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, _CURL_ENABLE);
                    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, _CURL_ENABLE);
                }
                curl_easy_setopt(curl, CURLOPT_MAXREDIRS, _options.maxRedirectCount);
                if (_options.referer) {
                    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, _CURL_DISABLE);
                    curl_easy_setopt(curl, CURLOPT_REFERER, KFGetBufferAddress<const char*>(_options.referer.Get()));
                }

                if (_callback != nullptr)
                    _callback(curl, LegacyCurlCallbackTypes::Startup, _callback_data);

                curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, _CURL_DISABLE);
                auto curle = curl_easy_perform(curl);
                _taskResult->SetUINT32(KFJP_CURL_RESULT_CURLE_CODE, (KF_UINT32)curle);
                if (curle != CURLE_OK)
                    r = KF_NETWORK_CONNECT_FAIL;
                if (curle == CURLE_OPERATION_TIMEDOUT)
                    r = KF_NETWORK_CONNECT_TIMEOUT;
            }

            if (curl != NULL) {
                long code = 0;
                if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code) == CURLE_OK)
                    _taskResult->SetUINT32(KFJP_CURL_RESULT_HTTP_CODE, (KF_UINT32)code);
                const char* ct = NULL;
                if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct) == CURLE_OK && ct != NULL)
                    _taskResult->SetString(KFJP_CURL_RESULT_CONTENT_TYPE, ct);
                const char* ru = NULL;
                if (curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &ru) == CURLE_OK && ru != NULL)
                    _taskResult->SetString(KFJP_CURL_RESULT_REDIRECT_URL, ru);
                long rc = 0;
                if (curl_easy_getinfo(curl, CURLINFO_REDIRECT_COUNT, &rc) == CURLE_OK)
                    _taskResult->SetUINT32(KFJP_CURL_RESULT_REDIRECT_COUNT, (KF_UINT32)rc);

                if (_callback != nullptr)
                    _callback(curl, LegacyCurlCallbackTypes::Finished, _callback_data);
                FreeCURL();
            }
        }

        if (_dataList->GetElementCount() > 0) {
            KFPtr<IKFBuffer> buf;
            r = KFMergeMemoryBufferUseListObject(_dataList, &buf);
            if (KF_SUCCEEDED(r))
                _taskResult->SetObject(KFJP_CURL_RESULT_HTTP_DATA, buf.Get());
        }
        if (_headerList->GetElementCount() > 0) {
            KFPtr<IKFBuffer> buf;
            r = KFMergeMemoryBufferUseListObject(_headerList, &buf);
            if (KF_SUCCEEDED(r))
                _taskResult->SetObject(KFJP_CURL_RESULT_HTTP_HEADERS, buf.Get());
        }
        
        externalResult->SetObject(_taskResult.Get());
        externalResult->SetResult(r);
        KFAsyncInvokeCallback(externalResult);
    }

    void SaveData2List(char* ptr, int len, IKFArrayList* dataList) {
        KFPtr<IKFBuffer> buf;
        if (KF_SUCCEEDED(KFCreateMemoryBuffer(len + 1, &buf))) {
            memcpy(KFGetBufferAddress<char*>(buf.Get()), ptr, len);
            buf->SetCurrentLength(len);
            dataList->AddElement(buf);
        }
    }

    void BodySaveData2List(char* ptr, int len) {
        SaveData2List(ptr, len, _dataList.Get());
    }

    void HeaderSaveData2List(char* ptr, int len) {
        SaveData2List(ptr, len, _headerList.Get());
    }

    void AddFormDataFromAttributes(IKFAttributes* form, curl_httppost** post, curl_httppost** last) {
        KFPtr<IKFBuffer> data;
        KFPtr<IKFBuffer> name;
        KFPtr<IKFBuffer> fileName;
        KFPtr<IKFBuffer> contentType;
        form->GetStringAlloc(_FORM_FIELD_NAME, &name);
        form->GetStringAlloc(_FORM_FIELD_FILENAME, &fileName);
        form->GetStringAlloc(_FORM_FIELD_CONTENTTYPE, &contentType);
        form->GetObject(_FORM_FIELD_DATA, _KF_INTERFACE_ID_BUFFER, (void**)&data);
        if (name != nullptr || data != nullptr || fileName == nullptr) {
            if (contentType == nullptr) {
                curl_formadd(post, last,
                    CURLFORM_COPYNAME, KFGetBufferAddress<const char*>(name.Get()),
                    CURLFORM_BUFFER, KFGetBufferAddress<const char*>(fileName.Get()),
                    CURLFORM_BUFFERPTR, data->GetAddress(),
                    CURLFORM_BUFFERLENGTH, data->GetCurrentLength(),
                    CURLFORM_END
                );
            } else {
                curl_formadd(post, last,
                    CURLFORM_COPYNAME, KFGetBufferAddress<const char*>(name.Get()),
                    CURLFORM_BUFFER, KFGetBufferAddress<const char*>(fileName.Get()),
                    CURLFORM_BUFFERPTR, data->GetAddress(),
                    CURLFORM_BUFFERLENGTH, data->GetCurrentLength(),
                    CURLFORM_CONTENTTYPE, KFGetBufferAddress<const char*>(contentType.Get()),
                    CURLFORM_END
                );
            }
        }
    }

    static size_t _curl_WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        size_t len = size * nmemb;
        ((JPCurlDownloader*)userdata)->BodySaveData2List(ptr, (int)len);
        return len;
    }

    static size_t _curl_HeaderCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        size_t len = size * nmemb;
        ((JPCurlDownloader*)userdata)->HeaderSaveData2List(ptr, (int)len);
        return len;
    }

private:
    struct Options {
        bool enableReuse;
        int reuse_TcpKeepIdle;
        int reuse_TcpKeepIntvl;
        bool useTcpFastOpen;
        bool disableFollow301_302;
        int maxRedirectCount;
        KFPtr<IKFBuffer> referer;
    } _options;

    KFPtr<IKFAttributes> _info;
    LegacyCurlCallback _callback;
    void* _callback_data;
    
    CURL* _curl;
    curl_slist* _headers;

    KFPtr<IKFBuffer> _postData;
    KFPtr<IKFAttributes> _taskResult;
    KFPtr<IKFArrayList> _dataList;
    KFPtr<IKFArrayList> _headerList;
    KFPtr<IKFArrayList> _formDataList;

    AsyncCallbackRouter<JPCurlDownloader> _networkTask;
};

// ***************

KF_RESULT KFAPI KFJPCreateCurlDownloader(IKFJPCurlDownloader** curlDownloader)
{
    if (curlDownloader == nullptr)
        return KF_INVALID_PTR;

    auto curl = new(std::nothrow) JPCurlDownloader();
    if (curl == nullptr)
        return KF_OUT_OF_MEMORY;

    auto result = curl->Prepare();
    if (KF_SUCCEEDED(result))
        *curlDownloader = curl;
    else
        curl->Recycle();

    return result;
}

KF_RESULT KFAPI KFJPCopyCurlDownloadedResult(IKFAsyncResult* result, IKFBuffer** data, KF_UINT32* httpCode, KF_UINT32* curle, char* contentType)
{
    if (result == nullptr)
        return KF_INVALID_ARG;

    KFPtr<IKFAttributes> attr;
    auto r = KFBaseGetInterface(result->GetObjectNoRef(), _KF_INTERFACE_ID_ATTRIBUTES, &attr);
    _KF_FAILED_RET(r);
    if (data) {
        r = attr->GetObject(KFJP_CURL_RESULT_HTTP_DATA, _KF_INTERFACE_ID_BUFFER, (void**)data);
        _KF_FAILED_RET(r);
    }
    if (httpCode) {
        r = attr->GetUINT32(KFJP_CURL_RESULT_HTTP_CODE, httpCode);
        _KF_FAILED_RET(r);
    }
    if (curle) {
        r = attr->GetUINT32(KFJP_CURL_RESULT_CURLE_CODE, curle);
        _KF_FAILED_RET(r);
    }
    if (contentType) {
        KFPtr<IKFBuffer> ct;
        if (KF_SUCCEEDED(attr->GetStringAlloc(KFJP_CURL_RESULT_CONTENT_TYPE, &ct)))
            strcpy(contentType, KFGetBufferAddress<const char*>(ct.Get()));
    }
    return r;
}

KF_RESULT KFAPI KFJPCopyCurlDownloadedResponse(IKFAsyncResult* result, IKFBuffer** response)
{
    if (result == nullptr || response == nullptr)
        return KF_INVALID_ARG;

    KFPtr<IKFAttributes> attr;
    auto r = KFBaseGetInterface(result->GetObjectNoRef(), _KF_INTERFACE_ID_ATTRIBUTES, &attr);
    _KF_FAILED_RET(r);

    KFPtr<IKFBuffer> headers;
    r = attr->GetObject(KFJP_CURL_RESULT_HTTP_HEADERS ,_KF_INTERFACE_ID_BUFFER, (void**)&headers);
    _KF_FAILED_RET(r);

    KFPtr<IKFBuffer> buf;
    r = KFCreateMemoryBuffer(headers->GetCurrentLength() + 16, &buf);
    _KF_FAILED_RET(r);
    memcpy(buf->GetAddress(), headers->GetAddress(), headers->GetCurrentLength());
    buf->SetCurrentLength(headers->GetCurrentLength());
    *response = buf.Detach();

    return r;
}

KF_RESULT KFAPI KFJPCopyCurlDownloadedRedirectInfo(IKFAsyncResult* result, IKFBuffer** redirectUrl, KF_UINT32* redirectCount) {
    if (result == nullptr)
        return KF_INVALID_ARG;

    KFPtr<IKFAttributes> attr;
    auto r = KFBaseGetInterface(result->GetObjectNoRef(), _KF_INTERFACE_ID_ATTRIBUTES, &attr);
    _KF_FAILED_RET(r);

    if (redirectUrl) {
        r = attr->GetStringAlloc(KFJP_CURL_RESULT_REDIRECT_URL, redirectUrl);
        _KF_FAILED_RET(r);
    }
    if (redirectCount) {
        r = attr->GetUINT32(KFJP_CURL_RESULT_REDIRECT_COUNT, redirectCount);
        _KF_FAILED_RET(r);
    }
    return KF_OK;
}