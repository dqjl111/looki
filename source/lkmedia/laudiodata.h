#ifndef LAUDIODATA_H
#define LAUDIODATA_H

#include "lkcore/livar.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include "lkmedia/3rd/miniaudio.h"

LK_NAMESPACE_BEGIN

class LKMEDIADEF LAudioEngineData : public LIVar
{
public:
    LAudioEngineData(const int listenerCount = MA_ENGINE_MAX_LISTENERS);
    virtual ~LAudioEngineData();

    ma_engine* engine(){
        return &m_engine;
    }

    inline bool isInit() const{
        return m_isInit;
    }

    int maxListenerCount();

    static std::shared_ptr<LAudioEngineData> engineData(uint8_t type);
    // 只查已存在的 engine，不会新建。lkmedia 的 ducking 用它避免
    // 「视频一播放就凭空打开音频设备」这种副作用。
    static std::shared_ptr<LAudioEngineData> existingEngineData(uint8_t type);
    static void setEngineData(uint8_t type, std::shared_ptr<LAudioEngineData> d);
private:
    ma_engine m_engine;
    bool m_isInit = false;
    static std::array<std::shared_ptr<LAudioEngineData>, 2> m_engines;
};

class LKMEDIADEF LSoundData : public LIVar
{
public:
    LSoundData(const std::filesystem::path& filePath,
               uint8_t type,
               std::shared_ptr<LAudioEngineData> ed = nullptr);
    virtual ~LSoundData();

    ma_sound* sound(){
        return &m_sound;
    }

    inline bool isInit() const{
        return m_isInit;
    }

    inline uint8_t type() const{
        return m_type;
    }

    inline const std::string& path() const{
        return m_path;
    }
private:
    ma_sound m_sound;
    std::string m_path;
    bool m_isInit = false;
    uint8_t m_type = 0;
};


LK_NAMESPACE_END
#endif // LAUDIODATA_H
