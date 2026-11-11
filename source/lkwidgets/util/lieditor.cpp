#include "lieditor.h"
#include "lkgui/lguiapplication.h"
#include "leditdoc.h"

LK_NAMESPACE_BEGIN

LIEditor::LIEditor()
{
    m_isViewUpdate = 1;
    m_isCaretUpdate = 1;
}


bool LIEditor::isInputable(const wchar_t c) const
{
    return true;
}


bool LIEditor::isAcceptableInput(const std::shared_ptr<LKeyEvent> event) const
{
    const std::wstring& text = event->text();
    if (text.empty()){
        return false;
    }

    const wchar_t c = text.at(0);

    // Formatting characters such as ZWNJ, ZWJ, RLM, etc. This needs to go before the
    // next test, since CTRL+SHIFT is sometimes used to input it on Windows.
    if (LStringUtil::category(c) == lchar::Other_Format){
        return true;
    }

    // ignore Ctrl/Ctrl+Shift; accept only AltGr (Alt+Ctrl) on German keyboards
    if (event->modifiers() == LKeyModifier::Ctrl
        || event->modifiers() == (LKeyModifier::Shift | LKeyModifier::Ctrl)) {
        return false;
    }

    if (LStringUtil::isPrint(c)){
        return true;
    }

    if (LStringUtil::category(c) == lchar::Other_PrivateUse){
        return true;
    }
    return isInputable(c);
}

bool LIEditor::isCommonTextEditShortcut(const std::shared_ptr<LKeyEvent> ke)
{
    if (ke->modifiers() == LKeyModifier::No
        || ke->modifiers() == LKeyModifier::Shift
        || ke->modifiers() == LKeyModifier::Keypad) {
        if (ke->key() < (uint32_t)LKey::Escape) {
            return true;
        } else {
            switch (ke->key()) {
            case (uint32_t)LKey::Return:
            case (uint32_t)LKey::Enter:
            case (uint32_t)LKey::Delete:
            case (uint32_t)LKey::Home:
            case (uint32_t)LKey::End:
            case (uint32_t)LKey::Backspace:
            case (uint32_t)LKey::Left:
            case (uint32_t)LKey::Right:
            case (uint32_t)LKey::Up:
            case (uint32_t)LKey::Down:
            case (uint32_t)LKey::Tab:{
                return true;
            }
            default:{
                break;
            }
            }
        }
    } else if (ke->matches(LKeySequence::Copy)
               || ke->matches(LKeySequence::Paste)
               || ke->matches(LKeySequence::Cut)
               || ke->matches(LKeySequence::Redo)
               || ke->matches(LKeySequence::Undo)
               || ke->matches(LKeySequence::MoveToNextWord)
               || ke->matches(LKeySequence::MoveToPreviousWord)
               || ke->matches(LKeySequence::MoveToStartOfDocument)
               || ke->matches(LKeySequence::MoveToEndOfDocument)
               || ke->matches(LKeySequence::SelectNextWord)
               || ke->matches(LKeySequence::SelectPreviousWord)
               || ke->matches(LKeySequence::SelectStartOfLine)
               || ke->matches(LKeySequence::SelectEndOfLine)
               || ke->matches(LKeySequence::SelectStartOfBlock)
               || ke->matches(LKeySequence::SelectEndOfBlock)
               || ke->matches(LKeySequence::SelectStartOfDocument)
               || ke->matches(LKeySequence::SelectEndOfDocument)
               || ke->matches(LKeySequence::SelectAll)
               ) {
        return true;
    }
    return false;
}

LCbor LIEditor::toCbor() const
{
    std::shared_ptr<LEditBase> c = editBase();
    LCbor::map cborMap = c->toCbor(false);
    cborMap[int(LEditBase::CborId::Job)] = job();
    return cborMap;
}

bool LIEditor::fromCbor(const LCbor &cbor)
{
    std::shared_ptr<LEditBase> c = editBase();
    if(cbor.isMap()){
        LCbor::map cborMap = cbor.toMap();
        return c->fromCbor(cborMap, true, true, doc());
    }
    return false;
}

