#include "lkmedia/lvideobackend.h"
#include "lkcore/lookiconfig.h"
#include "lkmedia/lvideobackendnull.h"

// 只有被 CMake 选中的后端实现才会参与编译，因此这里按宏 include。
// 新增 Linux 后端时：写好 lvideobackendgstreamer.cpp/.h，在 CMake 里把
// looki_VIDEO_BACKEND 设为 GStreamer 即可，无需改动任何调用方。
#if LOOKI_VIDEO_BACKEND_MF
#  include "lkmedia/lvideobackendmf.h"
#endif
#if LOOKI_VIDEO_BACKEND_GSTREAMER
#  include "lkmedia/lvideobackendgstreamer.h"
#endif

LK_NAMESPACE_BEGIN

std::shared_ptr<LVideoBackend> LVideoBackend::create()
{
#if LOOKI_VIDEO_BACKEND_MF
    return std::make_shared<LVideoBackendMF>();
#elif LOOKI_VIDEO_BACKEND_GSTREAMER
    return std::make_shared<LVideoBackendGStreamer>();
#else
    return std::make_shared<LVideoBackendNull>();
#endif
}

LK_NAMESPACE_END
