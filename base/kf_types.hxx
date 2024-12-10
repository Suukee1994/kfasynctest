#ifndef __KF_TYPES_H
#define __KF_TYPES_H

/////// *** Number TYPES *** ///////

#if _MSC_VER
typedef __int64           KF_INT64;
typedef __int32           KF_INT32;
typedef __int16           KF_INT16;
typedef __int8            KF_INT8;

typedef unsigned __int64  KF_UINT64;
typedef unsigned __int32  KF_UINT32;
typedef unsigned __int16  KF_UINT16;
typedef unsigned __int8   KF_UINT8;

typedef unsigned __int32  KF_BOOL;
#else
#include <stdint.h>

typedef int64_t           KF_INT64;
typedef int32_t           KF_INT32;
typedef int16_t           KF_INT16;
typedef int8_t            KF_INT8;

typedef uint64_t          KF_UINT64;
typedef uint32_t          KF_UINT32;
typedef uint16_t          KF_UINT16;
typedef uint8_t           KF_UINT8;

typedef uint32_t          KF_BOOL;
#endif

/////// *** KF_RECT *** ///////

struct KF_RECT
{
    KF_INT32 left;
    KF_INT32 top;
    KF_INT32 right;
    KF_INT32 bottom;

    bool operator==(const KF_RECT& other) const
    { return left == other.left && top == other.top && right == other.right && bottom == other.bottom; }

    bool IsEmpty() { return (Width() + Height() == 0); }
    KF_INT32 Width() const { return right - left; }
    KF_INT32 Height() const { return bottom - top; }
};

inline KF_RECT KF_RECT_MAKER(KF_INT32 left, KF_INT32 top, KF_INT32 right, KF_INT32 bottom)
{
    KF_RECT temp = {left, top, right, bottom};
    return temp;
}

/////// *** KF_SIZE *** ///////

struct KF_SIZE
{
    KF_INT32 width;
    KF_INT32 height;
    bool operator==(const KF_SIZE& other) const
    { return width == other.width && height == other.height; }
};

inline KF_SIZE KF_SIZE_MAKER(KF_INT32 width, KF_INT32 height)
{
    KF_SIZE temp = {width, height};
    return temp;
}

/////// *** KF_POINT *** ///////

struct KF_POINT
{
    KF_INT32 x;
    KF_INT32 y;
    bool operator==(const KF_POINT& other) const
    { return x == other.x && y == other.y; }
};

inline KF_POINT KF_POINT_MAKER(KF_INT32 x, KF_INT32 y)
{
    KF_POINT temp = {x, y};
    return temp;
}

/////// *** KF_RATIO *** ///////

struct KF_RATIO
{
    KF_INT32 num;
    KF_INT32 den;
    bool operator==(const KF_RATIO& other) const
    { return num == other.num && den == other.den; }
};

inline KF_RATIO KF_RATIO_MAKER(KF_INT32 num, KF_INT32 den)
{
    KF_RATIO temp = {num, den};
    return temp;
}

/////// *** KF_RESULT *** ///////

enum KF_RESULT
{
    KF_OK = 0,
    KF_NO = 1,

    KF_ERROR,
    KF_ABORT,
    KF_PENDING,
    KF_UNEXPECTED,
    KF_SHUTDOWN,
    KF_TIMEOUT,
    KF_ACCESS_DENIED,
    KF_OUT_OF_MEMORY,
    KF_BUF_TOO_SMALL,
    KF_RE_ENTRY,
    KF_INIT_ERROR,
    KF_IO_ERROR,
    KF_NO_INTERFACE,
    KF_NO_MATCH,

    KF_INVALID_ARG,
    KF_INVALID_PTR,
    KF_INVALID_DATA,
    KF_INVALID_STATE,
    KF_INVALID_INPUT,
    KF_INVALID_OUTPUT,
    KF_INVALID_OBJECT,
    KF_INVALID_OPERATION,

    KF_NOT_FOUND,
    KF_NOT_SUPPORTED,
    KF_NOT_INITIALIZED,
    KF_NOT_IMPLEMENTED,

    KF_NETWORK_DATA_INVALID,
    KF_NETWORK_JSON_INVALID,
    KF_NETWORK_SOCKET_INVALID,
    KF_NETWORK_CONNECT_FAIL,
    KF_NETWORK_CONNECT_TIMEOUT,
    KF_NETWORK_TOO_MAY_REQUESTS,
    KF_NETWORK_SEND_FAIL,
    KF_NETWORK_RECV_FAIL,
    KF_NETWORK_PROTOCOL_ERROR,
    KF_NETWORK_CRYPT_ERROR,

#ifdef KF_USE_MEDIA
    KF_MEDIA_ERR_UNKNOWN,
    KF_MEDIA_ERR_EXTERNAL,
    KF_MEDIA_ERR_HARDWARE,
    KF_MEDIA_ERR_UNAVAILABLE,
    KF_MEDIA_ERR_UNSUPPORTED,
    KF_MEDIA_ERR_CANCELLED,
    KF_MEDIA_ERR_STOPPED,
    KF_MEDIA_ERR_CLOSED,
    KF_MEDIA_ERR_ALLOC,
    KF_MEDIA_ERR_PARSE,
    KF_MEDIA_ERR_PROBE,
    KF_MEDIA_ERR_SEEK,

    KF_MEDIA_ERR_RENDER_HARDWARE,
    KF_MEDIA_ERR_RENDER_DISPLAY,
    KF_MEDIA_ERR_RENDER_PROCESSING,
    KF_MEDIA_ERR_RENDER_OPEN,
    KF_MEDIA_ERR_RENDER_START,
    KF_MEDIA_ERR_RENDER_PAUSE,
    KF_MEDIA_ERR_RENDER_STOP,
    KF_MEDIA_ERR_RENDER_RATE,