std::vector<int> LIEditor::activeIndexes() const
{
    std::vector<int> ret;
    std::shared_ptr<LEditBase> b = editBase();
    if(b->isContainer()){
        std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(b);
        b = c->m_activeChild;
        while(b != nullptr){
            if(LEditBase::isStart(b)){
                ret.push_back(-1);
                break;
            }else{
                int index = c->childIndex(b);
                if(index>=0){
                    ret.push_back(index);
                    if(b->isContainer()){
                        c = std::dynamic_pointer_cast<LEditContainer>(b);
                        b = c->m_activeChild;
                    }else if(b->isText()){
                        std::shared_ptr<LEditText> t = std::dynamic_pointer_cast<LEditText>(b);
                        ret.push_back(t->activePos());
                        break;
                    }else{
                        break;
                    }
                }else{
                    break;
                }
            }
        }
    }else if(b->isText()){
        std::shared_ptr<LEditText> t = std::dynamic_pointer_cast<LEditText>(b);
        ret.push_back(t->activePos());
    }
    return ret;
}

bool LIEditor::setActiveIndexes(const std::vector<int> &indexes)
{
    std::shared_ptr<LEditBase> eb = editBase();
    if(eb->isContainer()){
        std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(eb);
        c->m_activeChild = nullptr;
        std::shared_ptr<LEditBase> b = nullptr;

        int index = 0;
        int i=0;
        for(; i<(int)indexes.size(); ++i){
            index = indexes[i];
            if(b!= nullptr){
                b->setActivePos(index);
                return true;
            }else{
                const std::list<std::shared_ptr<LEditBase> >& cc = c->children();
                if((int)cc.size()> index && index>=0){
                    auto it = cc.begin();
                    std::advance(it, index);
                    c->m_activeChild = *it;
                    if(c->m_activeChild->isContainer()){
                        c = std::dynamic_pointer_cast<LEditContainer>(c->m_activeChild);
                        c->setActiveChild(nullptr);
                    }else{
                        b = c->m_activeChild;
                    }
                }else{
                    if(index <0){
                        c->m_activeChild = LEditBase::startBase();
                    }else{
                        c->m_activeChild = nullptr;
                    }
                    return true;
                }
            }
        }
        return (b!=nullptr) || (c!=eb);
    }else{
        if(indexes.size() >0 ){
            eb->setActivePos(indexes[0]);
            return true;
        }else{
            return false;
        }
    }
}

LIndexItem LIEditor::indexItem(const std::vector<int> &indexes) const
{
    LIndexItem ret;
    std::shared_ptr<LEditBase> eb = editBase();
    if(eb->isContainer()){
        std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(eb);
        if(indexes.empty()){
            ret.baseItem = c;
            return ret;
        }

        if(indexes[0]<0){
            ret.baseItem = LEditBase::startBase();
            return ret;
        }

        if(indexes[0] >=(int)c->children().size()){
            return ret;
        }

        std::shared_ptr<LEditBase> t = c;

        int index = 0;
        int i=0;
        for(; i<(int)indexes.size(); ++i){
            ret.validLevel = i;
            index = indexes[i];

            const std::list<std::shared_ptr<LEditBase> >& cc = c->children();
            if((int)cc.size()> index && index>=0){
                auto it = cc.begin();
                std::advance(it, index);
                t = *it;
                if(t->isContainer()){
                    c = std::dynamic_pointer_cast<LEditContainer>(t);
                    ret.baseItem = c;
                }else if(t->isText()){
                    ret.baseItem = t;
                    ++i;
                    if((int)indexes.size() > i){
                        std::shared_ptr<LEditText> b = std::dynamic_pointer_cast<LEditText>(t);
                        ret.validLevel = i;
                        index = indexes[i];
                        if(index>=0 && index<= (int)b->text().size()){
                            ret.textPos = index;
                        }
                    }
                    return ret;
                }else{
                    ret.baseItem = t;
                    return ret;
                }
            }else{
                if(index <0){
                    ret.baseItem = LEditBase::startBase();
                }else{
                    ret.baseItem = nullptr;
                }
                break;
            }
        }
        return ret;
    }else{
        ret.baseItem = eb;
        if(indexes.empty()){
            return ret;
        }

        ret.validLevel = 0;
        if(eb->isText()){
            if((int)indexes.size() > 0){
                std::shared_ptr<LEditText> b = std::dynamic_pointer_cast<LEditText>(eb);
                int index = indexes[0];
                if(index>=0 && index<= (int)b->text().size()){
                    ret.textPos = index;
                }
            }

        }
        return ret;
    }
}

