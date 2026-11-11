#ifndef LSVGSTRUCTURE_H
#define LSVGSTRUCTURE_H
#include "lkgraph/svg/lsvgnode.h"

LK_NAMESPACE_BEGIN

class LKSVGDEF LSvgStructure : public LSvgNode
{
public:
    LSvgStructure(std::shared_ptr<LSvgNode> parent);
    std::shared_ptr<LSvgNode> scopeNode(const std::string &id) const;
    void addChild(std::shared_ptr<LSvgNode>child, const std::string &id);
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const override;
    std::shared_ptr<LSvgNode> previousSiblingNode(std::shared_ptr<LSvgNode> n) const;
    const std::list<std::shared_ptr<LSvgNode>>& renderers() const { return m_renderers; }
protected:
    std::list<std::shared_ptr<LSvgNode>>          m_renderers;
    mutable bool              m_recursing = false;
};

class LKSVGDEF LSvgG : public LSvgStructure
{
public:
    LSvgG(std::shared_ptr<LSvgNode> parent);
    void drawCommand(LPainter *, LSvgExtraStates &) override;
    bool shouldDrawNode(LPainter *p, LSvgExtraStates &states) const override;
    Type type() const override;
    bool requiresGroupRendering() const override;
};

class LKSVGDEF LSvgDefs : public LSvgStructure
{
public:
    LSvgDefs(std::shared_ptr<LSvgNode>parent);
    void drawCommand(LPainter *, LSvgExtraStates &) override {}
    bool shouldDrawNode(LPainter *p, LSvgExtraStates &states) const override;
    Type type() const override;
};


class LKSVGDEF LSvgSymbolLike : public LSvgStructure
{
    // Marker, Symbol and potentially other elements share a lot of common
    // attributes and functionality. By making a common base class we can
    // avoid repetition.
public:
    enum class Overflow : uint8_t {
        Visible,
        Hidden,
        Scroll = Visible, //Will not support scrolling
        Auto = Visible
    };


    LSvgSymbolLike(std::shared_ptr<LSvgNode>parent, LRect<double> bounds, LRect<double> viewBox, LPoint<double> refP,
                   LFlags<LSvgPreserveAspectRatio> pAspectRatios, LSvgSymbolLike::Overflow overflow);
    void drawCommand(LPainter *, LSvgExtraStates &) override {}
    LRect<double> decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const override;
    bool requiresGroupRendering() const override;
protected:
    void setPainterToRectAndAdjustment(LPainter *p) const;
protected:
    LRect<double> m_rect;
    LRect<double> m_viewBox;
    LPoint<double> m_refP;
    LFlags<LSvgPreserveAspectRatio> m_pAspectRatios;
    Overflow m_overflow;
};


class LKSVGDEF LSvgSymbol : public LSvgSymbolLike
{
public:
    LSvgSymbol(std::shared_ptr<LSvgNode>parent, LRect<double> bounds, LRect<double> viewBox, LPoint<double> refP,
               LFlags<LSvgPreserveAspectRatio> pAspectRatios, LSvgSymbolLike::Overflow overflow);
    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;
};

class LKSVGDEF LSvgMarker : public LSvgSymbolLike
{
public:
    enum class Orientation : uint8_t {
        Auto,
        AutoStartReverse,
        Value
    };
    enum class MarkerUnits : uint8_t {
        StrokeWidth,
        UserSpaceOnUse
    };

    LSvgMarker(std::shared_ptr<LSvgNode> parent, LRect<double> bounds, LRect<double> viewBox, LPoint<double> refP,
               LFlags<LSvgPreserveAspectRatio> pAspectRatios, LSvgSymbolLike::Overflow overflow,
               Orientation orientation, double orientationAngle, MarkerUnits markerUnits);
    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    static void drawMarkersForNode(std::shared_ptr<LSvgNode>node, LPainter *p, LSvgExtraStates &states);
    static LRect<double> markersBoundsForNode(const std::shared_ptr<LSvgNode> node, LPainter *p, LSvgExtraStates &states);

