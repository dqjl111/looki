#include "ledittext.h"
#include "leditop.h"
#include "lkgui/text/lfontmetrics.h"
#include "leditcontainer.h"
LK_NAMESPACE_BEGIN

LEditText::LEditText()
{

}

bool LEditText::isText() const
{
    return true;
}

int LEditText::activePos() const
{
    return m_activePos;
}

void LEditText::setActivePos(int pos)
{
    if(pos <0){
        m_activePos = 0;
        return;
    }

    if(pos > m_textLayout.text().length()){
        m_activePos = m_textLayout.text().length();
        return;
    }
    m_activePos = pos;
}

bool LEditText::isAtStart() const
{
    return m_activePos == 0;
}

bool LEditText::isAtEnd() const
{
    return m_activePos >= m_textLayout.text().length();
}

void LEditText::moveToStart()
{
    m_activePos = 0;
}

void LEditText::moveToEnd()
{
    m_activePos = (int)m_textLayout.text().length();
}

std::list<std::shared_ptr<LEditOp> > LEditText::removeSelection()
{
    std::list<std::shared_ptr<LEditOp>> ret;
    if(m_cursorState == NoSelect){
        return ret;
    }

    int removeStart = m_partSelectStartId;
    int removeEnd = m_partSelectEndId;
    if(removeStart > removeEnd){
        removeStart = m_partSelectEndId;
        removeEnd = m_partSelectStartId;
    }
    if(removeStart != removeEnd){
        std::shared_ptr<LTextInputDelete> op
            = std::make_shared<LTextInputDelete>(false);
        buildIndexes(op->m_editItemIndex);
        op->m_editItemIndex.push_back(removeStart);
        op->m_text = m_textLayout.text().substr(removeStart, removeEnd - removeStart);
        ret.push_back(op);

        std::wstring t = m_textLayout.text();
        t.erase(removeStart, removeEnd - removeStart);
        m_textLayout.setText(t);
        if(m_activePos > removeEnd){
            m_activePos = removeStart + m_activePos - removeEnd;
        }else{
            if(m_activePos > removeStart){
                m_activePos = removeStart;
            }
        }
    }
    m_partSelectEndId = 0;
    m_partSelectStartId = 0;
    m_cursorState = NoSelect;
    return ret;
}

bool LEditText::moveCursor(LMoveOper op)
{
    // lWarn() << "Move op:" << (int)op;
    // lWarn() << "line count:" << m_textLayout.lineCount() << ", active pos:" << m_activePos;
    // lWarn() << m_textLayout.text();

    switch(op){
    case LMoveOper::NoMove:{
        return true;
    }
    case LMoveOper::StartOfWord:
    case LMoveOper::StartOfLine:{
        LTextLine line = m_textLayout.lineForTextPosition(m_activePos);
        if(line.isValid()){
            m_activePos = line.textStart();
        }else{
            m_activePos = 0;
        }
        return true;
    }
    case LMoveOper::Start:
    case LMoveOper::StartOfBlock:{
        m_activePos = 0;
        return true;
    }
    case LMoveOper::PreviousBlock:
    case LMoveOper::PreviousCharacter:
    case LMoveOper::PreviousWord:
    case LMoveOper::Left:
    case LMoveOper::WordLeft:
    case LMoveOper::PreviousCell:{
        if(m_activePos >0){
            m_activePos--;
            return true;
        }
        break;
    }
    case LMoveOper::EndOfWord:
    case LMoveOper::EndOfLine:{
        LTextLine line = m_textLayout.lineForTextPosition(m_activePos);
        if(line.isValid()){
            m_activePos = line.textStart() + line.textLength();
        }else{
            m_activePos = (int)m_textLayout.text().length();
        }
        return true;
    }
    case LMoveOper::End:
    case LMoveOper::EndOfBlock:{
        m_activePos = (int)m_textLayout.text().length();
        return true;
    }
    case LMoveOper::NextBlock:
    case LMoveOper::NextCharacter:
    case LMoveOper::NextWord:
    case LMoveOper::Right:
    case LMoveOper::WordRight:
    case LMoveOper::NextCell:{
        if(m_activePos < (int)m_textLayout.text().length()){
            m_activePos++;
            return true;
        }
        break;
    }
    case LMoveOper::Up:{
        LTextLine line = m_textLayout.lineForTextPosition(m_activePos);
        if(line.isValid()){
            LTextLine prevLine = m_textLayout.lineForTextPosition(line.textStart()-1);
            if(prevLine.isValid() && (line.textStart() != prevLine.textStart())){
                m_activePos = prevLine.textStart() + m_activePos-line.textStart();
                if(m_activePos > prevLine.textStart()+prevLine.textLength()){
                    m_activePos = prevLine.textStart()+prevLine.textLength();
                }
                return true;
            }
        }
        break;
    }
    case LMoveOper::Down:{
        LTextLine line = m_textLayout.lineForTextPosition(m_activePos);
        if(line.isValid()){
            LTextLine nextLine = m_textLayout.lineForTextPosition(line.textStart()+line.textLength());
            if(nextLine.isValid() && (line.textStart() != nextLine.textStart())){
                m_activePos = nextLine.textStart() + m_activePos-line.textStart();
                if(m_activePos > nextLine.textStart()+nextLine.textLength()){
                    m_activePos = nextLine.textStart()+nextLine.textLength();
                }
                return true;
            }
        }
        break;
    }
    default:{
        break;
    }
    }

    return false;
}

