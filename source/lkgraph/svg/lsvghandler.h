#ifndef LSVGHANDLER_H
#define LSVGHANDLER_H

#include "lkgui/text/lcssparser.h"
#include "lkgui/paint/lpen.h"
#include "lkgraph/svg/lsvgcsshandler.h"
#include "lkcore/io/lxml.h"
#include "lkgraph/svg/lsvggraphics.h"
LK_NAMESPACE_BEGIN


class LSvgDocument;
class LSvgHandler;
class LSvgStyleSelector;
class LSvgStyleProperty;

struct LSvgCssAttribute
{
    std::string name;
    std::string value;
};

class LKSVGDEF LSvgHandler
{
public:
    LSvgHandler(const std::string &content, LFlags<LSvgOption> options = {},  LSvgAnimatorType type = LSvgAnimatorType::Automatic);
    ~LSvgHandler();

    std::shared_ptr<LSvgDocument> document() const;

    void setDefaultCoordinateSystem(LSvgLengthType type);
    LSvgLengthType defaultCoordinateSystem() const;

    void pushColor(const LColor &color);
    void pushColorCopy();
    void popColor();
    LColor currentColor() const;


    void setInStyle(bool b);
    bool inStyle() const;

    std::shared_ptr<LSvgStyleSelector> selector() const;
    LSvgCssHandler &cssHandler();

    inline bool ok() const {
        return m_doc != nullptr && m_xmlRetSegment;
    }

    std::string error() const{
        return std::string(m_xmlRetSegment.error());
    }


    void setAnimPeriod(int64_t start, int64_t end);
    int64_t animationDuration() const;

    void parseCSStoXMLAttrs(const std::string &css, std::list<LSvgCssAttribute> *attributes);

    inline std::shared_ptr<LPen> defaultPen() const
    { return m_defaultPen; }

    LFlags<LSvgOption> options() const;
    LSvgAnimatorType animatorType() const;
    bool trustedSourceMode() const;

public:
    bool startElement(const std::string &localName, LXmlDoc::const_iterator xmlIter);
    bool endElement(std::string_view localName);
    bool characters(std::string_view str);
    bool processingInstruction(const std::string &target, LXmlDoc::const_iterator xmlIter);

private:
    void init();

    LXmlDoc m_xmlTree;
    LXmlSegmentIter m_xmlRetSegment;
    std::shared_ptr<LSvgDocument> m_doc;
    std::stack<std::shared_ptr<LSvgNode> > m_nodes;
    // TODO: This is only needed during parsing, so it unnecessarily takes up space after that.
    // Temporary container for :
    // - <use> nodes which haven't been resolved yet.
    // - <filter> nodes to be checked for unsupported filter primitives.
    std::list<std::shared_ptr<LSvgNode> > m_toBeResolved;

    enum CurrentNode
    {
        Unknown,
        Graphics,
        Style,
        Doc
    };
    std::stack<CurrentNode> m_skipNodes;

    /*!
        Follows the depths of elements. The top is current xml:space
        value that applies for a given element.
     */
    std::stack<LSvgText::WhitespaceMode> m_whitespaceMode;

    std::shared_ptr<LSvgStyleProperty> m_style;

    LSvgLengthType m_defaultCoords;

    std::stack<LColor> m_colorStack;
    std::stack<int>    m_colorTagCount;

    int64_t m_animEnd;

    bool m_inStyle;

    std::shared_ptr<LSvgStyleSelector> m_selector;
    LSvgCssHandler m_cssHandler;

    void parse();
    void resolvePaintServers(std::shared_ptr<LSvgNode> node, int nestedDepth = 0);
    void resolveNodes();

    std::shared_ptr<LPen> m_defaultPen;

    const LFlags<LSvgOption> m_options;
    const LSvgAnimatorType m_animatorType;
};



LK_NAMESPACE_END
#endif // LSVGHANDLER_H
