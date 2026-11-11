#ifndef LFONTMETRICS_H
#define LFONTMETRICS_H
#include "lkgui/text/lfont.h"
#include "lkgui/lrect.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LFontMetrics
{
public:
    LFontMetrics(const std::shared_ptr<LFont> font = nullptr);

    double horizontalAdvance(const std::wstring& , int len = -1) const;
    double horizontalAdvance(wchar_t) const;
    double averageCharWidth() const;

    double ascent() const;
    double capHeight() const;
    double descent() const;
    double height() const;
    double leading() const;
    double lineSpacing() const;
    double minLeftBearing() const;
    double minRightBearing() const;
    double maxWidth() const;

    LRect<int> boundingRect(const LRect<int> &r, uint32_t flags,
                            const std::wstring &text, int tabstops = 0, int *tabarray = nullptr) const;
    inline LRect<int> boundingRect(int x, int y, int w, int h, uint32_t flags,
                                   const std::wstring &text, int tabstops = 0, int *tabarray = nullptr) const
    { return boundingRect(LRect<int>(x, y, w, h, false), flags, text, tabstops, tabarray); }

    LRect<int> boundingRect(wchar_t ch) const;
    LSize<int> size(int flags, const std::wstring& str, int tabstops = 0, int *tabarray = nullptr) const;

    std::wstring elidedText(const std::wstring &text, LTextElide mode, double width, int flags = 0) const;
protected:
    const std::shared_ptr<LFont> m_font = nullptr;
};


LK_NAMESPACE_END
#endif // LFONTMETRICS_H