int LEditText::maxIndex() const
{
    return (int)m_textLayout.text().length();
}

LPoint<double> LEditText::activePoint() const
{
    return LPoint<double>(cursorToX(), cursorToY());
}

bool LEditText::setToActivePoint(const LPoint<double> &p)
{
    double x= p.m_x - m_rect.m_x;
    double y= p.m_y - m_rect.m_y;
    for(int i=0; i<m_textLayout.lineCount(); ++i){
        LTextLine line = m_textLayout.lineAt(i);
        if(line.y() <= y){
            if((i+1) == m_textLayout.lineCount()){
                if(line.y() + line.height() >= y ){
                    m_activePos = line.xToCursor(x, LTextLine::CursorBetweenCharacters);
                    return true;
                }
            }else{
                if(line.y() + line.height() > y ){
                    m_activePos = line.xToCursor(x, LTextLine::CursorBetweenCharacters);
                    return true;
                }
            }
        }
    }
    return false;
}

void LEditText::buildTextIndexes(std::vector<int> &indexes)
{
    std::shared_ptr<LEditContainer> parent =m_parent.lock();
    if(parent != nullptr){
        parent->buildIndexes(indexes);
        indexes.push_back(parent->childIndex(getEditT<LEditBase>()));
    }
    indexes.push_back(m_activePos);
}

std::vector<int> LEditText::textIndexes()
{
    std::vector<int> indexes;
    buildTextIndexes(indexes);
    return indexes;
}

std::list<std::shared_ptr<LEditOp> > LEditText::input(const std::wstring &v)
{
    inputText(v);

    std::list<std::shared_ptr<LEditOp>> ret;
    std::shared_ptr<LTextInputDelete> op = std::make_shared<LTextInputDelete>(true);
    buildTextIndexes(op->m_editItemIndex);
    op->m_text = v;
    ret.push_back(op);
    return ret;
}

void LEditText::setText(const std::wstring &v)
{
    std::wstring t = parseInput(v);
    if(t.empty()){
        m_textLayout.setText(v);
        m_activePos = (int)v.size();
    }else{
        m_textLayout.setText(t);
        m_activePos = (int)t.size();
    }
    reserve(v);
}

void LEditText::inputText(const std::wstring &v)
{
    std::wstring t = parseInput(v);
    if(t.empty()){
        m_textLayout.insertText(m_activePos, v);
        m_activePos += (int) v.size();
    }else{
        m_textLayout.insertText(m_activePos, t);
        m_activePos += (int) t.size();
    }
    reserve(v);
}

void LEditText::reserve(const std::wstring &v)
{

}

std::shared_ptr<LEditText> LEditText::create(const std::wstring& v)
{
    std::shared_ptr<LEditText> ret = std::make_shared<LEditText>();
    ret->setText(v);
    return ret;
}

int LEditText::maxLineWidth() const
{
    return LK_TEXT_LINE_MAX_WIDTH;
}

std::wstring LEditText::selectText() const
{
    if(m_cursorState == NoSelect){
        return L"";
    }

    int removeStart = m_partSelectStartId;
    int removeEnd = m_partSelectEndId;
    if(removeStart > removeEnd){
        removeStart = m_partSelectEndId;
        removeEnd = m_partSelectStartId;
    }
    if(removeStart != removeEnd){
        const std::wstring str = text();
        return str.substr(removeStart, removeEnd-removeStart);
    }
    return L"";
}

std::wstring LEditText::parseInput(const std::wstring &v)
{
    return v;
}

uint32_t LEditText::type() const
{
    return uint32_t(LEditType::Text);
}

bool LEditText::isScript() const
{
    return false;
}

bool LEditText::isSriptEnd() const
{
    return false;
}

bool LEditText::canMergeText(const std::wstring&) const
{
    return true;
}

void LEditText::calculateSize()
{
    if(m_isHiding){
        m_rect.m_width =0;
        m_rect.m_height = 0;
        return;
    }

    redoTextLayout();
    LRect<double> t = m_textLayout.boundingRect();
    m_rect.m_width =t.m_width;
    m_rect.m_height = t.m_height;
}

