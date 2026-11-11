#include "lsvgabstractanimation.h"

LK_NAMESPACE_BEGIN


LSvgAbstractAnimation::LSvgAbstractAnimation()
    : m_start(0)
    , m_duration(0)
    , m_finished(false)
    , m_iterationCount(0)
{

}


void LSvgAbstractAnimation::appendProperty(std::shared_ptr<LSvgAbstractAnimatedProperty> property)
{
    m_properties.push_back(property);
}

bool LSvgAbstractAnimation::finished() const
{
    return m_finished;
}

bool LSvgAbstractAnimation::isActive() const
{
    return !finished();
}

void LSvgAbstractAnimation::evaluateAnimation(int64_t elapsedTime)
{
    double fractionOfTotalTime = 0;
    if (m_duration != 0 && elapsedTime >= m_start) {
        fractionOfTotalTime = (elapsedTime - m_start) / m_duration;
        if (m_iterationCount >= 0 && m_iterationCount < fractionOfTotalTime) {
            m_finished = true;
            return;
        }
    }

    double fractionOfCurrentIterationTime = fractionOfTotalTime - std::trunc(fractionOfTotalTime);

    for (std::shared_ptr<LSvgAbstractAnimatedProperty> animProperty : m_properties) {
        const std::list<double>& keyFrames = animProperty->keyFrames();
        std::list<double>::const_iterator iter = keyFrames.cbegin();
        if(iter != keyFrames.cend()){
            double from = *iter;
            ++iter;
            int i= 1;
            while (iter != keyFrames.cend()) {
                double to = *iter;
                if (fractionOfCurrentIterationTime >= from && fractionOfCurrentIterationTime < to) {
                    double currFraction = (fractionOfCurrentIterationTime - from) / (to - from);
                    animProperty->interpolate(i, currFraction);
                }
                from = to;
                ++iter;
                ++i;
            }
        }

    }
}

void LSvgAbstractAnimation::setRunningTime(int64_t startMs, int64_t durationMs)
{
    m_start = (startMs > 0) ? startMs : 0;
    m_duration = (durationMs > 0) ? durationMs : 0;
}

int64_t LSvgAbstractAnimation::start() const
{
    return m_start;
}

int64_t LSvgAbstractAnimation::duration() const
{
    return m_duration;
}

void LSvgAbstractAnimation::setIterationCount(int64_t count)
{
    m_iterationCount = count;
}

int64_t LSvgAbstractAnimation::iterationCount() const
{
    return m_iterationCount;
}

LSvgCssAnimation::LSvgCssAnimation()
{

}

LSvgAbstractAnimation::AnimationType LSvgCssAnimation::animationType() const
{
    return AnimationType::CSS;
}

LK_NAMESPACE_END