std::vector<int> LIEditor::selectStartIndexes() const
{
    std::vector<int> ret;
    std::shared_ptr<LEditBase> eb = editBase();
    if(eb->isContainer()){
        std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(eb);
        if(c->m_cursorState & LEditBase::PartialSelect){
            int startId = c->partSelectStartId();

            while(c != nullptr){
                const std::list<std::shared_ptr<LEditBase> >& cc = c->children();
                if(startId >=0 && startId< (int)cc.size()){
                    ret.push_back(startId);
                    auto it = cc.begin();
                    std::advance(it, startId);
                    std::shared_ptr<LEditBase> b = *it;

                    if(b->m_cursorState & LEditBase::PartialSelect){
                        if(b->isContainer()){
                            c = std::dynamic_pointer_cast<LEditContainer>(b);
                            startId = c->partSelectStartId();
                        }else{
                            startId = b->partSelectStartId();
                            ret.push_back(startId);
                            break;
                        }
                    }else{
                        break;
                    }
                }else{
                    if(startId<0){
                        ret.push_back(-1);
                    }
                    break;
                }
            }
        }
    }else{
        ret.push_back(eb->partSelectStartId());
    }
    return ret;
}

std::vector<int> LIEditor::selectEndIndexes() const
{
    std::vector<int> ret;
    std::shared_ptr<LEditBase> eb = editBase();
    if(eb->isContainer()){
        std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(eb);
        if(c->m_cursorState & LEditBase::PartialSelect){
            int endId = c->partSelectEndId();

            while(c != nullptr){
                const std::list<std::shared_ptr<LEditBase> >& cc = c->children();
                if(endId >=0 && endId< (int)cc.size()){
                    ret.push_back(endId);
                    auto it = cc.begin();
                    std::advance(it, endId);
                    std::shared_ptr<LEditBase> b = *it;

                    if(b->isContainer()){
                        c = std::dynamic_pointer_cast<LEditContainer>(b);
                        endId = c->partSelectEndId();
                    }else{
                        endId = b->partSelectEndId();
                        ret.push_back(endId);
                        break;
                    }
                }else{
                    if(endId<0){
                        ret.push_back(-1);
                    }
                    break;
                }
            }
        }
    }else{
        ret.push_back(eb->partSelectEndId());
    }
    return ret;
}

std::vector<int> LIEditor::selectItemIndexes() const
{
    std::vector<int> ret;
    std::shared_ptr<LEditBase> eb = editBase();
    if(eb->isContainer()){
        std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(eb);
        if(c->m_cursorState & LEditBase::PartialSelect){
            int startId = c->partSelectStartId();
            int endId = c->partSelectEndId();

            while(c != nullptr){
                if(startId == endId){
                    const std::list<std::shared_ptr<LEditBase> >& cc = c->children();
                    if(startId >=0 && startId<(int)cc.size()){
                        ret.push_back(startId);
                        auto it = cc.begin();
                        std::advance(it, startId);
                        std::shared_ptr<LEditBase> b = *it;

                        if(b->m_cursorState & LEditBase::PartialSelect){
                            if(b->isContainer()){
                                c = std::dynamic_pointer_cast<LEditContainer>(b);
                                startId = c->partSelectStartId();
                                endId = c->partSelectEndId();
                            }else{
                                ret.push_back(b->partSelectStartId());
                                break;
                            }
                        }else{
                            break;
                        }
                    }else{
                        break;
                    }
                }else{
                    break;
                }
            }
        }
    }else{
        ret.push_back(eb->partSelectStartId());
    }
    return ret;
}

