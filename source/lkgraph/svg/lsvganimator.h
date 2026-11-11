#ifndef LSVGANIMATOR_H
#define LSVGANIMATOR_H

#include "lkgraph/svg/lsvgabstractanimation.h"
#include "lkgraph/svg/lsvgnode.h"

LK_NAMESPACE_BEGIN

class LKSVGDEF LSvgAbstractAnimator
{
public:
    LSvgAbstractAnimator();
    virtual ~LSvgAbstractAnimator();

    void appendAnimation(const std::shared_ptr<LSvgNode> node, std::shared_ptr<LSvgAbstractAnimation> anim);
    std::list<std::shared_ptr<LSvgAbstractAnimation> > animationsForNode(const std::shared_ptr<LSvgNode> node) const;

    void advanceAnimations();
    virtual void restartAnimation() = 0;
    virtual int64_t currentElapsed() = 0;
    virtual void setAnimatorTime(int64_t time) = 0;

    void setAnimationDuration(int64_t dur);
    int64_t animationDuration() const;

protected:
    int64_t m_time;
    int64_t m_animationDuration;

private:
    std::list<std::shared_ptr<LSvgAbstractAnimation> > combinedAnimationsForNode(const std::shared_ptr<LSvgNode> node) const;

private:
    using AnimationMap = std::unordered_map<LSvgNode*, std::list<std::shared_ptr<LSvgAbstractAnimation> >>;
    AnimationMap m_animationsSMIL;
    AnimationMap m_animationsCSS;
};

class LKSVGDEF LSvgAnimator : public LSvgAbstractAnimator
{
public:
    LSvgAnimator();
    ~LSvgAnimator();

    virtual void restartAnimation() override;
    virtual int64_t currentElapsed() override;
    virtual void setAnimatorTime(int64_t time) override;
};

class LKSVGDEF LSvgAnimationController : public LSvgAbstractAnimator
{
public:
    LSvgAnimationController();
    ~LSvgAnimationController();

    virtual void restartAnimation() override;
    virtual int64_t currentElapsed() override;
    virtual void setAnimatorTime(int64_t time) override;
};

LK_NAMESPACE_END

#endif // LSVGANIMATOR_H
