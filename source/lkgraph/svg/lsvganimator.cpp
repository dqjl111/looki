#include "lsvganimator.h"
#include <list>
#include <unordered_map>

LK_NAMESPACE_BEGIN

LSvgAbstractAnimator::LSvgAbstractAnimator()
    : m_time(0)
    , m_animationDuration(0)
{
}

LSvgAbstractAnimator::~LSvgAbstractAnimator()
{
}


void LSvgAbstractAnimator::appendAnimation(const std::shared_ptr<LSvgNode> node, std::shared_ptr<LSvgAbstractAnimation> anim)
{
    if (node == nullptr){
        return;
    }

    if (anim->animationType() == LSvgAbstractAnimation::SMIL){
        AnimationMap::iterator iter = m_animationsSMIL.find(node.get());
        if(iter != m_animationsSMIL.end()){
            iter->second.push_back(anim);
        }else{
            std::list<std::shared_ptr<LSvgAbstractAnimation>> t;
            t.push_back(anim);
            m_animationsSMIL[node.get()] = t;
        }
    }else{
        AnimationMap::iterator iter = m_animationsCSS.find(node.get());
        if(iter != m_animationsCSS.end()){
            iter->second.push_back(anim);
        }else{
            std::list<std::shared_ptr<LSvgAbstractAnimation>> t;
            t.push_back(anim);
            m_animationsCSS[node.get()] = t;
        }
    }
}

std::list<std::shared_ptr<LSvgAbstractAnimation> > LSvgAbstractAnimator::animationsForNode(const std::shared_ptr<LSvgNode> node) const
{
    return combinedAnimationsForNode(node);
}

void LSvgAbstractAnimator::advanceAnimations()
{
    int64_t elapsedTime = currentElapsed();
    for (auto itr = m_animationsCSS.begin(); itr != m_animationsCSS.end(); itr++) {
        std::list<std::shared_ptr<LSvgAbstractAnimation> > &nodeAnimations = itr->second;
        for (std::shared_ptr<LSvgAbstractAnimation> anim : nodeAnimations) {
            if (!anim->finished()){
                anim->evaluateAnimation(elapsedTime);
            }
        }
    }

    for (auto itr = m_animationsSMIL.begin(); itr != m_animationsSMIL.end(); itr++) {
        std::list<std::shared_ptr<LSvgAbstractAnimation> > &nodeAnimations = itr->second;
        for (std::shared_ptr<LSvgAbstractAnimation> anim : nodeAnimations) {
            if (!anim->finished()){
                anim->evaluateAnimation(elapsedTime);
            }
        }
    }
}

void LSvgAbstractAnimator::setAnimationDuration(int64_t dur)
{
    m_animationDuration = dur;
}

int64_t LSvgAbstractAnimator::animationDuration() const
{
    return m_animationDuration;
}

std::list<std::shared_ptr<LSvgAbstractAnimation> > LSvgAbstractAnimator::combinedAnimationsForNode(const std::shared_ptr<LSvgNode> node) const
{
    if (node == nullptr){
        return std::list<std::shared_ptr<LSvgAbstractAnimation> >();
    }

    std::list<std::shared_ptr<LSvgAbstractAnimation>> t;
    AnimationMap::const_iterator iter = m_animationsSMIL.find(node.get());
    if(iter != m_animationsSMIL.cend()){
        t = iter->second;
    }

    iter = m_animationsCSS.find(node.get());
    if(iter != m_animationsCSS.cend()){
        t.insert(t.end(), iter->second.cbegin(), iter->second.cend());
    }
    return t;
}

LSvgAnimator::LSvgAnimator()
{
}

LSvgAnimator::~LSvgAnimator()
{
}

void LSvgAnimator::restartAnimation()
{
    m_time = std::chrono::duration_cast<std::chrono::milliseconds>(LSysClock::now().time_since_epoch()).count();
}

int64_t LSvgAnimator::currentElapsed()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(LSysClock::now().time_since_epoch()).count() - m_time;
}

void LSvgAnimator::setAnimatorTime(int64_t time)
{
    m_time += time;
}

LSvgAnimationController::LSvgAnimationController()
{
}

LSvgAnimationController::~LSvgAnimationController()
{
}

void LSvgAnimationController::restartAnimation()
{
    m_time = 0;
}

int64_t LSvgAnimationController::currentElapsed()
{
    return m_time;
}

void LSvgAnimationController::setAnimatorTime(int64_t time)
{
    m_time = std::max<int64_t>(0, time);
}



LK_NAMESPACE_END