void LIEditor::doSelectRange(const std::vector<int> &start, const std::vector<int> &end) const
{
    std::shared_ptr<LEditBase> c = editBase();
    bool isLeftToRight = LEditBase::compareIndexes(start, end) <= 0;
    if(isLeftToRight){
        c->selectRange(start, end, 0, uint8_t(LEditBase::SelectToRight));
    }else{
        c->selectRange(start, end, 0, uint8_t(LEditBase::SelectToLeft));
    }
}

std::list<std::shared_ptr<LEditOp> > LIEditor::doDelete(bool isForward)
{
    std::shared_ptr<LEditBase> c = editBase();
    if(c->m_cursorState == LEditBase::NoSelect){
        c->markSelectStart();
        if(isForward){
            c->moveCursor(LMoveOper::NextWord);
        }else{
            c->moveCursor(LMoveOper::PreviousWord);
        }
        std::vector<int> start = selectStartIndexes();
        std::vector<int> end = activeIndexes();

        doSelectRange(start, end);
    }
    std::list<std::shared_ptr<LEditOp> > ret = c->removeSelection();
    update(true, true);
    return ret;
}

std::list<std::shared_ptr<LEditOp> > LIEditor::doInput(const std::wstring &text)
{
    return std::list<std::shared_ptr<LEditOp> >();
}

std::list<std::shared_ptr<LEditOp> > LIEditor::doInputCbor(const LCbor &cbor)
{
    std::list<std::shared_ptr<LEditOp> > ret;

    if(cbor.isMap()){
        LCbor::map m = cbor.toMap();
        LCbor::map::const_iterator iter = m.find(int(LEditBase::CborId::Type));
        if(iter == m.end()){
            return ret;
        }

        std::shared_ptr<LEditBase> c = editBase();
        if(c->isContainer()){
            const LCbor& cbor = iter->second;
            const uint32_t ttype = (uint32_t)cbor.toUInt64();
            if(ttype == c->type()){
                LCbor::map::const_iterator mapIter = m.find(int(LEditBase::CborId::Children));
                if(mapIter != m.end()){
                    bool hasInput = false;

                    const LCbor& ccbor = mapIter->second;
                    const LCbor::array cborArr = ccbor.toArray();
                    LCbor::array::const_iterator cIter = cborArr.begin();
                    std::shared_ptr<LEditDoc> pDoc = doc();
                    for(; cIter != cborArr.end(); ++cIter){
                        std::shared_ptr<LEditBase> b= LEditDoc::createBase(*cIter, true, pDoc);
                        if(b != nullptr){
                            if(hasInput){
                                ret.append_range(doEnterNewItem());
                            }
                            ret.append_range(doInputItem(b));
                            hasInput = true;
                        }
                    }
                }
            }else{
                std::shared_ptr<LEditDoc> pDoc = doc();
                std::shared_ptr<LEditBase> b= LEditDoc::createBase(cbor, true, pDoc);
                ret.append_range(doInputItem(b));
            }
        }else if(c->isText()){
            std::shared_ptr<LEditDoc> pDoc = doc();
            std::shared_ptr<LEditBase> b= LEditDoc::createBase(cbor, true, pDoc);
            if(b != nullptr){
                if(b->isText()){
                    std::shared_ptr<LEditText> text = std::dynamic_pointer_cast<LEditText>(b);
                    if(text != nullptr){
                        ret.append_range(doInput(text->text()));
                    }
                }
            }else{
                ret.append_range(doInput(LStringUtil::utf8ToWide(cbor.toString())));
            }
        }
    }
    return ret;
}

