#ifndef LSIZEPOLICY_H
#define LSIZEPOLICY_H

#include "lkgui/lguienums.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LSizePolicy
{
public:
    enum PolicyFlag {
        GrowFlag = 1,
        ExpandFlag = 2,
        ShrinkFlag = 4,
        IgnoreFlag = 8
    };


    enum Policy {
        Fixed = 0,
        Minimum = GrowFlag,
        Maximum = ShrinkFlag,
        Preferred = GrowFlag | ShrinkFlag,
        MinimumExpanding = GrowFlag | ExpandFlag,
        Expanding = GrowFlag | ShrinkFlag | ExpandFlag,
        Ignored = ShrinkFlag | GrowFlag | IgnoreFlag
    };


    enum ControlType {
        DefaultType      = 0x00000001,
        CheckBox         = 0x00000002,
        ComboBox         = 0x00000004,
        Frame            = 0x00000008,
        GroupBox         = 0x00000010,
        Label            = 0x00000020,
        Edit             = 0x00000040,
        LineEdit         = 0x00000080,
        PushButton       = 0x00000100,
        RadioButton      = 0x00000200,
        Slider           = 0x00000400,
        SpinBox          = 0x00000800,
        Menu             = 0x00001000,
        MenuBar          = 0x00002000,
        TabBar           = 0x00004000,
    };

    LSizePolicy(uint32_t data = 0);
    LSizePolicy(Policy horizontal, Policy vertical, ControlType type = DefaultType);

    LFlags<LOrientation> expandingDirections() const;
    ControlType controlType() const noexcept;
    void setControlType(ControlType type) noexcept;

    inline void setHasHeightForWidth(bool b) noexcept { m_hfw = b;  }
    inline bool hasHeightForWidth() const noexcept { return m_hfw; }
    inline void setHasWidthForHeight(bool b) noexcept { m_wfh = b;  }
    inline bool hasWidthForHeight() const noexcept { return m_wfh; }

    inline bool operator==(const LSizePolicy& s) const noexcept { return m_data == s.m_data; }
    inline bool operator!=(const LSizePolicy& s) const noexcept { return m_data != s.m_data; }

    LSizePolicy transposed() const;

    union {
        struct{
            uint32_t m_horStretch : 8;
            uint32_t m_verStretch : 8;
            uint32_t m_horPolicy : 4;
            uint32_t m_verPolicy : 4;
            uint32_t m_ctype : 5;
            uint32_t m_hfw : 1;
            uint32_t m_wfh : 1;
            uint32_t m_retainSizeWhenHidden : 1;
        };
        uint32_t m_data = 0;
    };
};


LK_NAMESPACE_END

#endif // LSIZEPOLICY_H
