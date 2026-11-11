#include "lsound.h"

#include "lkaudio/laudiodata.h"

LK_NAMESPACE_BEGIN
LSound::LSound()
{

}

LSound::LSound(const std::filesystem::path &filePath, Type type)
{
    load(filePath, type);
}

bool LSound::isInit()
{
    if(m_data != nullptr){
        return m_data->isInit();
    }
    return false;
}

bool LSound::load(const std::filesystem::path &filePath, Type type)
{
    m_data = std::make_shared<LSoundData>(filePath, (uint8_t)type);
    return m_data->isInit();
}

bool LSound::play()
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_start( m_data->sound() );
        return true;
    }
    return false;
}

bool LSound::stop()
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_stop( m_data->sound() );
        return true;
    }
    return false;
}

bool LSound::pause()
{
    return stop();
}

bool LSound::replay()
{
    seek( 0ull );
    return play();
}

bool LSound::isPlaying() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_is_playing( m_data->sound() ) == MA_TRUE;
    }
    return false;
}

bool LSound::isEnd() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_at_end( m_data->sound() ) == MA_TRUE;
    }
    return false;
}

float LSound::getDurationInSeconds() const
{
    float duration = 0.0f;
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_get_length_in_seconds( m_data->sound(), &duration );
    }
    return duration;
}

float LSound::getCursorInSeconds() const
{
    float cursor = 0.0f;
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_get_cursor_in_seconds( m_data->sound(), &cursor );
    }
    return cursor;
}

bool LSound::seek(uint64_t milliseconds)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_uint32 sampleRate;
        ma_sound_get_data_format( m_data->sound(), nullptr, nullptr, &sampleRate, nullptr, 0 );
        const ma_uint64 pcmFrame = ( sampleRate * milliseconds ) / 1000;
        ma_sound_seek_to_pcm_frame( m_data->sound(), pcmFrame );
        return true;
    }
    return false;
}

bool LSound::setLooping(bool looping)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_looping( m_data->sound(), looping ? MA_TRUE : MA_FALSE );
        return true;
    }
    return false;
}

bool LSound::isLooping() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_is_looping( m_data->sound() ) == MA_TRUE;
    }
    return false;
}

bool LSound::setVolume(float volume)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_volume( m_data->sound(), volume );
        return true;
    }
    return false;
}

float LSound::getVolume() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_get_volume( m_data->sound() );
    }
    return 0.0f;
}

bool LSound::setPan(float pan)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_pan( m_data->sound(), pan );
        return true;
    }
    return false;
}

float LSound::getPan() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_get_pan( m_data->sound() );
    }
    return 0.0f;
}

bool LSound::setPitch(float pitch)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_pitch( m_data->sound(), pitch );
        return true;
    }
    return false;
}

float LSound::getPitch() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_get_pitch( m_data->sound() );
    }
    return 0.0f;
}

bool LSound::setPosition(float x, float y, float z)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_position( m_data->sound(), x, y, z );
        return true;
    }
    return false;
}

bool LSound::getPosition(float &x, float &y, float &z) const
{
    if(m_data != nullptr && m_data->isInit()){
        ma_vec3f pos = ma_sound_get_position( m_data->sound() );
        x = pos.x;
        y = pos.y;
        z = pos.z;
        return true;
    }
    return false;
}

bool LSound::setDirection(float x, float y, float z)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_direction( m_data->sound(), x, y, z );
        return true;
    }
    return false;
}

bool LSound::getDirection(float &x, float &y, float &z) const
{
    if(m_data != nullptr && m_data->isInit()){
        ma_vec3f pos = ma_sound_get_direction( m_data->sound() );
        x = pos.x;
        y = pos.y;
        z = pos.z;
        return true;
    }
    return false;
}

bool LSound::setVelocity(float x, float y, float z)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_velocity( m_data->sound(), x, y, z );
        return true;
    }
    return false;
}

bool LSound::getVelocity(float &x, float &y, float &z) const
{
    if(m_data != nullptr && m_data->isInit()){
        ma_vec3f pos = ma_sound_get_velocity( m_data->sound() );
        x = pos.x;
        y = pos.y;
        z = pos.z;
        return true;
    }
    return false;
}

bool LSound::setCone(float innerAngle, float outerAngle, float outerGain)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_cone( m_data->sound(), innerAngle, outerAngle, outerGain);
        return true;
    }
    return false;
}

