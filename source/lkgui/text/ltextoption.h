#ifndef LTEXTOPTION_H
#define LTEXTOPTION_H

#include "lkgui/lguienums.h"
#include "lkcore/lglobal.h"
#include <list>

LK_NAMESPACE_BEGIN

class LKGUIDEF LTextOption
{
public:
    LTextOption();
    LTextOption(const LTextOption &o);
    LTextOption &operator=(const LTextOption &o);

    enum TabType {
        LeftTab,
        RightTab,
        CenterTab,
        DelimiterTab
    };

    enum WrapMode {
        NoWrap,
        WordWrap,
        ManualWrap,
        WrapAnywhere,
        WrapAtWordBoundaryOrAnywhere
    };

    enum Flag {
        ShowTabsAndSpaces = 0x1,
        ShowLineAndParagraphSeparators = 0x2,
        AddSpaceForLineAndParagraphSeparators = 0x4,
        SuppressColors = 0x8,
        ShowDocumentTerminator = 0x10,
        IncludeTrailingSpaces = 0x80000000
    };


    struct Tab {
        inline Tab() : m_position(80), m_type(LTextOption::LeftTab) { }
        inline Tab(double pos, TabType tabType, wchar_t delim = 0)
            : m_position(pos), m_type(tabType), m_delimiter(delim) {}

        inline bool operator==(const Tab &other) const {
            return m_type == other.m_type
                   && lFuzzyCompare(m_position, other.m_position)
                   && m_delimiter == other.m_delimiter;
        }

        inline bool operator!=(const Tab &other) const {
            return !operator==(other);
        }

        double m_position;
        TabType m_type;
        wchar_t m_delimiter;
    };


    void setTabArray(const std::list<double> &tabStops);
    std::list<double> tabArray() const;

    uint32_t m_align : 8;
    uint32_t m_wordWrap : 4;
    uint32_t m_design : 1;
    uint32_t m_direction : 2;
    uint32_t m_flags;
    double m_tabStopDistance;

    std::list<Tab> m_tabStops;
};


LK_NAMESPACE_END

#endif // LTEXTOPTION_H