std::list<std::shared_ptr<LEditOp> > LIEditor::doInputItem(std::shared_ptr<LEditBase> item)
{
    return std::list<std::shared_ptr<LEditOp> >();
}

std::list<std::shared_ptr<LEditOp> > LIEditor::doEnterNewItem()
{
    return std::list<std::shared_ptr<LEditOp> >();
}

bool LIEditor::doMovePosition(std::shared_ptr<LKeyEvent> e)
{
    LMoveMode mode = LMoveMode::MoveAnchor;
    LMoveOper op = LMoveOper::NoMove;
    if (e->matches(LKeySequence::MoveToNextChar)) {
        op = LMoveOper::Right;
    }
    else if (e->matches(LKeySequence::MoveToPreviousChar)) {
        op = LMoveOper::Left;
    }
    else if (e->matches(LKeySequence::SelectNextChar)) {
        op = LMoveOper::Right;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::SelectPreviousChar)) {
        op = LMoveOper::Left;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::SelectNextWord)) {
        op = LMoveOper::WordRight;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::SelectPreviousWord)) {
        op = LMoveOper::WordLeft;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::SelectStartOfLine)) {
        op = LMoveOper::StartOfLine;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::SelectEndOfLine)) {
        op = LMoveOper::EndOfLine;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::SelectStartOfBlock)) {
        op = LMoveOper::StartOfBlock;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::SelectEndOfBlock)) {
        op = LMoveOper::EndOfBlock;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::SelectStartOfDocument)) {
        op = LMoveOper::Start;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::SelectEndOfDocument)) {
        op = LMoveOper::End;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::SelectPreviousLine)) {
        op = LMoveOper::Up;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::SelectNextLine)) {
        op = LMoveOper::Down;
        mode = LMoveMode::KeepAnchor;
    }
    else if (e->matches(LKeySequence::MoveToNextWord)) {
        op = LMoveOper::WordRight;
    }
    else if (e->matches(LKeySequence::MoveToPreviousWord)) {
        op = LMoveOper::WordLeft;
    }
    else if (e->matches(LKeySequence::MoveToEndOfBlock)) {
        op = LMoveOper::EndOfBlock;
    }
    else if (e->matches(LKeySequence::MoveToStartOfBlock)) {
        op = LMoveOper::StartOfBlock;
    }
    else if (e->matches(LKeySequence::MoveToNextLine)) {
        op = LMoveOper::Down;
    }
    else if (e->matches(LKeySequence::MoveToPreviousLine)) {
        op = LMoveOper::Up;
    }
    else if (e->matches(LKeySequence::MoveToStartOfLine)) {
        op = LMoveOper::StartOfLine;
    }
    else if (e->matches(LKeySequence::MoveToEndOfLine)) {
        op = LMoveOper::EndOfLine;
    }
    else if (e->matches(LKeySequence::MoveToStartOfDocument)) {
        op = LMoveOper::Start;
    }
    else if (e->matches(LKeySequence::MoveToEndOfDocument)) {
        op = LMoveOper::End;
    }
    else {
        return false;
    }

    return doMovePosition(op, mode);
}

bool LIEditor::doDelete(const std::vector<int> &indexes)
{
    LIndexItem item = indexItem(indexes);
    if(item.baseItem){
        if((item.validLevel+1) == indexes.size()){
            if(item.textPos>=0){
                std::shared_ptr<LEditText> t = std::dynamic_pointer_cast<LEditText>(item.baseItem);
                if(t != nullptr){
                    bool ret = t->deletePos(item.textPos);
                    update(true, false);;
                    return ret;
                }
            }else{
                std::shared_ptr<LEditContainer> cp = item.baseItem->parent();
                if(cp != nullptr){
                    bool ret = cp->deleteChild(item.baseItem);
                    update(true, false);;
                    return ret;
                }
            }
        }
    }
    return false;
}