bool LSound::getCone(float &innerAngle, float &outerAngle, float &outerGain) const
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_get_cone( m_data->sound(), &innerAngle, &outerAngle, &outerGain);
        return true;
    }
    return false;
}

bool LSound::setAttenuationModel(AttenuationModel attenuation)
{
    if(m_data != nullptr && m_data->isInit()){
        switch ( attenuation )
        {
        case AttenuationModel::None:{
            ma_sound_set_attenuation_model( m_data->sound(), ma_attenuation_model_none );
            break;
        }
        case AttenuationModel::Inverse:{
            ma_sound_set_attenuation_model( m_data->sound(), ma_attenuation_model_inverse );
            break;
        }
        case AttenuationModel::Linear:{
            ma_sound_set_attenuation_model( m_data->sound(), ma_attenuation_model_linear );
            break;
        }
        case AttenuationModel::Exponential:{
            ma_sound_set_attenuation_model( m_data->sound(), ma_attenuation_model_exponential );
            break;
        }
        default:{
            return false;
        }
        }
        return true;
    }
    return false;
}

LSound::AttenuationModel LSound::getAttenuationModel() const
{
    if(m_data != nullptr && m_data->isInit()){
        switch ( ma_sound_get_attenuation_model( m_data->sound() ) )
        {
        case ma_attenuation_model_none:{
            return AttenuationModel::None;
        }
        case ma_attenuation_model_inverse:{
            return AttenuationModel::Inverse;
        }
        case ma_attenuation_model_linear:{
            return AttenuationModel::Linear;
        }
        case ma_attenuation_model_exponential:{
            return AttenuationModel::Exponential;
        }
        default:{
            break;
        }
        }
    }
    return AttenuationModel::None;
}

bool LSound::setRollOff(float rollOff)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_rolloff( m_data->sound(), rollOff );
        return true;
    }
    return false;
}

float LSound::getRollOff() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_get_rolloff( m_data->sound() );
    }
    return 0.0f;
}

bool LSound::setMinGain(float minGain)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_min_gain( m_data->sound(), minGain );
        return true;
    }
    return false;
}

float LSound::getMinGain() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_get_min_gain( m_data->sound() );
    }
    return 0.0f;
}

bool LSound::setMaxGain(float maxGain)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_max_gain( m_data->sound(), maxGain );
        return true;
    }
    return false;
}

float LSound::getMaxGain() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_get_max_gain( m_data->sound() );
    }
    return 0.0f;
}

bool LSound::setMinDistance(float minDistance)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_min_distance( m_data->sound(), minDistance );
        return true;
    }
    return false;
}

float LSound::getMinDistance() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_get_min_distance( m_data->sound() );
    }
    return 0.0f;
}

bool LSound::setMaxDistance(float maxDistance)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_max_distance( m_data->sound(), maxDistance );
        return true;
    }
    return false;
}

float LSound::getMaxDistance() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_get_max_distance( m_data->sound() );
    }
    return 0.0f;
}

bool LSound::setDopplerFactor(float dopplerFactor)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_doppler_factor( m_data->sound(), dopplerFactor );
        return true;
    }
    return false;
}

float LSound::getDopplerFactor() const
{
    if(m_data != nullptr && m_data->isInit()){
        return ma_sound_get_doppler_factor( m_data->sound() );
    }
    return 0.0f;
}

bool LSound::setFade(float endVolume, uint64_t milliseconds)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_fade_in_milliseconds( m_data->sound(), -1.0f, endVolume, milliseconds );
        return true;
    }
    return false;
}

bool LSound::setStartTime(uint64_t milliseconds)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_start_time_in_milliseconds( m_data->sound(), milliseconds );
        return true;
    }
    return false;
}

bool LSound::setStopTime(uint64_t milliseconds)
{
    if(m_data != nullptr && m_data->isInit()){
        ma_sound_set_stop_time_in_milliseconds( m_data->sound(), milliseconds );
        return true;
    }
    return false;
}

const LSound::Type LSound::type() const
{
    if(m_data != nullptr && m_data->isInit()){
        return (Type)m_data->type();
    }
    return Type::Music;
}

const std::string &LSound::path() const
{
    if(m_data != nullptr && m_data->isInit()){
        return m_data->path();
    }
    return LStringUtil::empty;
}