    Orientation orientation() const {
        return m_orientation;
    }
    double orientationAngle() const {
        return m_orientationAngle;
    }
    MarkerUnits markerUnits() const {
        return m_markerUnits;
    }
    Type type() const override;

private:
    static void drawHelper(const std::shared_ptr<LSvgNode>node, LPainter *p,
                           LSvgExtraStates &states, LRect<double> *boundingRect = nullptr);

    Orientation m_orientation;
    double m_orientationAngle;
    MarkerUnits m_markerUnits;
};

class LKSVGDEF LSvgFilterContainer : public LSvgStructure
{
public:

    LSvgFilterContainer(std::shared_ptr<LSvgNode> parent, const LSvgRectInfo &bounds, LSvgUnitTypes filterUnits, LSvgUnitTypes primitiveUnits);
    void drawCommand(LPainter *, LSvgExtraStates &) override {}
    bool shouldDrawNode(LPainter *, LSvgExtraStates &) const override;
    Type type() const override;
    std::shared_ptr<LImage>  applyFilter(const std::shared_ptr<LImage>  &buffer, LPainter *p, const LRect<double> &bounds) const;
    void setSupported(bool supported);
    bool supported() const;
    LRect<double> filterRegion(const LRect<double> &itemBounds) const;
private:
    LSvgRectInfo m_rectInfo;
    LSvgUnitTypes m_filterUnits;
    LSvgUnitTypes m_primitiveUnits;
    bool m_supported;
};


class LKSVGDEF LSvgSwitch : public LSvgStructure
{
public:
    LSvgSwitch(std::shared_ptr<LSvgNode> parent);
    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;

    std::shared_ptr<LSvgNode>childToRender() const;

private:
    void initSvg();
private:
    std::string m_systemLanguage;
    std::string m_systemLanguagePrefix;
};

class LKSVGDEF LSvgMask : public LSvgStructure
{
public:
    LSvgMask(std::shared_ptr<LSvgNode> parent, const LSvgRectInfo& bounds,  LSvgUnitTypes contentsUnits);
    void drawCommand(LPainter *, LSvgExtraStates &) override {}
    bool shouldDrawNode(LPainter *, LSvgExtraStates &) const override;
    Type type() const override;
    std::shared_ptr<LImage>  createMask(LPainter *p, LSvgExtraStates &states, std::shared_ptr<LSvgNode> targetNode, LRect<double> *globalRect) const;
    std::shared_ptr<LImage>  createMask(LPainter *p, LSvgExtraStates &states, const LRect<double> &localRect, LRect<double> *globalRect) const;

    const LSvgRectInfo& rectInfo() const
    {
        return m_rectInfo;
    }

    LSvgUnitTypes contentUnits() const
    {
        return m_contentUnits;
    }

private:
    LSvgRectInfo m_rectInfo;
    LSvgUnitTypes m_contentUnits;
};

class LKSVGDEF LSvgPattern : public LSvgStructure
{
public:
    LSvgPattern(std::shared_ptr<LSvgNode> parent, const LSvgRectInfo& bounds, const LRect<double>& viewBox,
                LSvgUnitTypes contentUnits, const LTransform2D& transform);
    void drawCommand(LPainter *, LSvgExtraStates &) override {}
    bool shouldDrawNode(LPainter *, LSvgExtraStates &) const override;
    std::shared_ptr<LImage>  patternImage(LPainter *p, LSvgExtraStates &states, const std::shared_ptr<LSvgNode> patternElement);
    Type type() const override;
    const LTransform2D& appliedTransform() const { return m_appliedTransform; }

private:
    std::shared_ptr<LImage>  renderPattern(LSize<int> size, double contentScaleX, double contentScaleY);
    void calculateAppliedTransform(LTransform2D& worldTransform, const LRect<double>& peLocalBB, const LSize<int>& imageSize);

private:
    LTransform2D m_appliedTransform;
    LSvgRectInfo m_rectInfo;
    LRect<double> m_viewBox;
    LSvgUnitTypes m_contentUnits;
    LTransform2D m_transform;
};

LK_NAMESPACE_END

#endif // LSVGSTRUCTURE_H