void LIEditor::update(bool isViewUpdate, bool isCaretUpdate)
{
    m_isViewUpdate = isViewUpdate;
    m_isCaretUpdate = isCaretUpdate;
    sigUpdate.emt();
}

uint32_t LIEditor::contentLength() const
{
    return 0;
}


LRect<double> LIEditor::cursorRectForIndexPos(const std::vector<int> &pos) const
{
    if(!pos.empty()){
        LIndexItem item = indexItem(pos);
        if(item.baseItem == nullptr){
            std::shared_ptr<LEditBase> eb = editBase();
            double cix = eb->m_rect.x2();
            double ciy = eb->m_rect.m_y;
            int w = m_cursorWidth;
            double ch = eb->m_rect.m_height + 1;
            return LRect<double>(cix-5, ciy, w+9, ch, false);
        }

        if(item.baseItem == LEditBase::startBase()){
            std::shared_ptr<LEditBase> eb = editBase();
            double cix = eb->m_rect.m_x;
            double ciy = eb->m_rect.m_y;
            int w = m_cursorWidth;
            double ch = eb->m_rect.m_height + 1;
            return LRect<double>(cix-5, ciy, w+9, ch, false);
        }

        if(item.baseItem->isText()){
            std::shared_ptr<LEditText> t =  std::dynamic_pointer_cast<LEditText>(item.baseItem);
            if(item.textPos >=0){
                return t->rectForPos(item.textPos, m_cursorWidth);
            }else{
                double cix = t->cursorToX();
                double ciy = t->cursorToY();
                int w = m_cursorWidth;
                double ch = t->m_rect.m_height + 1;
                return LRect<double>(cix-5, ciy, w+9, ch, false);
            }
        }else{
            double cix = item.baseItem->m_rect.m_x;
            double ciy = item.baseItem->m_rect.m_y;
            int w = m_cursorWidth;
            double ch = item.baseItem->m_rect.m_height + 1;
            return LRect<double>(cix-5, ciy, w+9, ch, false);
        }
    }
    return LRect<double>();
}

LRect<double> LIEditor::cursorRect() const
{
    return cursorRectForIndexPos(activeIndexes());
}

LRect<double> LIEditor::anchorRect() const
{
    return cursorRectForIndexPos(selectStartIndexes());
}

void LIEditor::clearContent()
{
    std::shared_ptr<LEditBase> eb = editBase();
    if(eb->isContainer()){
        std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(eb);
        c->clear();
    }else if(eb->isText()){
        std::shared_ptr<LEditText> t =  std::dynamic_pointer_cast<LEditText>(eb);
        t->setText(L"");
    }
}

void LIEditor::setContent(const std::wstring &text)
{
    clearContent();
    doInput(text);
}

void LIEditor::setContentCbor(const LCbor &cbor)
{
    clearContent();
    doInputCbor(cbor);
}

std::wstring LIEditor::content() const
{
    std::shared_ptr<LEditBase> eb = editBase();
    if(eb->isText()){
        std::shared_ptr<LEditText> t =  std::dynamic_pointer_cast<LEditText>(eb);
        return t->text();
    }
    return L"";
}

LCbor LIEditor::contentCbor() const
{
    std::shared_ptr<LEditBase> eb = editBase();
    if(eb != nullptr){
        return eb->toCbor(false);
    }
    return LCbor();
}

std::wstring LIEditor::selectContent() const
{
    std::shared_ptr<LEditBase> eb = editBase();
    if(eb->isText()){
        std::shared_ptr<LEditText> t =  std::dynamic_pointer_cast<LEditText>(eb);
        return t->selectText();
    }
    return L"";
}

LCbor LIEditor::selectContentCbor() const
{
    std::shared_ptr<LEditBase> eb = editBase();
    if(eb != nullptr){
        return eb->toCbor(true);
    }
    return LCbor();
}

bool LIEditor::hasSelectContent() const
{
    std::shared_ptr<LEditBase> eb = editBase();
    if(eb != nullptr){
        return eb->hasSelect();
    }
    return false;
}