bool LSound::setListener(uint8_t listenerId)
{
    if(m_data != nullptr && m_data->isInit()){
        std::shared_ptr<LAudioEngineData> ed = LAudioEngineData::engineData(m_data->type());
        if(ed != nullptr){
            if(listenerId < ed->maxListenerCount()){
                ma_sound_set_pinned_listener_index( m_data->sound(), listenerId );
                return true;
            }
        }
    }
    return false;
}

int LSound::maxListenerCount(Type t)
{
    std::shared_ptr<LAudioEngineData> ed = LAudioEngineData::engineData((uint8_t)t);
    if(ed != nullptr){
        return ed->maxListenerCount();
    }
    return 0;
}

bool LSound::setListenerPosition(Type t, uint8_t listenerId, float x, float y, float z)
{
    std::shared_ptr<LAudioEngineData> ed = LAudioEngineData::engineData((uint8_t)t);
    if(ed != nullptr){
        if(listenerId < ed->maxListenerCount()){
            ma_engine_listener_set_position(ed->engine(), listenerId, x, y, z );
            return true;
        }
    }
    return false;
}

bool LSound::getListenerPosition(Type t, uint8_t listenerId, float& x, float& y, float& z)
{
    std::shared_ptr<LAudioEngineData> ed = LAudioEngineData::engineData((uint8_t)t);
    if(ed != nullptr){
        if(listenerId < ed->maxListenerCount()){
            ma_vec3f pos = ma_engine_listener_get_position(ed->engine(), listenerId);
            x = pos.x;
            y = pos.y;
            z = pos.z;
            return true;
        }
    }
    return false;
}

bool LSound::setListenerDirection(Type t, uint8_t listenerId, float x, float y, float z)
{
    std::shared_ptr<LAudioEngineData> ed = LAudioEngineData::engineData((uint8_t)t);
    if(ed != nullptr){
        if(listenerId < ed->maxListenerCount()){
            ma_engine_listener_set_direction(ed->engine(), listenerId, x, y, z );
            return true;
        }
    }
    return false;
}

bool LSound::getListenerDirection(Type t, uint8_t listenerId, float& x, float& y, float& z)
{
    std::shared_ptr<LAudioEngineData> ed = LAudioEngineData::engineData((uint8_t)t);
    if(ed != nullptr){
        if(listenerId < ed->maxListenerCount()){
            ma_vec3f pos = ma_engine_listener_get_direction(ed->engine(), listenerId);
            x = pos.x;
            y = pos.y;
            z = pos.z;
            return true;
        }
    }
    return false;
}

bool LSound::setListenerWorldUp(Type t, uint8_t listenerId, float x, float y, float z)
{
    std::shared_ptr<LAudioEngineData> ed = LAudioEngineData::engineData((uint8_t)t);
    if(ed != nullptr){
        if(listenerId < ed->maxListenerCount()){
            ma_engine_listener_set_world_up(ed->engine(), listenerId, x, y, z );
            return true;
        }
    }
    return false;
}

bool LSound::getListenerWorldUp(Type t, uint8_t listenerId, float& x, float& y, float& z)
{
    std::shared_ptr<LAudioEngineData> ed = LAudioEngineData::engineData((uint8_t)t);
    if(ed != nullptr){
        if(listenerId < ed->maxListenerCount()){
            ma_vec3f pos = ma_engine_listener_get_world_up(ed->engine(), listenerId);
            x = pos.x;
            y = pos.y;
            z = pos.z;
            return true;
        }
    }
    return false;
}

bool LSound::setListenerCone(Type t, uint8_t listenerId, float innerAngle, float outerAngle, float outerGain)
{
    std::shared_ptr<LAudioEngineData> ed = LAudioEngineData::engineData((uint8_t)t);
    if(ed != nullptr){
        if(listenerId < ed->maxListenerCount()){
            ma_engine_listener_set_cone(ed->engine(), listenerId, innerAngle, outerAngle, outerGain);
            return true;
        }
    }
    return false;
}

bool LSound::getListenerCone(Type t, uint8_t listenerId, float &innerAngle, float &outerAngle, float &outerGain)
{
    std::shared_ptr<LAudioEngineData> ed = LAudioEngineData::engineData((uint8_t)t);
    if(ed != nullptr){
        if(listenerId < ed->maxListenerCount()){
            ma_engine_listener_get_cone( ed->engine(), listenerId, &innerAngle, &outerAngle, &outerGain );
            return true;
        }
    }
    return false;
}

LK_NAMESPACE_END
