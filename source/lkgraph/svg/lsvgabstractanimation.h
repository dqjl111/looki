#ifndef LSVGABSTRACTANIMATION_H
#define LSVGABSTRACTANIMATION_H

#include "lkgraph/svg/lsvgtanimatedproperty.h"


LK_NAMESPACE_BEGIN

class LKSVGDEF LSvgAbstractAnimation
{
public:
    enum AnimationType
    {
        CSS,
        SMIL,
    };

    LSvgAbstractAnimation();

    virtual AnimationType animationType() const = 0;
    void evaluateAnimation(int64_t elapsedTime);

    void setRunningTime(int64_t startMs, int64_t durationMs);
    int64_t start() const;
    int64_t duration() const;

    void setIterationCount(int64_t count);
    int64_t iterationCount() const;

    virtual void appendProperty(std::shared_ptr<LSvgAbstractAnimatedProperty> property);
    inline const std::list<std::shared_ptr<LSvgAbstractAnimatedProperty>>& properties() const{
        return m_properties;
    }

    bool finished() const;
    virtual bool isActive() const;

protected:
    int64_t m_start;
    int64_t m_duration;
    bool m_finished;
    int64_t m_iterationCount;
    std::list<std::shared_ptr<LSvgAbstractAnimatedProperty>> m_properties;
};

class LKSVGDEF LSvgCssAnimation : public LSvgAbstractAnimation
{
public:
    LSvgCssAnimation();
    virtual AnimationType animationType() const override;
};

LK_NAMESPACE_END

#endif // LSVGABSTRACTANIMATION_H