int LIEditor::contentRowNum() const
{
    return 1;
}



bool LIEditor::doMovePosition(LMoveOper op, LMoveMode mode)
{
    std::shared_ptr<LEditBase> c = editBase();
    bool ret = false;
    if(mode == LMoveMode::KeepAnchor){
        c->markSelectStart();
        ret = c->moveCursor(op);
        std::vector<int> start = selectStartIndexes();
        std::vector<int> end = activeIndexes();
        doSelectRange(start, end);
    }else{
        c->deSelect();
        ret = c->moveCursor(op);
    }
    update(false, true);
    return ret;
}

bool LIEditor::doMouseClick(const LPoint<double> &p)
{
    std::shared_ptr<LEditBase> c = editBase();
    c->deSelect();
    LRect<double> r = c->bounds();
    if((r.m_y <= p.m_y) && (r.y2() >= p.m_y)){
        c->mouseClick(p);
    }else{
        c->moveToEnd();
    }
    update(false, true);
    return true;
}

bool LIEditor::doMouseRelease(const LPoint<double> &p)
{
    std::shared_ptr<LEditBase> eb = editBase();
    eb->markSelectStart();
    bool ret = eb->mouseClick(p);
    if(ret){
        if(eb->isContainer()){
            std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(eb);
            if(LEditBase::isValid(c->m_activeChild) == false){
                eb->deSelect();
                update(false, true);
                return ret;
            }
        }

        std::vector<int> start = selectStartIndexes();
        std::vector<int> end = activeIndexes();
        doSelectRange(start, end);
    }else{
        eb->deSelect();
    }
    update(false, true);
    return ret;
}

bool LIEditor::doMouseSelectMove(const LPoint<double> &p)
{
    std::shared_ptr<LEditBase> eb = editBase();
    eb->markSelectStart();
    bool ret = eb->mouseClick(p);
    if(ret){
        if(eb->isContainer()){
            std::shared_ptr<LEditContainer> c = std::dynamic_pointer_cast<LEditContainer>(eb);
            if(LEditBase::isValid(c->m_activeChild) == false){
                update(false, true);
                return ret;
            }
        }

        std::vector<int> start = selectStartIndexes();
        std::vector<int> end = activeIndexes();
        doSelectRange(start, end);
        update(false, true);
    }
    return ret;
}

void LIEditor::doDraw(LPainter *painter,
                      const LPoint<double> &pos,
                      int flags,
                      const LRect<double> &clip)
{
    if(m_hideCursor || m_blinkStatus){
        flags = (flags & ~((int)LEditBase::DrawFlags::Cursor));
    }

    std::shared_ptr<LEditBase> c = editBase();
    c->draw(painter, pos, flags, clip);
    if(flags & (int)LEditBase::DrawFlags::Cursor){
        c->drawCursor(painter, pos, m_cursorWidth, clip);
    }
}

void LIEditor::calculateCaret()
{

}

bool LIEditor::checkUpdate()
{
    bool ret = m_isViewUpdate | m_isCaretUpdate;
    m_isViewUpdate = 0;
    m_isCaretUpdate = 0;
    return ret;
}


std::shared_ptr<LPalette> LIEditor::palette()
{
    if(m_palette != nullptr){
        return m_palette;
    }
    return LGuiApplication::palette();
}

const std::shared_ptr<LFont> LIEditor::font() const
{
    if(m_font != nullptr){
        return m_font;
    }
    return LGuiApplication::font();
}

void LIEditor::blink()
{
    if(!m_delayBlink){
        m_blinkStatus = !m_blinkStatus;
    }else{
        m_delayBlink = false;
    }
}

void LIEditor::resetBlink()
{
    m_blinkStatus = false;
    m_delayBlink = true;
}

void LIEditor::setMaxLength(int len)
{
    if(m_maxLength != len){
        m_maxLength = len;
        updateMaxLength();
    }
}

LK_NAMESPACE_END
