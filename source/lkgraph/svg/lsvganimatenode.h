#ifndef LSVGANIMATENODE_H
#define LSVGANIMATENODE_H

#include "lkgraph/svg/lsvgabstractanimation.h"
#include "lkgraph/svg/lsvgnode.h"

LK_NAMESPACE_BEGIN

class LKSVGDEF LSvgAnimateNode : public LSvgNode, public LSvgAbstractAnimation
{
public:
    enum Additive
    {
        Sum,
        Replace
    };
    enum Fill
    {
        Freeze,
        Remove
    };

public:
    LSvgAnimateNode(std::shared_ptr<LSvgNode> parent = nullptr);
    virtual ~LSvgAnimateNode();

    void setLinkId(const std::string &link);
    const std::string &linkId() const;

    virtual AnimationType animationType() const override;
    virtual bool isActive() const override;

    void setRunningTime(int startMs, int durMs, int endMs, int by);
    void setRepeatCount(int repeatCount);

    void setFill(Fill fill);
    Fill fill() const;

    void setAdditiveType(Additive additive = Additive::Replace);
    Additive additiveType() const;

    virtual void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    virtual bool shouldDrawNode(LPainter *p, LSvgExtraStates &states) const override;

protected:
    double m_end;
    Fill m_fill;
    Additive m_additive;
    std::string m_linkId;
};

class LKSVGDEF LSvgAnimateColor : public LSvgAnimateNode
{
public:
    LSvgAnimateColor(std::shared_ptr<LSvgNode> parent = nullptr);
    virtual Type type() const override;
};

class LKSVGDEF LSvgAnimateTransform : public LSvgAnimateNode
{
public:
    LSvgAnimateTransform(std::shared_ptr<LSvgNode> parent = nullptr);
    virtual Type type() const override;
};

LK_NAMESPACE_END

#endif // LSVGANIMATENODE_H