    KF_MEDIA_UNSUPPORTED_CODEC,
    KF_MEDIA_UNSUPPORTED_SERVICE,
    KF_MEDIA_UNSUPPORTED_BYTESTREAM,
    KF_MEDIA_UNSUPPORTED_COLOR_SPACE,
    KF_MEDIA_UNSUPPORTED_COLOR_FORMAT,
    KF_MEDIA_UNSUPPORTED_FORMAT_CHANGE,

    KF_MEDIA_BAD_REQUEST,
    KF_MEDIA_WRONG_STATE,
    KF_MEDIA_STATE_PENDING,

    KF_MEDIA_BUF_TOO_SMALL,
    KF_MEDIA_ALREADY_EXISTS,

    KF_MEDIA_FIXED_STREAMS,
    KF_MEDIA_END_OF_STREAM,
    KF_MEDIA_FLUSH_NEEDED,
    KF_MEDIA_NEED_MORE_DATA,
    KF_MEDIA_FORMAT_CHANGED,
    KF_MEDIA_FORMAT_CHANGED_BREAK,

    KF_MEDIA_INVALID_DATA,
    KF_MEDIA_INVALID_FILE,
    KF_MEDIA_INVALID_TYPE,
    KF_MEDIA_INVALID_COLOR,
    KF_MEDIA_INVALID_CODEC,
    KF_MEDIA_INVALID_CLOCK,
    KF_MEDIA_INVALID_INPUT,
    KF_MEDIA_INVALID_INDEX,
    KF_MEDIA_INVALID_BUFFER,
    KF_MEDIA_INVALID_FORMAT,
    KF_MEDIA_INVALID_STREAM,
    KF_MEDIA_INVALID_HEADER,
    KF_MEDIA_INVALID_SAMPLE,
    KF_MEDIA_INVALID_OBJECT,
    KF_MEDIA_INVALID_PROFILE,
    KF_MEDIA_INVALID_POSITION,
    KF_MEDIA_INVALID_TIMESTAMP,
    KF_MEDIA_INVALID_DESCRIPTOR,
    KF_MEDIA_INVALID_PRIVATE_DATA,

    KF_MEDIA_NOT_INITIALIZED,
    KF_MEDIA_NOT_ACCEPTING,
    KF_MEDIA_NOT_WRITABLE,
    KF_MEDIA_NOT_SEEKABLE,
    KF_MEDIA_NOT_FOUND,
    KF_MEDIA_NOT_SET,

    KF_MEDIA_SCHEME_HANDLER_NOT_FOUND,
    KF_MEDIA_STREAM_HANDLER_NOT_FOUND,
    KF_MEDIA_CODEC_FILTER_NOT_FOUND,
    KF_MEDIA_DECODER_NOT_FOUND,
    KF_MEDIA_ENCODER_NOT_FOUND,

    KF_MEDIA_NO_ANY_BUFFER,
    KF_MEDIA_NO_SERVICE,
    KF_MEDIA_NO_STREAM,
    KF_MEDIA_NO_SAMPLE,
    KF_MEDIA_NO_TIMESTAMP,
    KF_MEDIA_NO_DURATION,
    KF_MEDIA_NO_CLOCK,
    KF_MEDIA_NO_TIME_SOURCE,

    KF_MEDIA_NO_SOURCE,
    KF_MEDIA_NO_CODEC,
    KF_MEDIA_NO_SINK,

    KF_MEDIA_FAILED_SOURCE,
    KF_MEDIA_FAILED_CODEC,
    KF_MEDIA_FAILED_SINK,
#endif

    KF_RESULT_RESERVED
};

#define KF_SUCCEEDED(x) ((x) == KF_OK)
#define KF_FAILED(x)    ((x) != KF_OK)

/////// *** OTHERS *** ///////

const char* KFTypeGetResultString(KF_RESULT result);

#define KF_TRUE  1
#define KF_FALSE 0

#define _KF_FAILED_END(kr)            if (KF_FAILED((kr))) { return; }
#define _KF_FAILED_RET(kr)            if (KF_FAILED((kr))) { return kr; }
#define _KF_FAILED_RETX(kr,x)         if (KF_FAILED((kr))) { return x; }
#define _KF_FAILED_BREAK(kr)          if (KF_FAILED((kr))) { break; }
#define _KF_FAILED_CONTINUE(kr)       if (KF_FAILED((kr))) { continue; }
#define _KF_FAILED_GOTO_TAG(kr,tag)   if (KF_FAILED((kr))) { goto tag; }

#define _KF_FCC(fcc) \
    ((((unsigned)(fcc) & 0xFF) << 24) | \
    (((unsigned)(fcc) & 0xFF00) << 8) | \
    (((unsigned)(fcc) & 0xFF0000) >> 8) | \
    (((unsigned)(fcc) & 0xFF000000) >> 24))

#define _KF_MAX(a, b) ((a) > (b) ? (a) : (b))
#define _KF_MIN(a, b) ((a) > (b) ? (b) : (a))
#define _KF_CLAMP(a, min, max) (((a) < (min)) ? (min) : (((a) > (max)) ? (max) : (a)))

#define _KF_ALIGN_UP(x, align)   (((x) + (align) - 1) & ~((align) - 1))
#define _KF_ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

#define _KF_HAVE_FLAG(flags, x) (((flags) & (x)) != 0)

#endif //__KF_TYPES_H