#ifndef LTEXTBOUNDARYFINDER_H
#define LTEXTBOUNDARYFINDER_H
#include "lkcore/text/lchar.h"
#include "lkcore/lflags.h"
#include <vector>

LK_NAMESPACE_BEGIN

enum class LTextBoundaryReason : uint16_t{
    NotAtBoundary = 0,
    BreakOpportunity = 0x1f,
    StartOfItem = 0x20,
    EndOfItem = 0x40,
    MandatoryBreak = 0x80,
    SoftHyphen = 0x100
};
L_EN_FLAGS(LTextBoundaryReason)

class LKCOREDEF LTextBoundaryFinder
{
public:
    enum BoundaryType {
        Grapheme,
        Word,
        Sentence,
        Line
    };

    LTextBoundaryFinder();
    LTextBoundaryFinder(BoundaryType type, const std::wstring &string);
    virtual ~LTextBoundaryFinder();

    inline bool isValid() const { return attributes.size() == length; }

    inline BoundaryType type() const { return t; }
    std::wstring string() const;

    void toStart();
    void toEnd();
    int position() const;
    void setPosition(int position);

    int toNextBoundary();
    int toPreviousBoundary();

    bool isAtBoundary() const;
    LFlags<LTextBoundaryReason> boundaryReasons() const;

private:

    BoundaryType t;
    std::wstring s;
    const wchar_t *chars;
    int length;
    int pos;
    uint8_t freePrivate : 1;
    std::vector<LCharAttributes> attributes;
};

LK_NAMESPACE_END

#endif // LTEXTBOUNDARYFINDER_H
