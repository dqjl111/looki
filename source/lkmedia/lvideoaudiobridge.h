#ifndef LVIDEOAUDIOBRIDGE_H
#define LVIDEOAUDIOBRIDGE_H

#include "lkcore/livar.h"

LK_NAMESPACE_BEGIN

// 方案 A 的音量协同：视频播放时压低 lkmedia(miniaudio) 主音量。
// ducked=true 时 gain 生效（0..1）；ducked=false 时恢复 1.0。
// LOOKI_VIDEO_AUDIO_DUCKING=0 时为空实现，lkmedia 不链接 lkmedia。
LKMEDIADEF void lVideoSetAudioDucking(bool ducked, double gain);

LK_NAMESPACE_END

#endif // LVIDEOAUDIOBRIDGE_H
