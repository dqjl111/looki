#ifndef LCURSOR_H
#define LCURSOR_H
#include "lkgui/lpoint.h"
#include "lkgui/lguienums.h"
#include "lkgui/image/lpixmap.h"

LK_NAMESPACE_BEGIN

class LScreen;
class LKGUIDEF LCursor
{
public:
    LCursor();
    LCursor(LCursorShape shape);
    LCursor(const LCursor& c);

    static LPoint<int> pos();
    static LPoint<int> pos(const std::shared_ptr<LScreen> screen);
    static void setPos(int x, int y);
    static void setPos(std::shared_ptr<LScreen> screen, int x, int y);
    inline static void setPos(const LPoint<int> &p) { setPos(p.m_x, p.m_y); }
    inline static void setPos(std::shared_ptr<LScreen> screen, const LPoint<int> &p) { setPos(screen, p.m_x, p.m_y); }

    LCursorShape shape() const;
    void setShape(LCursorShape newShape);

    inline void reset(){
        m_flags = 0;
    }

    inline LCursor& operator=(const LCursor &b){
        m_flags = b.m_flags;
        return *this;
    }

    inline bool isValid() const{
        return m_isValid;
    }

    inline void setIsValid(bool isValid){
        m_isValid = isValid;
    }

    inline bool operator==(const LCursor &b) const{
        return m_flags == b.m_flags;
    }
    inline bool operator!=(const LCursor &b) const{
        return m_flags != b.m_flags;
    }
protected:
    union{
        uint8_t m_flags = 0;
        struct{
            LCursorShape m_cshape: 7;
            uint8_t m_isValid: 1;
        };

    };
};


LK_NAMESPACE_END
#endif // LCURSOR_H
