#include "lsvganimatenode.h"

LK_NAMESPACE_BEGIN

LSvgAnimateNode::LSvgAnimateNode(std::shared_ptr<LSvgNode> parent)
    : LSvgNode(parent)
    , m_end(0)
    , m_fill(Fill::Freeze)
    , m_additive(Additive::Replace)
{
}

LSvgAnimateNode::~LSvgAnimateNode()
{

}

void LSvgAnimateNode::setLinkId(const std::string &link)
{
    m_linkId = link;
}

const std::string &LSvgAnimateNode::linkId() const
{
    return m_linkId;
}

LSvgAbstractAnimation::AnimationType LSvgAnimateNode::animationType() const
{
    return AnimationType::SMIL;
}

bool LSvgAnimateNode::isActive() const
{
    return !finished() || m_fill == Fill::Freeze;
}

void LSvgAnimateNode::setRunningTime(int startMs, int durMs, int endMs, int by)
{
    m_start = startMs;
    m_end = endMs;
    m_duration = durMs;
}

void LSvgAnimateNode::setRepeatCount(int repeatCount)
{
    setIterationCount(repeatCount);
}

void LSvgAnimateNode::setFill(Fill fill)
{
    m_fill = fill;
}

LSvgAnimateNode::Fill LSvgAnimateNode::fill() const
{
    return m_fill;
}

void LSvgAnimateNode::setAdditiveType(Additive additive)
{
    m_additive = additive;
}

LSvgAnimateNode::Additive LSvgAnimateNode::additiveType() const
{
    return m_additive;
}

void LSvgAnimateNode::drawCommand(LPainter *p, LSvgExtraStates &states)
{

}

bool LSvgAnimateNode::shouldDrawNode(LPainter *p, LSvgExtraStates &states) const
{

    return false;
}

LSvgAnimateColor::LSvgAnimateColor(std::shared_ptr<LSvgNode> parent)
    : LSvgAnimateNode(parent)
{
}

LSvgNode::Type LSvgAnimateColor::type() const
{
    return LSvgNode::AnimateColor;
}

LSvgAnimateTransform::LSvgAnimateTransform(std::shared_ptr<LSvgNode> parent)
    : LSvgAnimateNode(parent)
{
}

LSvgNode::Type LSvgAnimateTransform::type() const
{
    return Type::AnimateTransform;
}
LK_NAMESPACE_END
