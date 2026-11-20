#include "lkmedia/lvideoaudiobridge.h"
#include "lkcore/lookiconfig.h"

#if LOOKI_VIDEO_AUDIO_DUCKING
#  include "lkmedia/laudiodata.h"
#endif

LK_NAMESPACE_BEGIN

void lVideoSetAudioDucking(bool ducked, double gain)
{
#if LOOKI_VIDEO_AUDIO_DUCKING
    const float volume = ducked ? static_cast<float>(gain) : 1.0f;
    // 用 existingEngineData 而不是 engineData：后者会在音效从未使用过时
    // 凭空创建一个 miniaudio engine（打开音频设备），这不是我们想要的副作用。
    for (uint8_t type = 0; type < 2; ++type) {
        std::shared_ptr<LAudioEngineData> ed = LAudioEngineData::existingEngineData(type);
        if (ed != nullptr && ed->isInit()) {
            ma_engine_set_volume(ed->engine(), volume);
        }
    }
#else
    (void)ducked;
    (void)gain;
#endif
}

LK_NAMESPACE_END