void LEditText::draw(LPainter *painter,
                     const LPoint<double> &pos,
                     int flags,
                     const LRect<double> &clip) const
{
    if(m_isHiding){
        return;
    }

    LRect<double> b = m_rect.translated(pos);
    if(b.isIntersects(clip)){
        std::vector<LTextLayout::FormatRange> selections;
        if (flags & (int)DrawFlags::Selections) {
            LTextLayout::FormatRange o;
            if (m_partSelectStartId < m_partSelectEndId) {
                o.start = m_partSelectStartId;
                o.length = m_partSelectEndId - m_partSelectStartId;
                std::shared_ptr<LPalette> pal = palette();
                o.format.setBackground(pal->brush(LPalette::Highlight));
                o.format.setForeground(pal->brush(LPalette::HighlightedText));
                selections.push_back(o);
            }else if(m_partSelectEndId < m_partSelectStartId){
                o.start = m_partSelectEndId;
                o.length = m_partSelectStartId - m_partSelectEndId;
                std::shared_ptr<LPalette> pal = palette();
                o.format.setBackground(pal->brush(LPalette::Highlight));
                o.format.setForeground(pal->brush(LPalette::HighlightedText));
                selections.push_back(o);
            }else {
                // mask selection
                if (blinkStatus()){
                    o.start = m_activePos;
                    o.length = 1;
                    std::shared_ptr<LPalette> pal = palette();
                    o.format.setBackground(pal->brush(LPalette::Text));
                    o.format.setForeground(pal->brush(LPalette::Window));
                    selections.push_back(o);
                }
            }
        }

        if (flags & (int)DrawFlags::Text){
            m_textLayout.draw(painter, b.topLeft(), selections, b.intersected(clip));
        }
    }
}

void LEditText::drawCursor(LPainter *p,
                           const LPoint<double> &pos,
                           int width,
                           const LRect<double> &clip) const
{
    LRect<double> b = m_rect.translated(pos);
    if(b.m_width<1){
        b.m_width = 1;
    }
    if(b.isIntersects(clip)){
        std::shared_ptr<LPalette> pal = palette();
        std::shared_ptr<LPen> oldPen = p->pen();
        p->setPen(pal->brush(LPalette::Text)->m_color);

        m_textLayout.drawCursor(p,
                                b.topLeft(),
                                m_activePos,
                                width);

        p->setPen(oldPen, false);
    }
}

int LEditText::xToPos(double x, LTextLine::CursorPosition betweenOrOn) const
{
    x-= m_rect.m_x;
    return m_textLayout.lineForTextPosition(m_activePos).xToCursor(x, betweenOrOn);
}

int LEditText::yToLine(double y)
{
    y-= m_rect.m_y;
    for(int i=0; i<m_textLayout.lineCount(); ++i){
        LTextLine line = m_textLayout.lineAt(i);
        if((line.y() <= y) && ((line.y() + line.height())> y)){
            return i;
        }
    }
    return m_textLayout.lineCount();
}

LRect<double> LEditText::rectForPos(int cursor, int cursorWith) const
{
    LTextLine l = m_textLayout.lineForTextPosition(cursor);
    double cix = l.cursorToX(cursor);
    int w = cursorWith;
    double ch = l.height() + 1;
    return LRect<double>(m_rect.m_x+cix-5, m_rect.m_y, w+9, ch, false);
}


void LEditText::redoTextLayout() const
{
    m_textLayout.clearLayout();

    const std::shared_ptr<LFont> fnt = font();
    m_textLayout.setFont(fnt);
    double height = 0;
    LFontMetrics fm(fnt);
    double leading = fm.leading();
    height = -leading;

    m_textLayout.setCacheEnabled(true);
    m_textLayout.beginLayout();
    while (1) {
        LTextLine line = m_textLayout.createLine();
        if (!line.isValid()){
            break;
        }
        line.setLineWidth(maxLineWidth());
        height += leading;
        height = std::ceil(height);
        line.setPosition(LPoint<double>(0., height));
        height += line.height();
    }
    m_textLayout.endLayout();
}

void LEditText::clear()
{
    m_textLayout.setText(L"");
    m_activePos = 0;
}

bool LEditText::deleteActive(bool isForward)
{
    if(isForward){
        if(m_activePos>=0 && m_activePos< (int)m_textLayout.text().length()){
            std::wstring t = m_textLayout.text();
            t.erase(m_activePos, 1);
            m_textLayout.setText(t);

            if(m_activePos >= (int)m_textLayout.text().length()){
                m_activePos = (int)m_textLayout.text().length();
            }
            return true;
        }
    }else{
        int delPos = m_activePos-1;
        if(delPos>=0 && delPos< (int)m_textLayout.text().length()){
            std::wstring t = m_textLayout.text();
            t.erase(delPos, 1);
            m_textLayout.setText(t);
            m_activePos = delPos;
            return true;
        }
    }
    return false;
}

bool LEditText::deletePos(int pos, int length)
{
    if(pos>=0 && pos< (int)m_textLayout.text().length()){
        m_activePos = pos;
        std::wstring t = m_textLayout.text();
        t.erase(pos, length);
        m_textLayout.setText(t);
        return true;
    }
    return false;
}


LK_NAMESPACE_END
