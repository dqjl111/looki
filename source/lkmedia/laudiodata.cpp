#include "laudiodata.h"
#include "lkcore/ldebug.h"
#include <algorithm>
#include "lkmedia/lsound.h"

LK_NAMESPACE_BEGIN

std::array<std::shared_ptr<LAudioEngineData>, 2> LAudioEngineData::m_engines;

LAudioEngineData::LAudioEngineData(const int listenerCount){
    ma_engine_config config = ma_engine_config_init();
    config.listenerCount    = std::clamp(listenerCount, 1, MA_ENGINE_MAX_LISTENERS);
    if ( ma_engine_init( &config, &m_engine ) != MA_SUCCESS )
    {
        lWarn() << "Failed to initialize audio engine.";
    }else{
        m_isInit = true;
    }
}

LAudioEngineData::~LAudioEngineData(){
    if(m_isInit){
        ma_engine_stop(&m_engine);
        ma_engine_uninit( &m_engine );
        m_isInit = false;
    }
}

int LAudioEngineData::maxListenerCount()
{
    if(m_isInit){
        return ma_engine_get_listener_count(&m_engine);
    }
    return 0;
}

std::shared_ptr<LAudioEngineData> LAudioEngineData::engineData(uint8_t type)
{
    if(type < m_engines.size()){
        if(m_engines[type] == nullptr){
            m_engines[type] = std::make_shared<LAudioEngineData>();
        }
        return m_engines[type];
    }
    return nullptr;
}

std::shared_ptr<LAudioEngineData> LAudioEngineData::existingEngineData(uint8_t type)
{
    if(type < m_engines.size()){
        return m_engines[type];
    }
    return nullptr;
}

void LAudioEngineData::setEngineData(uint8_t type, std::shared_ptr<LAudioEngineData> d)
{
    if(type <  m_engines.size()){
        m_engines[type] = d;
    }
}


LSoundData::LSoundData(const std::filesystem::path& filePath,
                       uint8_t type,
                       std::shared_ptr<LAudioEngineData> ed)
    : m_type(type), m_path(filePath.string())
{
    if(ed == nullptr){
        ed = LAudioEngineData::engineData(type);
    }

    if(ed != nullptr && ed->isInit()){
        int flags = 0;
        if(type == (uint8_t)LSound::Type::Sound){
            flags = MA_SOUND_FLAG_DECODE;
        }else{
            flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;
        }

        if (ma_sound_init_from_file(ed->engine(),
                                    m_path.data(),
                                    flags,
                                    nullptr,
                                    nullptr,
                                    &m_sound ) != MA_SUCCESS )
        {
            lWarn() << "Failed to initialize sound from source: " << m_path;
        }else{
            m_isInit = true;
        }
    }
}

LSoundData::~LSoundData()
{
    if(m_isInit){
        ma_sound_uninit( &m_sound );
        m_isInit = false;
    }
}



LK_NAMESPACE_END

