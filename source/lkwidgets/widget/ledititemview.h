#ifndef LEDITITEMVIEW_H
#define LEDITITEMVIEW_H

#include "lkwidgets/widget/labstractscrollarea.h"
#include "lkwidgets/util/lundostack.h"
#include "lkwidgets/util/lieditor.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LEditItemView : public LFrame
{
public:
     _LIVarTagEx(LEditItemView, LWidget)

    LEditItemView(std::shared_ptr<LWidget> parent = nullptr);
    LEditItemView(std::shared_ptr<LIEditor> editor, std::shared_ptr<LWidget> parent = nullptr);
    enum ScrollMode {
        ScrollPerItem = 0,
        ScrollPerPixel = 1
    };

    inline bool isReadOnly() const{
        return m_isReadOnly;
    }

    void setIsReadOnly(bool v);

    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSizeHint() const override;
    inline std::shared_ptr<LIEditor> editor() const{
        return m_editor;
    }

    void setEditor(std::shared_ptr<LIEditor> e);
    virtual LPoint<int> contentsOffset() const;

    inline bool isCursorVisible() const{
        return m_editor->isCursorVisible();
    }

    void setCursorVisible(bool v);

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
    void inputCbor(const LCbor& cbor);

    int maxLength() const;
    void setMaxLength(int);

    void enableUndoStack(std::shared_ptr<LUndoStack> stack = nullptr);
    void disableUndoStack();
    bool hasUndoStack() const;
    void clearUndoStack();

    std::wstring text(std::wstring lineSeperator = L"\n") const;
    void setText(std::wstring& text);
    std::wstring selectText(std::wstring lineSeperator = L"\n") const;

    void updateScrollBars(bool force = false);
    void updateEditPosition();

    virtual LVariant inputMethodQuery(LInputMethodQuery, const LVariant& args = LVariant()) const override;
protected:
    virtual void initSpecific() override;
    virtual void paintEvent(std::shared_ptr<LPaintEvent> ev) override;
    virtual void mousePressEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseMoveEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseDoubleClickEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void keyPressEvent(std::shared_ptr<LKeyEvent> ev) override;
    virtual void keyReleaseEvent(std::shared_ptr<LKeyEvent> ev) override;
    virtual void inputMethodEvent(std::shared_ptr<LInputMethodEvent> ev) override;
    virtual void focusInEvent(std::shared_ptr<LFocusEvent> ev) override;
    virtual void focusOutEvent(std::shared_ptr<LFocusEvent> ev) override;
    virtual void contextMenuEvent(std::shared_ptr<LContextMenuEvent> e) override;
    virtual std::shared_ptr<LMenu> createContextMenu();

    virtual bool checkOpResult();

    std::shared_ptr<LEditText> editText() const;

    bool cursorMoveKeyEvent(std::shared_ptr<LKeyEvent> ev);
    LRect<int> adjustedControlRect(const LRect<double> &r) const;
    LPoint<double> scrollEditPoint(const LPoint<double>& p) const;
    void rebuildScrollBars();

    std::shared_ptr<LIEditor> m_editor;
    std::shared_ptr<LUndoStack> m_undoStack;

    union{
        uint8_t m_editFlags = 0;
        struct{
            uint8_t m_isReadOnly : 1;
            uint8_t m_mousePressed: 1;
        };
    };
};

LK_NAMESPACE_END

#endif // LEDITITEMVIEW_H
