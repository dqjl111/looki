#ifndef LABSTRACTLINEEDIT_H
#define LABSTRACTLINEEDIT_H

#include "lkwidgets/util/ltextlineeditor.h"
#include "lkwidgets/util/llineeditor.h"
#include "lkwidgets/util/lundostack.h"
#include "lkwidgets/widget/lframe.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LAbstractLineEdit : public LFrame
{
public:
    LAbstractLineEdit(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);

    int maxLength() const;
    void setMaxLength(int);

    void setTextMargins(int left, int top, int right, int bottom);
    void setTextMargins(const LMargins<int> &margins);

    inline void setCursorWidth(uint8_t w){
        m_editor->m_cursorWidth = w;
    }

    inline uint8_t cursorWidth(){
        return m_editor->m_cursorWidth;
    }

    inline bool isReadOnly() const{
        return m_isReadOnly;
    }

    void setIsReadOnly(bool v);

    inline bool isCursorVisible() const{
        return m_editor->isCursorVisible();
    }

    void setCursorVisible(bool v);

    inline bool isShowPlaceholderText() const{
        return m_isShowPlaceholderText;
    }

    inline void setIsShowPlaceholderText(bool v){
        m_isShowPlaceholderText = v? 1: 0;
    }

    inline const std::wstring& placeholderText() const{
        return m_placeholderText;
    }

    inline void setPlaceholderText(const std::wstring& s){
        m_placeholderText = s;
    }

    LRect<int> cursorRect() const;

    virtual void blink() override;

    void deselect();
    void selectAll();
    void copy();
    void cut();
    void paste();

    void undo();
    void redo();
    inline void del(){
        del(true);
    }
    void del(bool isForward);
    void input(const std::wstring& text);

    void enableUndoStack(std::shared_ptr<LUndoStack> stack = nullptr);
    void disableUndoStack();
    bool hasUndoStack() const;
    void clearUndoStack();
    virtual LVariant inputMethodQuery(LInputMethodQuery, const LVariant& args = LVariant()) const override;
protected:
    virtual void mousePressEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseMoveEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseDoubleClickEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void keyPressEvent(std::shared_ptr<LKeyEvent> ev) override;
    virtual void keyReleaseEvent(std::shared_ptr<LKeyEvent> ev) override;
    virtual void focusInEvent(std::shared_ptr<LFocusEvent> ev) override;
    virtual void focusOutEvent(std::shared_ptr<LFocusEvent> ev) override;

    virtual void inputMethodEvent(std::shared_ptr<LInputMethodEvent> ev) override;
    virtual void contextMenuEvent(std::shared_ptr<LContextMenuEvent> e) override;
    virtual std::shared_ptr<LMenu> createContextMenu();
    virtual void initSpecific() override;

    virtual LRect<int> editRect() const = 0;
    virtual bool checkOpResult();

    void paintEditLine(LPainter& p, std::shared_ptr<LPalette> pal);
    LRect<int> adjustedEditRect(const LRect<double> &r) const;
    LPoint<double> scrollEditPoint(const LPoint<double>& p) const;
    bool cursorMoveKeyEvent(std::shared_ptr<LKeyEvent> e);

    std::shared_ptr<LEditText> editText() const;

    union{
        uint8_t m_editFlags = 0;
        struct{
            uint8_t m_isReadOnly : 1;
            uint8_t m_isShowPlaceholderText : 1;
            uint8_t m_mousePressed: 1;
        };
    };

    uint16_t m_align = (LAlignment::Left | LAlignment::VCenter | LAlignment::ExpandTabs);

    std::wstring m_placeholderText;
    std::shared_ptr<LIEditor> m_editor = nullptr;
    std::shared_ptr<LUndoStack> m_undoStack;

    double m_hscroll = 0;
    double m_vscroll = 0;
};


LK_NAMESPACE_END
#endif // LABSTRACTLINEEDIT_H
