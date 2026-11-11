#ifndef LSVGNODE_H
#define LSVGNODE_H
#include "lkgraph/svg/lsvghelper.h"
#include "lkgraph/svg/lsvgstyle.h"
#include "lkgui/paint/lpainter.h"

LK_NAMESPACE_BEGIN
class LKSVGDEF LSvgNode: public LIVar
{
public:
    enum Type
    {
        Doc,
        Group,
        Defs,
        Switch,
        AnimateColor,
        AnimateTransform,
        Circle,
        Ellipse,
        Image,
        Line,
        Path,
        Polygon,
        Polyline,
        Rect,
        Text,
        Textarea,
        Tspan,
        Use,
        Video,
        Mask,
        Symbol,
        Marker,
        Pattern,
        Filter,
        FeMerge,
        FeMergenode,
        FeColormatrix,
        FeGaussianblur,
        FeOffset,
        FeComposite,
        FeFlood,
        FeBlend,
        FeUnsupported
    };
    enum DisplayMode {
        InlineMode,
        BlockMode,
        ListItemMode,
        RunInMode,
        CompactMode,
        MarkerMode,
        TableMode,
        InlineTableMode,
        TableRowGroupMode,
        TableHeaderGroupMode,
        TableFooterGroupMode,
        TableRowMode,
        TableColumnGroupMode,
        TableColumnMode,
        TableCellMode,
        TableCaptionMode,
        NoneMode,
        InheritMode
    };

public:
    LSvgNode(std::shared_ptr<LSvgNode> parent= nullptr);
    virtual ~LSvgNode();
    void draw(LPainter *p, LSvgExtraStates &states);
    virtual bool separateFillStroke(const LSvgExtraStates &) const {return false;}
    virtual void drawCommand(LPainter *p, LSvgExtraStates &states) = 0;
    void fillThenStroke(LPainter *p, LSvgExtraStates &states);
    std::shared_ptr<LImage>  drawIntoBuffer(LPainter *p, LSvgExtraStates &states, const LRect<int> &boundsRect);
    void applyMaskToBuffer(std::shared_ptr<LImage>  proxy, std::shared_ptr<LImage>  mask) const;
    void drawWithMask(LPainter *p, LSvgExtraStates &states, const std::shared_ptr<LImage>  mask, const LRect<int> &boundsRect);
    void applyBufferToCanvas(LPainter *p, std::shared_ptr<LImage>  proxy) const;

    std::shared_ptr<LSvgNode> parent() const;
    bool isDescendantOf(const std::shared_ptr<LSvgNode> parent) const;

    void appendStyleProperty(std::shared_ptr<LSvgStyleProperty> prop, const std::string &id);
    void applyStyle(LPainter *p, LSvgExtraStates &states) const;
    void applyStyleRecursive(LPainter *p, LSvgExtraStates &states) const;
    void revertStyle(LPainter *p, LSvgExtraStates &states) const;
    void revertStyleRecursive(LPainter *p, LSvgExtraStates &states) const;
    void applyAnimatedStyle(LPainter *p, LSvgExtraStates &states) const;
    void revertAnimatedStyle(LPainter *p, LSvgExtraStates &states) const;

    std::shared_ptr<LSvgStyleProperty> styleProperty(LSvgStyleProperty::Type type) const;
    std::shared_ptr<LSvgPaintStyleProperty> styleProperty(const std::string &id) const;

    std::shared_ptr<LSvgDocument> document() const;

    virtual Type type() const = 0;
    std::string typeName() const;
    virtual LRect<double> internalFastBounds(LPainter *p, LSvgExtraStates &states) const;
    virtual LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states) const;
    LRect<double> bounds(LPainter *p, LSvgExtraStates &states) const;
    LRect<double> bounds() const;
    virtual LRect<double> decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const;
    virtual LRect<double> decoratedBounds(LPainter *p, LSvgExtraStates &states) const;

    void setRequiredFeatures(const std::list<std::string> &lst);
    const std::list<std::string> & requiredFeatures() const;

    void setRequiredExtensions(const std::list<std::string> &lst);
    const std::list<std::string> & requiredExtensions() const;

    void setRequiredLanguages(const std::list<std::string> &lst);
    const std::list<std::string> & requiredLanguages() const;

    void setRequiredFormats(const std::list<std::string> &lst);
    const std::list<std::string> & requiredFormats() const;

    void setRequiredFonts(const std::list<std::string> &lst);
    const std::list<std::string> & requiredFonts() const;

    void setVisible(bool visible);
    bool isVisible() const;

    void setDisplayMode(DisplayMode display);
    DisplayMode displayMode() const;

    std::string nodeId() const;
    void setNodeId(const std::string &i);

    std::string xmlClass() const;
    void setXmlClass(const std::string &str);

    std::string maskId() const;
    void setMaskId(const std::string &str);
    bool hasMask() const;

    std::string filterId() const;
    void setFilterId(const std::string &str);
    bool hasFilter() const;

    std::string markerStartId() const;
    void setMarkerStartId(const std::string &str);
    bool hasMarkerStart() const;

    std::string markerMidId() const;
    void setMarkerMidId(const std::string &str);
    bool hasMarkerMid() const;

    std::string markerEndId() const;
    void setMarkerEndId(const std::string &str);
    bool hasMarkerEnd() const;

    bool hasAnyMarker() const;

    virtual bool requiresGroupRendering() const;

    virtual bool shouldDrawNode(LPainter *p, LSvgExtraStates &states) const;
    const LSvgStaticStyle &style() const { return m_style; }
protected:
    mutable LSvgStaticStyle m_style;
    mutable LSvgAnimatedStyle m_animatedStyle;

    LRect<double> filterRegion(LRect<double> bounds) const;

    static double strokeWidth(LPainter *p);
    static void initPainter(LPainter *p);

    enum BoundsMode {
        Simplistic,
        IncludeMiterLimit
    };
    static LRect<double> boundsOnStroke(LPainter *p, const std::shared_ptr<LPainterPath> path, double width, BoundsMode mode);

private:
    std::weak_ptr<LSvgNode> m_parent;

    std::list<std::string> m_requiredFeatures;
    std::list<std::string> m_requiredExtensions;
    std::list<std::string> m_requiredLanguages;
    std::list<std::string> m_requiredFormats;
    std::list<std::string> m_requiredFonts;

    bool        m_visible;

    std::string m_id;
    std::string m_class;
    std::string m_maskId;
    std::string m_filterId;
    std::string m_markerStartId;
    std::string m_markerMidId;
    std::string m_markerEndId;


    DisplayMode m_displayMode;
    mutable LRect<double> m_cachedBounds;

    friend class LSvgDocument;
};

inline std::shared_ptr<LSvgNode> LSvgNode::parent() const
{
    return m_parent.lock();
}

inline bool LSvgNode::isVisible() const
{
    return m_visible;
}

inline std::string LSvgNode::nodeId() const
{
    return m_id;
}

inline std::string LSvgNode::xmlClass() const
{
    return m_class;
}


LK_NAMESPACE_END

#endif
