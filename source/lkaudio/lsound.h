#ifndef LSOUND_H
#define LSOUND_H

#include "lkcore/livar.h"
#include <filesystem>

LK_NAMESPACE_BEGIN
class LSoundData;
class LAudioListener;
class LKAUDIODEF LSound
{
public:
    enum class Type
    {
        Sound,               ///< A short sound effect.
        Music,               ///< A longer sound like background music.
        Background = Music,  ///< An alias for Music.
        Stream     = Music,  ///< An alias for Music.
    };

    enum class AttenuationModel
    {
        None,         ///< No distance attenuation and no spatialization.
        Inverse,      ///< Equivalent to OpenAL's AL_INVERSE_DISTANCE_CLAMPED.
        Linear,       ///< Linear attenuation. Equivalent to OpenAL's AL_LINEAR_DISTANCE_CLAMPED.
        Exponential,  ///< Exponential attenuation. Equivalent to OpenAL's AL_EXPONENT_DISTANCE_CLAMPED.
    };

    LSound();
    LSound(const std::filesystem::path& filePath, Type type = Type::Stream);

    bool isInit();

    bool load( const std::filesystem::path& filePath, Type type = Type::Stream);
    bool play();
    bool stop();
    bool pause();
    bool replay();

    bool isPlaying() const;
    bool isEnd() const;

    float getDurationInSeconds() const;
    float getCursorInSeconds() const;
    bool seek( uint64_t milliseconds = 0ull );

    bool setLooping( bool looping );
    bool isLooping() const;
    //音量
    bool setVolume( float volume );
    float getVolume() const;

    //左右声道平衡
    bool setPan( float pan );
    float getPan() const;

    //引擎音高
    bool setPitch( float pitch );
    float getPitch() const;

    //立体声位置
    bool setPosition(float x, float y, float z);
    bool getPosition(float& x, float& y, float& z) const;

    //立体声方向
    bool setDirection(float x, float y, float z);
    bool getDirection(float& x, float& y, float& z) const;

    //立体声速度，多普勒效应
    bool setVelocity(float x, float y, float z);
    bool getVelocity(float& x, float& y, float& z) const;

    //立体声锥形角度和衰减
    bool setCone( float innerAngle, float outerAngle, float outerGain );
    bool getCone(float& innerAngle, float& outerAngle, float& outerGain) const;

    //立体声衰减模型
    bool setAttenuationModel( AttenuationModel attenuation );
    AttenuationModel getAttenuationModel() const;

    //距离衰减因子
    bool setRollOff( float rollOff );
    float getRollOff() const;

    //最小增益
    bool setMinGain( float minGain );
    float getMinGain() const;

    //最大增益
    bool setMaxGain( float maxGain );
    float getMaxGain() const;

    //最小距离
    bool setMinDistance( float minDistance );
    float getMinDistance() const;

    //最大距离
    bool setMaxDistance( float maxDistance );
    float getMaxDistance() const;

    //多普乐效应因子
    bool setDopplerFactor( float dopplerFactor );
    float getDopplerFactor() const;

    //淡去
    bool setFade( float endVolume, uint64_t milliseconds );
    bool setStartTime( uint64_t milliseconds );
    bool setStopTime( uint64_t milliseconds );

    const Type type() const;
    const std::string& path() const;

    bool setListener(uint8_t listenerId);

    static int maxListenerCount(Type t);

    static bool setListenerPosition(Type t, uint8_t listenerId, float x, float y, float z);
    static bool getListenerPosition(Type t, uint8_t listenerId, float& x, float& y, float& z);

    static bool setListenerDirection(Type t, uint8_t listenerId, float x, float y, float z);
    static bool getListenerDirection(Type t, uint8_t listenerId, float& x, float& y, float& z);

    static bool setListenerWorldUp(Type t, uint8_t listenerId, float x, float y, float z);
    static bool getListenerWorldUp(Type t, uint8_t listenerId, float& x, float& y, float& z);

    static bool setListenerCone(Type t, uint8_t listenerId, float innerAngle, float outerAngle, float outerGain );
    static bool getListenerCone(Type t, uint8_t listenerId, float& innerAngle, float& outerAngle, float& outerGain);

private:
    std::shared_ptr<LSoundData> m_data;
};


LK_NAMESPACE_END

#endif // LSOUND_H
