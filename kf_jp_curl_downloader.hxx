#ifndef __KF_JP__CURL_DOWNLOADER_H
#define __KF_JP__CURL_DOWNLOADER_H

#include <base/kf_base.hxx>
#include <base/kf_buffer.hxx>
#include <base/kf_attr.hxx>
#include <async/kf_async_abstract.hxx>

#define KFJP_CURL_RESULT_CURLE_CODE           "CURLE_CODE"      //UINT32
#define KFJP_CURL_RESULT_HTTP_DATA            "HTTP_DATA"       //OBJ(IKFBuffer)
#define KFJP_CURL_RESULT_HTTP_CODE            "HTTP_CODE"       //UINT32
#define KFJP_CURL_RESULT_HTTP_HEADERS         "HTTP_HEADERS"    //OBJ(IKFBuffer)
#define KFJP_CURL_RESULT_CONTENT_TYPE         "CONTENT_TYPE"    //STR
#define KFJP_CURL_RESULT_REDIRECT_URL         "REDIRECT_URL"    //STR
#define KFJP_CURL_RESULT_REDIRECT_COUNT       "REDIRECT_COUNT"  //UINT32

#define KFJP_CURL_OPT_ENABLE_REUSE            "opt_enable_reuse"           //UINT32(Bool)
#define KFJP_CURL_OPT_REUSE_TCP_KEEPIDLE      "opt_reuse_tcp_keepidle"     //UINT32(Default:120s)
#define KFJP_CURL_OPT_REUSE_TCP_KEEPINTVL     "opt_reuse_tcp_keepintvl"    //UINT32(Default:30s)
#define KFJP_CURL_OPT_USE_TCP_FASTOPEN        "opt_use_tcp_fastopen"       //UINT32(Bool)
#define KFJP_CURL_OPT_DISABLE_FOLLOW301_302   "opt_disable_follow301_302"  //UINT32(Bool)
#define KFJP_CURL_OPT_MAX_REDIRECT_COUNT      "opt_max_redirect_count"     //UINT32
#define KFJP_CURL_OPT_REFERER                 "opt_referer"                //STR
/*
  以后备用：
  CURLOPT_NOBODY
  CURLOPT_DNS_SERVERS
*/

#define _KF_INTERFACE_ID_JP_CURL_DOWNLOADER "kf_iid_jp_curl_downloader"
struct IKFJPCurlDownloader : public IKFBaseObject {
	virtual KF_RESULT SetUrl(const char* url) = 0;
	virtual KF_RESULT SetUserAgent(const char* userAgent) = 0;
	virtual KF_RESULT SetCookie(const char* cookie) = 0;
	virtual KF_RESULT SetTimeout(int timeout_sec) = 0;
	virtual KF_RESULT SetPostData(IKFBuffer* postData) = 0;
	virtual KF_RESULT SetHeaders(IKFAttributes* headers) = 0;
	virtual KF_RESULT SetOptions(IKFAttributes* options) = 0;
	virtual KF_RESULT AddPostFile(const char* fieldName, const char* fileName, const char* contentType, IKFBuffer* data) = 0;

	enum LegacyCurlCallbackTypes {
		Startup,
		Finished
	};
	typedef bool (*LegacyCurlCallback)(void* curlHandle, LegacyCurlCallbackTypes type, void* state);
	virtual KF_RESULT SetLegacyCallback(LegacyCurlCallback cb, void* state) = 0;

	virtual KF_RESULT StartTask(KASYNCOBJECT worker, IKFBaseObject* state, IKFAsyncCallback* callback) = 0;
	virtual KF_RESULT CloseTask() = 0;
};

KF_RESULT KFAPI KFJPCreateCurlDownloader(IKFJPCurlDownloader** curlDownloader);
KF_RESULT KFAPI KFJPCopyCurlDownloadedResult(IKFAsyncResult* result, IKFBuffer** data, KF_UINT32* httpCode, KF_UINT32* curle, char* contentType);
KF_RESULT KFAPI KFJPCopyCurlDownloadedResponse(IKFAsyncResult* result, IKFBuffer** response);
KF_RESULT KFAPI KFJPCopyCurlDownloadedRedirectInfo(IKFAsyncResult* result, IKFBuffer** redirectUrl, KF_UINT32* redirectCount);

#endif //__KF_JP__CURL_DOWNLOADER_H