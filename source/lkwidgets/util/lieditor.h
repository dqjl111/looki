#ifndef LTEDITOR_H
#define LTEDITOR_H
#include <concepts>
#include <iterator>
#include "lkwidgets/util/leditcontainer.h"
#include "lkwidgets/util/ledittext.h"
#include "lkgui/lguievent.h"
#include "lkcore/lsignal.h"

LK_NAMESPACE_BEGIN

struct LIndexItem
{
    std::shared_ptr<LEditBase> baseItem = nullptr;
    int textPos=-1;
    int validLevel=-1;
};

class LKWIDGETSDEF LIEditor
{
public:
    LIEditor();

    virtual bool isAcceptableInput(const std::shared_ptr<LKeyEvent> event) const;
    static bool isCommonTextEditShortcut(const std::shared_ptr<LKeyEvent> ke);

    virtual LCbor toCbor() const;
    virtual bool fromCbor(const LCbor &cbor);

    virtual std::shared_ptr<LEditBase> editBase() const = 0;

    virtual uint32_t job() const = 0;
    virtual std::vector<int> activeIndexes() const;
    virtual bool setActiveIndexes(const std::vector<int>& indexes);

    virtual LIndexItem indexItem(const std::vector<int>& indexes) const;
    virtual std::vector<int> selectStartIndexes() const;
    virtual std::vector<int> selectEndIndexes() const;
    virtual std::vector<int> selectItemIndexes() const;
    virtual void doSelectRange(const std::vector<int>& start, const std::vector<int>& end) const;

    virtual std::list<std::shared_ptr<LEditOp>> doDelete(bool isForward = false);
    virtual std::list<std::shared_ptr<LEditOp>> doInput(const std::wstring& text);
    virtual std::list<std::shared_ptr<LEditOp>> doInputCbor(const LCbor& cbor);
    virtual std::list<std::shared_ptr<LEditOp>> doEnterNewItem();

    virtual bool doMovePosition(std::shared_ptr<LKeyEvent> e);
    virtual bool doMovePosition(LMoveOper op, LMoveMode mode);
    virtual bool doMouseClick(const LPoint<double>& p);
    virtual bool doMouseRelease(const LPoint<double>& p);
    virtual bool doMouseSelectMove(const LPoint<double>& p);

    virtual void doDraw( LPainter *painter,
                      const LPoint<double> &pos = LPoint<double>(),
                      int flags = (int)(LEditBase::DrawFlags::All),
                      const LRect<double> &clip = LRect<double>());
    virtual void update(bool isViewUpdate, bool isCaretUpdate);
    virtual uint32_t contentLength() const;
    virtual LRect<double> cursorRectForIndexPos(const std::vector<int>& pos) const;
    virtual LRect<double> cursorRect() const;
    virtual LRect<double> anchorRect() const;

    virtual void clearContent();
    virtual void setContent(const std::wstring& text);
    virtual void setContentCbor(const LCbor& cbor);
    virtual std::wstring content() const;
    virtual LCbor contentCbor() const;

    virtual std::wstring selectContent() const;
    virtual LCbor selectContentCbor() const;
    virtual bool hasSelectContent() const;
    virtual int contentRowNum() const;

    inline bool isContentEmpty() const{
        return contentLength() == 0;
    }

    inline bool hasContent() const{
        return contentLength() > 0;
    }

    virtual std::shared_ptr<LEditDoc> doc() const{
        return m_doc.lock();
    }
    virtual void setDoc(std::shared_ptr<LEditDoc> d){
        m_doc = d;
    }

    virtual void calculateCaret();
    virtual bool checkUpdate();

    inline void setPalette(std::shared_ptr<LPalette> pal){
        m_palette = pal;
    }

    std::shared_ptr<LPalette> palette();

    inline bool isViewUpdate(){
        return m_isViewUpdate;
    }

    inline bool isCaretUpdate(){
        return m_isCaretUpdate;
    }


    const std::shared_ptr<LFont> font() const;
    inline void setFont(std::shared_ptr<LFont> f){
        m_font = f;
    }

    inline bool isCursorVisible() const{
        return m_hideCursor == false;
    }

    inline void setCursorVisible(bool v){
        m_hideCursor = (v == false);
    }

    inline bool blinkStatus() const{
        return m_blinkStatus;
    }

    inline bool hasScriptFontSize() const{
        return m_hasScriptFontSize;
    }

    inline void setHasScriptFontSize(bool v){
        m_hasScriptFontSize = v;
    }

    void blink();
    void resetBlink();
    inline int maxLength(){
        return m_maxLength;
    }
    void setMaxLength(int len);

    uint8_t m_cursorWidth = 2;

    LSignal<> sigUpdate;
protected:
    virtual bool isInputable(const wchar_t c) const;
    virtual void updateMaxLength(){}
    bool doDelete(const std::vector<int>& indexes);
    virtual std::list<std::shared_ptr<LEditOp>> doInputItem(std::shared_ptr<LEditBase> item);

    std::weak_ptr<LEditDoc> m_doc;
    std::shared_ptr<LPalette> m_palette = nullptr;
    std::shared_ptr<LFont> m_font = nullptr;
    int m_maxLength = -1;

    union{
        uint8_t m_editFlags = 0;
        struct{
            uint8_t m_isViewUpdate : 1;
            uint8_t m_isCaretUpdate: 1;
            uint8_t m_hideCursor : 1; // used to hide the m_cursor inside preedit areas
            uint8_t m_blinkStatus : 1;
            uint8_t m_delayBlink : 1;
            uint8_t m_hasScriptFontSize: 1;
        };
    };
};

template<typename T>
concept TEditBase = std::derived_from<T, LEditBase>;

template<TEditBase T>
class LTEditor : public T, public LIEditor
{
public:
    LTEditor(){}

    virtual std::shared_ptr<LEditBase> editBase() const override{
        return LEditBase::getEditT<LEditBase>();
    }

    virtual std::shared_ptr<LIEditor> editor() const override{
        return LEditBase::getEditT<LIEditor>();
    }


    virtual uint32_t type() const override{
        return uint32_t(LEditType::Editor);
    }
};



LK_NAMESPACE_END

#endif // LTEDITOR_H
