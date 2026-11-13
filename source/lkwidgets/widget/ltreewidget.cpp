#include "ltreewidget.h"
#include "lkwidgets/widget/llineedit.h"
#include "lkwidgets/util/leditbase.h"
#include "lkwidgets/draw/ldrawutil.h"
#include "lkgui/paint/lpainter.h"
#include "lkgui/paint/lline.h"
#include "lkgui/image/ltexture.h"
#include "lkgui/text/lfontmetrics.h"
#include <algorithm>
#include <chrono>

LK_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// LTreeWidgetItem
// ---------------------------------------------------------------------------
LTreeWidgetItem::LTreeWidgetItem(const std::wstring &text,
                                 std::shared_ptr<LTexture> icon,
                                 const LVariant &userData)
    : LActionBase(text, icon, userData)
{
    m_isListItem = true;
    m_texts.push_back(text);
}

LTreeWidgetItem::LTreeWidgetItem(const std::vector<std::wstring> &texts,
                                 std::shared_ptr<LTexture> icon,
                                 const LVariant &userData)
    : LActionBase(texts.empty() ? std::wstring() : texts.front(), icon, userData)
{
    m_isListItem = true;
    m_texts = texts;
    if(m_texts.empty()){
        m_texts.push_back(std::wstring());
    }
}

int LTreeWidgetItem::columnCount() const
{
    return std::max<int>(1, (int)m_texts.size());
}

void LTreeWidgetItem::setColumnCount(int count)
{
    m_texts.resize(std::max<int>(1, count));
    if(auto tree = m_tree.lock()){
        tree->onItemChanged();
    }
}

std::wstring LTreeWidgetItem::text(int column) const
{
    if(column < 0 || column >= (int)m_texts.size()){
        return std::wstring();
    }
    if(column == 0){
        return LActionBase::text();
    }
    return m_texts[column];
}

void LTreeWidgetItem::setText(int column, const std::wstring &t)
{
    if(column < 0){
        return;
    }
    if(column >= (int)m_texts.size()){
        m_texts.resize(column + 1);
    }
    m_texts[column] = t;
    if(column == 0){
        LActionBase::setText(t);
    }
    if(auto tree = m_tree.lock()){
        tree->onItemChanged();
    }
}

int LTreeWidgetItem::addChild(const std::shared_ptr<LTreeWidgetItem> &child)
{
    if(child == nullptr || child.get() == this){
        return -1;
    }
    child->m_parentItem = getPtrT<LTreeWidgetItem>();
    m_children.push_back(child);
    if(auto tree = m_tree.lock()){
        tree->onItemChanged();
    }
    return (int)m_children.size() - 1;
}

void LTreeWidgetItem::insertChild(int index, const std::shared_ptr<LTreeWidgetItem> &child)
{
    if(child == nullptr || child.get() == this){
        return;
    }
    index = std::max<int>(0, std::min<int>(index, (int)m_children.size()));
    child->m_parentItem = getPtrT<LTreeWidgetItem>();
    m_children.insert(m_children.begin() + index, child);
    if(auto tree = m_tree.lock()){
        tree->onItemChanged();
    }
}

bool LTreeWidgetItem::removeChild(const std::shared_ptr<LTreeWidgetItem> &child)
{
    const int index = indexOfChild(child);
    if(index < 0){
        return false;
    }
    m_children[index]->m_parentItem.reset();
    m_children.erase(m_children.begin() + index);
    if(auto tree = m_tree.lock()){
        tree->onItemChanged();
    }
    return true;
}

std::shared_ptr<LTreeWidgetItem> LTreeWidgetItem::child(int index) const
{
    if(index < 0 || index >= (int)m_children.size()){
        return nullptr;
    }
    return m_children[index];
}

int LTreeWidgetItem::childCount() const
{
    return (int)m_children.size();
}

int LTreeWidgetItem::indexOfChild(const std::shared_ptr<LTreeWidgetItem> &child) const
{
    for(int i = 0; i < (int)m_children.size(); ++i){
        if(m_children[i] == child){
            return i;
        }
    }
    return -1;
}

std::shared_ptr<LTreeWidgetItem> LTreeWidgetItem::parentItem() const
{
    return m_parentItem.lock();
}

int LTreeWidgetItem::depth() const
{
    int d = 0;
    std::shared_ptr<LTreeWidgetItem> p = m_parentItem.lock();
    while(p != nullptr){
        ++d;
        p = p->m_parentItem.lock();
    }
    return d;
}

bool LTreeWidgetItem::isLeaf() const
{
    return m_children.empty();
}

bool LTreeWidgetItem::isExpanded() const
{
    return m_expanded;
}

void LTreeWidgetItem::setExpanded(bool expanded)
{
    if(m_expanded == expanded){
        return;
    }
    m_expanded = expanded;
    if(auto tree = m_tree.lock()){
        tree->onItemChanged();
    }
}

void LTreeWidgetItem::setEditable(bool editable)
{
    if(m_editable != editable){
        m_editable = editable;
        if(auto tree = m_tree.lock()){
            tree->onItemChanged();
        }
    }
}

bool LTreeWidgetItem::isEditable() const
{
    return m_editable;
}

void LTreeWidgetItem::setCellEditable(int column, bool editable)
{
    if(column < 0){
        return;
    }
    if(column >= (int)m_cellEditable.size()){
        m_cellEditable.resize(column + 1, -1);
    }
    m_cellEditable[column] = editable ? 1 : 0;
    if(auto tree = m_tree.lock()){
        tree->onItemChanged();
    }
}

void LTreeWidgetItem::resetCellEditable(int column)
{
    if(column < 0 || column >= (int)m_cellEditable.size()){
        return;
    }
    m_cellEditable[column] = -1;
    if(auto tree = m_tree.lock()){
        tree->onItemChanged();
    }
}

int LTreeWidgetItem::cellEditable(int column) const
{
    if(column < 0 || column >= (int)m_cellEditable.size()){
        return -1;
    }
    return m_cellEditable[column];
}

void LTreeWidgetItem::activate(ActionEvent event)
{
    LActionBase::activate(event);
    if(event == Trigger){
        sigTrigger.emt(getPtrT<LTreeWidgetItem>());
    }
}

void LTreeWidgetItem::triggerDoubleClick()
{
    sigDoubleClick.emt(getPtrT<LTreeWidgetItem>());
}

// ---------------------------------------------------------------------------
// 单元格就地编辑框：回车提交、Esc 取消、失焦提交
// ---------------------------------------------------------------------------
class LTreeItemEditor : public LLineEdit
{
public:
    explicit LTreeItemEditor(std::shared_ptr<LWidget> parent)
        : LLineEdit(parent)
    {
    }

    LSignal<> sigCommit;
    LSignal<> sigCancel;

    /** setText 后光标停在开头，直接输入会插到最前面 */
    void moveCursorToEnd()
    {
        if(m_editor == nullptr){
            return;
        }
        std::shared_ptr<LEditBase> base = m_editor->editBase();
        if(base != nullptr){
            base->setActivePos((int)text().size());
        }
    }

protected:
    // 只覆盖一行单元格，聚焦描边用 1px
    int focusFrameWidth() const override { return 1; }

    void keyPressEvent(std::shared_ptr<LKeyEvent> e) override
    {
        const uint32_t key = e->key();
        // 0x01000004 = Return，0x01000005 = Enter，0x01000000 = Escape；
        // 兼容直接送来 CR/LF、ESC 字符值的平台消息
        if(key == 0x01000004 || key == 0x01000005 || key == 13 || key == 10){
            e->accept();
            sigCommit.emt();
            return;
        }
        if(key == 0x01000000 || key == 27){
            e->accept();
            sigCancel.emt();
            return;
        }
        LLineEdit::keyPressEvent(e);
    }

    void focusOutEvent(std::shared_ptr<LFocusEvent> e) override
    {
        LLineEdit::focusOutEvent(e);
        sigCommit.emt();
    }
};

// ---------------------------------------------------------------------------
// 内部视图：绘制与鼠标命中
// ---------------------------------------------------------------------------
class LTreeWidgetView : public LWidget
{
public:
    explicit LTreeWidgetView(std::shared_ptr<LWidget> parent)
        : LWidget(parent)
    {
    }

    void updateScrollBars()
    {
        std::shared_ptr<LTreeWidget> owner = parentT<LTreeWidget>();
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        if(owner == nullptr || area == nullptr){
            return;
        }

        const int rowsH = owner->visibleRowCount() * owner->rowHeight();
        const int viewH = std::max<int>(1, height() - owner->headerHeight());
        std::shared_ptr<LScrollBar> vBar = area->verticalScrollBar();
        if(vBar != nullptr){
            vBar->setRange(0, std::max<int>(0, rowsH - viewH));
            vBar->setPageStep(viewH);
            vBar->setSingleStep(std::max<int>(1, owner->rowHeight()));
            vBar->setSliderPosition(vBar->value());
        }

        std::shared_ptr<LScrollBar> hBar = area->horizontalScrollBar();
        if(hBar != nullptr){
            hBar->setRange(0, std::max<int>(0, owner->contentWidth(width()) - width()));
            hBar->setPageStep(std::max<int>(1, width()));
            hBar->setSingleStep(20);
            hBar->setSliderPosition(hBar->value());
        }
    }

protected:
    void paintEvent(std::shared_ptr<LPaintEvent> e) override
    {
        (void)e;
        std::shared_ptr<LTreeWidget> owner = parentT<LTreeWidget>();
        if(owner == nullptr){
            return;
        }
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        const int offX = area != nullptr ? area->contentsOffset().m_x : 0;
        const int offY = area != nullptr ? area->contentsOffset().m_y : 0;
        const int vw = width();
        const int vh = height();
        const int hh = owner->headerHeight();
        const int rh = owner->rowHeight();

        LPainter p(paintDevice());
        std::shared_ptr<LPalette> pal = palette();
        p.fillRect(rect().toRectF(), pal->brush(LPalette::Base));

        const LFontMetrics fm = fontMetrics();
        const LColor baseC = pal->brush(LPalette::Base)->m_color;
        const LColor textC = pal->brush(LPalette::Text)->m_color;
        const LColor hlC = pal->brush(LPalette::Highlight)->m_color;
        const auto mix = [](uint8_t a, uint8_t b, double f) -> uint8_t {
            return uint8_t(a + (int(b) - int(a)) * f);
        };
        const LColor gridC(mix(baseC.m_red, textC.m_red, 0.14),
                           mix(baseC.m_green, textC.m_green, 0.14),
                           mix(baseC.m_blue, textC.m_blue, 0.14));
        const LColor selC(mix(baseC.m_red, hlC.m_red, 0.18),
                          mix(baseC.m_green, hlC.m_green, 0.18),
                          mix(baseC.m_blue, hlC.m_blue, 0.18));
        const LColor borderC = pal->brush(LPalette::Mid)->m_color;

        // ---- 表头：固定在视口顶部，只随横向滚动平移 ----
        if(hh > 0){
            p.save();
            p.setClipRect(LRect<int>(0, 0, vw, hh, false), LClipOper::Intersect);
            std::shared_ptr<LRenderRule> sectionRule =
                    owner->elementStyle().getRenderRule(LUiElementType::Section,
                                                        (uint16_t)LUiState::Enabled);
            if(sectionRule != nullptr && sectionRule->hasDrawable()){
                sectionRule->drawRule(&p, LRect<int>(0, 0, vw, hh, false));
            }else{
                p.setBrush(pal->brush(LPalette::Button)->m_color, LBrush::SolidPattern);
                p.setPen(LPenStyle::No);
                p.drawRect(LRect<int>(0, 0, vw, hh, false));
            }

            p.setPen(borderC, LPenStyle::Solid, 1);
            p.drawLine(LLine<int>(0, hh - 1, vw, hh - 1));

            const double hBaseline = (hh - fm.height()) / 2.0 + fm.ascent();
            for(int c = 0; c < owner->columnCount(); ++c){
                const int cx = owner->columnX(c) - offX;
                const int cw = owner->columnWidth(c);
                if(cx + cw <= 0 || cx >= vw){
                    continue;
                }
                p.setPen(borderC, LPenStyle::Solid, 1);
                if(c > 0){
                    p.drawLine(LLine<int>(cx, 0, cx, hh));
                }
                p.setPen(textC, LPenStyle::Solid, 1);
                p.save();
                p.setClipRect(LRect<int>(cx, 0, cw, hh, false), LClipOper::Intersect);
                p.drawText(LPoint<double>(cx + 8, hBaseline), owner->headerLabel(c));
                p.restore();
            }
            p.restore();
        }

        // ---- 行：内容坐标 -> 视口坐标 = (x - offX, y + hh - offY) ----
        const int contentLeft = -offX;
        p.save();
        p.setClipRect(LRect<int>(0, hh, vw, std::max<int>(1, vh - hh), false),
                      LClipOper::Intersect);

        for(int row = 0; row < owner->visibleRowCount(); ++row){
            LRect<int> r = owner->rowRect(row, vw);
            r.translate(-offX, hh - offY);
            if(r.y2() < hh || r.m_y > vh){
                continue;
            }

            p.save();
            p.setRenderHints(LRenderHint::Antialiasing, true);

            uint16_t itemState = (uint16_t)LUiState::Enabled;
            if(row == owner->currentRow()){
                itemState |= (uint16_t)LUiState::Selected;
            }
            if(row == m_hoverRow){
                itemState |= (uint16_t)LUiState::Hover;
            }
            std::shared_ptr<LRenderRule> rule =
                    owner->elementStyle().getRenderRule(LUiElementType::Item, itemState);
            bool hasBackground = false;
            if(rule != nullptr && rule->hasDrawable()){
                rule->drawRule(&p, r);
                hasBackground = true;
            }
            if(!hasBackground && row == owner->currentRow()){
                p.setBrush(selC, LBrush::SolidPattern);
                p.setPen(LPenStyle::No);
                p.drawRoundedRect(r.toRectF().adjusted(2, 2, -2, -2), 4, 4,
                                  LSizeMode::Absolute);
            }

            // 列分隔线
            p.setPen(gridC, LPenStyle::Solid, 1);
            for(int c = 1; c < owner->columnCount(); ++c){
                const int cx = contentLeft + owner->columnX(c);
                if(cx >= 0 && cx <= vw){
                    p.drawLine(LLine<int>(cx, r.m_y, cx, r.y2()));
                }
            }

            std::shared_ptr<LTreeWidgetItem> item = owner->itemAtRow(row);
            if(item != nullptr){
                const int depth = item->depth();
                const int arrowX = contentLeft + 6 + depth * owner->indentation();
                const int arrowY = r.m_y + (rh - 8) / 2;

                // 展开标记：实心三角形，展开朝下、折叠朝右
                if(!item->isLeaf()){
                    LPoint<double> tri[3];
                    if(item->isExpanded()){
                        tri[0] = LPoint<double>(arrowX, arrowY + 1);
                        tri[1] = LPoint<double>(arrowX + 8, arrowY + 1);
                        tri[2] = LPoint<double>(arrowX + 4, arrowY + 7);
                    }else{
                        tri[0] = LPoint<double>(arrowX, arrowY);
                        tri[1] = LPoint<double>(arrowX, arrowY + 8);
                        tri[2] = LPoint<double>(arrowX + 6, arrowY + 4);
                    }
                    p.setBrush(textC, LBrush::SolidPattern);
                    p.setPen(LPenStyle::No);
                    p.drawPolygon(tri, 3);
                }

                // 样式表前景色优先；没有规则时用默认文本色
                bool hasForeground = false;
                if(rule != nullptr){
                    hasForeground = (rule->applyForeground(&p) & 0x01) != 0;
                }
                if(!hasForeground){
                    p.setPen(textC, LPenStyle::Solid, 1);
                }

                const double baseline = r.m_y + (rh - fm.height()) / 2.0 + fm.ascent();
                const LSize<int> iconSize = owner->iconSize();
                double textX = contentLeft + 6 + depth * owner->indentation() + 12;
                if(item->icon() != nullptr){
                    const double iy = r.m_y + (rh - iconSize.m_y) / 2.0;
                    p.drawPixmap(LPoint<double>(textX, iy),
                                 item->icon()->getPixmap(iconSize, LTexture::Normal, true));
                    textX += iconSize.m_x + 6;
                }

                p.save();
                p.setClipRect(LRect<int>(contentLeft + owner->columnX(0), r.m_y,
                                         owner->columnWidth(0), rh, false),
                              LClipOper::Intersect);
                p.drawText(LPoint<double>(textX, baseline), item->text(0));
                p.restore();

                // 属性列：从各自列左边界开始，超长部分由裁剪忽略
                for(int c = 1; c < owner->columnCount(); ++c){
                    const int cx = contentLeft + owner->columnX(c);
                    const int cw = owner->columnWidth(c);
                    if(cx + cw <= 0 || cx >= vw){
                        continue;
                    }
                    p.save();
                    p.setClipRect(LRect<int>(cx, r.m_y, cw, rh, false),
                                  LClipOper::Intersect);
                    p.drawText(LPoint<double>(cx + 8, baseline), item->text(c));
                    p.restore();
                }
            }

            p.restore();
        }
        p.restore();
    }

    void resizeEvent(std::shared_ptr<LResizeEvent> e) override
    {
        updateScrollBars();
        LWidget::resizeEvent(e);
    }

    /** 视口坐标 -> 行内容坐标 */
    LPoint<int> toContent(const LPoint<int> &pos) const
    {
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        std::shared_ptr<LTreeWidget> owner = parentT<LTreeWidget>();
        LPoint<int> p = pos;
        if(area != nullptr){
            p.m_x += area->contentsOffset().m_x;
            p.m_y += area->contentsOffset().m_y;
        }
        if(owner != nullptr){
            p.m_y -= owner->headerHeight();
        }
        return p;
    }

    void mouseMoveEvent(std::shared_ptr<LMouseEvent> e) override
    {
        std::shared_ptr<LTreeWidget> owner = parentT<LTreeWidget>();
        if(owner == nullptr){
            e->ignore();
            return;
        }
        int row = -1;
        if(!(owner->isHeaderVisible() && e->pos().m_y < owner->headerHeight())){
            row = owner->rowAt(toContent(e->pos()), width());
        }
        if(row != m_hoverRow){
            m_hoverRow = row;
            update();
        }
        LWidget::mouseMoveEvent(e);
    }

    void mousePressEvent(std::shared_ptr<LMouseEvent> e) override
    {
        std::shared_ptr<LTreeWidget> owner = parentT<LTreeWidget>();
        if(owner == nullptr || e->button() != LMouseButton::Left){
            e->ignore();
            return;
        }

        // 表头点击：发出列号，便于调用方排序
        if(owner->isHeaderVisible() && e->pos().m_y < owner->headerHeight()){
            std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
            const int x = e->pos().m_x + (area != nullptr ? area->contentsOffset().m_x : 0);
            const int col = owner->columnAt(x);
            if(col >= 0){
                owner->sigHeaderClicked.emt(col);
                e->accept();
                return;
            }
            e->ignore();
            return;
        }

        const LPoint<int> contentPos = toContent(e->pos());
        const int row = owner->rowAt(contentPos, width());
        if(row < 0){
            e->ignore();
            return;
        }

        // 点中展开标记：只切换展开状态，不改变选中行
        if(owner->branchRect(row).contains(contentPos)){
            owner->toggleExpanded(row);
            e->accept();
            return;
        }

        owner->setCurrentRow(row);
        owner->sigItemClicked.emt(row);
        std::shared_ptr<LTreeWidgetItem> item = owner->itemAtRow(row);
        if(item != nullptr){
            item->activate(LActionBase::Trigger);
        }

        // 与 LListWidget 一致：自行判定双击，避免系统双击消息不派发
        const auto now = std::chrono::steady_clock::now();
        const bool isDoubleClick = (row == m_lastClickRow)
                && (now - m_lastClickTime < std::chrono::milliseconds(500));
        if(isDoubleClick){
            m_lastClickRow = -1;
            m_lastClickTime = {};
            owner->sigItemDoubleClicked.emt(row);
            if(item != nullptr){
                item->triggerDoubleClick();
            }
            // 单元格允许编辑则进入就地编辑；否则（如默认只读的树列）双击用于展开/折叠
            const int col = owner->columnAt(contentPos.m_x);
            if(col >= 0 && owner->isCellEditable(row, col)){
                owner->editItem(row, col);
            }else if(item != nullptr && !item->isLeaf()){
                owner->toggleExpanded(row);
            }
        }else{
            m_lastClickRow = row;
            m_lastClickTime = now;
        }
        e->accept();
    }

    int m_hoverRow = -1;
    int m_lastClickRow = -1;
    std::chrono::steady_clock::time_point m_lastClickTime{};
};

// ---------------------------------------------------------------------------
// LTreeWidget
// ---------------------------------------------------------------------------
LTreeWidget::LTreeWidget(std::shared_ptr<LWidget> parent)
    : LAbstractScrollArea(parent)
{
    // 滚动条在 initScroll() 之后才存在，构造中不能设置策略
}

void LTreeWidget::initSpecific()
{
    LAbstractScrollArea::initSpecific();
    setVerticalScrollBarPolicy(LScrollPolicy::AsNeeded);
    setHorizontalScrollBarPolicy(LScrollPolicy::AsNeeded);
    if(m_iconSize.isNull()){
        m_iconSize = LSize<int>(16, 16);
    }
}

void LTreeWidget::initViewport()
{
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    std::shared_ptr<LTreeWidgetView> view = makeInit<LTreeWidgetView>(q);
    view->setBackgroundRole(LPalette::Base);
    view->setAutoFillBackground(true);
    view->addEventFilter(q);
    m_viewport = view;
    m_view = view;
}

LSize<int> LTreeWidget::sizeHint() const
{
    return LSize<int>(420, 240);
}

// ---- 列与表头 ----
int LTreeWidget::columnCount() const
{
    return m_columnCount;
}

void LTreeWidget::setColumnCount(int count)
{
    count = std::max<int>(1, count);
    if(count == m_columnCount && (int)m_columnWidths.size() == count){
        return;
    }
    const int old = m_columnCount;
    m_columnCount = count;
    m_columnWidths.resize(count);
    m_headers.resize(count);
    m_columnEditable.resize(count);
    for(int c = old; c < count; ++c){
        m_columnWidths[c] = (c == 0) ? 200 : 120;
        m_columnEditable[c] = (c == 0) ? 0 : 1;
    }
    refresh();
}

int LTreeWidget::columnWidth(int column) const
{
    if(column < 0 || column >= (int)m_columnWidths.size()){
        return 0;
    }
    return std::max<int>(1, m_columnWidths[column]);
}

void LTreeWidget::setColumnWidth(int column, int width)
{
    if(column < 0 || column >= m_columnCount){
        return;
    }
    m_columnWidths[column] = std::max<int>(20, width);
    refresh();
}

std::vector<std::wstring> LTreeWidget::headerLabels() const
{
    return m_headers;
}

void LTreeWidget::setHeaderLabels(const std::vector<std::wstring> &labels)
{
    if((int)labels.size() > m_columnCount){
        setColumnCount((int)labels.size());
    }
    for(int c = 0; c < (int)labels.size(); ++c){
        m_headers[c] = labels[c];
    }
    refresh();
}

std::wstring LTreeWidget::headerLabel(int column) const
{
    if(column < 0 || column >= (int)m_headers.size()){
        return std::wstring();
    }
    return m_headers[column];
}

bool LTreeWidget::isHeaderVisible() const
{
    return m_headerVisible;
}

void LTreeWidget::setHeaderVisible(bool visible)
{
    if(m_headerVisible != visible){
        m_headerVisible = visible;
        refresh();
    }
}

// ---- 树结构 ----
void LTreeWidget::registerSubTree(const std::shared_ptr<LTreeWidgetItem> &item)
{
    if(item == nullptr){
        return;
    }
    item->m_tree = getPtrT<LTreeWidget>();
    for(int i = 0; i < item->childCount(); ++i){
        registerSubTree(item->child(i));
    }
}

void LTreeWidget::releaseSubTree(const std::shared_ptr<LTreeWidgetItem> &item)
{
    if(item == nullptr){
        return;
    }
    item->m_tree.reset();
    for(int i = 0; i < item->childCount(); ++i){
        releaseSubTree(item->child(i));
    }
}

void LTreeWidget::collect(const std::shared_ptr<LTreeWidgetItem> &item)
{
    if(item == nullptr){
        return;
    }
    m_visible.push_back(item);
    if(!item->isExpanded()){
        return;
    }
    for(int i = 0; i < item->childCount(); ++i){
        collect(item->child(i));
    }
}

void LTreeWidget::walkAll(const std::shared_ptr<LTreeWidgetItem> &item, bool expanded)
{
    if(item == nullptr){
        return;
    }
    item->m_expanded = expanded;
    for(int i = 0; i < item->childCount(); ++i){
        walkAll(item->child(i), expanded);
    }
}

void LTreeWidget::rebuildVisible()
{
    m_visible.clear();
    for(const auto &root : m_roots){
        collect(root);
    }
    if(m_currentRow >= (int)m_visible.size()){
        m_currentRow = (int)m_visible.size() - 1;
    }
    // 同步 action 的选中状态
    for(int i = 0; i < (int)m_visible.size(); ++i){
        m_visible[i]->setSelected(i == m_currentRow);
    }
}

void LTreeWidget::refresh()
{
    // 结构/展开状态变化会让正在编辑的单元格错位，先提交
    if(m_editRow >= 0){
        commitEdit();
    }
    for(const auto &root : m_roots){
        registerSubTree(root);
    }
    rebuildVisible();
    if(std::shared_ptr<LTreeWidgetView> view = m_view.lock()){
        view->updateScrollBars();
        view->update();
    }else if(m_viewport != nullptr){
        m_viewport->update();
    }
    update();
}

void LTreeWidget::onItemChanged()
{
    refresh();
}

int LTreeWidget::addTopLevelItem(const std::shared_ptr<LTreeWidgetItem> &item)
{
    if(item == nullptr || item->m_parentItem.lock() != nullptr){
        return -1;
    }
    if(std::shared_ptr<LTreeWidget> t = item->m_tree.lock()){
        if(t.get() != this){
            return -1;
        }
    }
    m_roots.push_back(item);
    refresh();
    return (int)m_roots.size() - 1;
}

std::shared_ptr<LTreeWidgetItem> LTreeWidget::addTopLevelItem(
        const std::wstring &text, std::shared_ptr<LTexture> icon,
        const LVariant &userData)
{
    std::shared_ptr<LTreeWidgetItem> item =
            std::make_shared<LTreeWidgetItem>(text, icon, userData);
    addTopLevelItem(item);
    return item;
}

int LTreeWidget::addChild(const std::shared_ptr<LTreeWidgetItem> &parent,
                          const std::shared_ptr<LTreeWidgetItem> &child)
{
    if(parent == nullptr || child == nullptr || child.get() == parent.get()){
        return -1;
    }
    if(parent->m_tree.lock().get() != this){
        return -1;
    }
    if(child->m_parentItem.lock() != nullptr){
        return -1;
    }
    if(std::shared_ptr<LTreeWidget> t = child->m_tree.lock()){
        if(t.get() != this){
            return -1;
        }
    }
    child->m_parentItem = parent;
    parent->m_children.push_back(child);
    refresh();
    return parent->childCount() - 1;
}

std::shared_ptr<LTreeWidgetItem> LTreeWidget::addLeaf(
        const std::shared_ptr<LTreeWidgetItem> &parent,
        const std::vector<std::wstring> &texts,
        std::shared_ptr<LTexture> icon,
        const LVariant &userData)
{
    std::shared_ptr<LTreeWidgetItem> item =
            std::make_shared<LTreeWidgetItem>(texts, icon, userData);
    addChild(parent, item);
    return item;
}

void LTreeWidget::clear()
{
    cancelEdit();
    for(const auto &root : m_roots){
        releaseSubTree(root);
    }
    m_roots.clear();
    m_visible.clear();
    m_currentRow = -1;
    refresh();
}

int LTreeWidget::topLevelItemCount() const
{
    return (int)m_roots.size();
}

std::shared_ptr<LTreeWidgetItem> LTreeWidget::topLevelItem(int index) const
{
    if(index < 0 || index >= (int)m_roots.size()){
        return nullptr;
    }
    return m_roots[index];
}

// ---- 展开 / 折叠 ----
void LTreeWidget::setItemExpanded(const std::shared_ptr<LTreeWidgetItem> &item,
                                  bool expanded)
{
    if(item == nullptr || item->isExpanded() == expanded){
        return;
    }
    item->m_expanded = expanded;
    refresh();
    if(expanded){
        sigItemExpanded.emt(item);
    }else{
        sigItemCollapsed.emt(item);
    }
}

void LTreeWidget::expandItem(const std::shared_ptr<LTreeWidgetItem> &item)
{
    setItemExpanded(item, true);
}

void LTreeWidget::collapseItem(const std::shared_ptr<LTreeWidgetItem> &item)
{
    setItemExpanded(item, false);
}

void LTreeWidget::toggleExpanded(int row)
{
    std::shared_ptr<LTreeWidgetItem> item = itemAtRow(row);
    if(item == nullptr || item->isLeaf()){
        return;
    }
    setItemExpanded(item, !item->isExpanded());
}

void LTreeWidget::expandAll()
{
    for(const auto &root : m_roots){
        walkAll(root, true);
    }
    refresh();
}

void LTreeWidget::collapseAll()
{
    for(const auto &root : m_roots){
        walkAll(root, false);
    }
    refresh();
}

// ---- 选中与可见行 ----
int LTreeWidget::visibleRowCount() const
{
    return (int)m_visible.size();
}

std::shared_ptr<LTreeWidgetItem> LTreeWidget::itemAtRow(int row) const
{
    if(row < 0 || row >= (int)m_visible.size()){
        return nullptr;
    }
    return m_visible[row];
}

int LTreeWidget::rowOf(const std::shared_ptr<LTreeWidgetItem> &item) const
{
    for(int i = 0; i < (int)m_visible.size(); ++i){
        if(m_visible[i] == item){
            return i;
        }
    }
    return -1;
}

std::shared_ptr<LTreeWidgetItem> LTreeWidget::currentItem() const
{
    return itemAtRow(m_currentRow);
}

int LTreeWidget::currentRow() const
{
    return m_currentRow;
}

void LTreeWidget::setCurrentRow(int row)
{
    if(row < -1 || row >= (int)m_visible.size() || row == m_currentRow){
        return;
    }
    m_currentRow = row;
    for(int i = 0; i < (int)m_visible.size(); ++i){
        m_visible[i]->setSelected(i == row);
    }
    sigCurrentItemChanged.emt(currentItem());
    if(m_viewport != nullptr){
        m_viewport->update();
    }
}

void LTreeWidget::setCurrentItem(const std::shared_ptr<LTreeWidgetItem> &item)
{
    setCurrentRow(rowOf(item));
}

// ---- 外观 ----
void LTreeWidget::setIconSize(const LSize<int> &size)
{
    if(m_iconSize != size){
        m_iconSize = size;
        refresh();
    }
}

const LSize<int> &LTreeWidget::iconSize() const
{
    return m_iconSize;
}

void LTreeWidget::setIndentation(int indent)
{
    if(m_indentation != indent){
        m_indentation = std::max<int>(8, indent);
        refresh();
    }
}

int LTreeWidget::indentation() const
{
    return m_indentation;
}

// ---- 属性列就地编辑 ----
void LTreeWidget::setEditable(bool editable)
{
    m_editable = editable;
    if(!editable){
        cancelEdit();
    }
}

bool LTreeWidget::isEditable() const
{
    return m_editable;
}

void LTreeWidget::setColumnEditable(int column, bool editable)
{
    if(column < 0 || column >= m_columnCount){
        return;
    }
    m_columnEditable[column] = editable ? 1 : 0;
    if(!editable && m_editRow >= 0 && m_editColumn == column){
        cancelEdit();
    }
}

bool LTreeWidget::isColumnEditable(int column) const
{
    if(column < 0 || column >= (int)m_columnEditable.size()){
        return false;
    }
    return m_columnEditable[column] != 0;
}

bool LTreeWidget::isCellEditable(const std::shared_ptr<LTreeWidgetItem> &item,
                                 int column) const
{
    if(!m_editable || item == nullptr || column < 0 || column >= m_columnCount){
        return false;
    }
    // 单元格显式设置优先级最高，其次是行级 + 列级默认
    const int cell = item->cellEditable(column);
    if(cell >= 0){
        return cell != 0;
    }
    return isColumnEditable(column) && item->isEditable();
}

bool LTreeWidget::isCellEditable(int row, int column) const
{
    return isCellEditable(itemAtRow(row), column);
}

void LTreeWidget::editItem(int row, int column)
{
    if(row < 0 || row >= (int)m_visible.size() || m_viewport == nullptr){
        return;
    }
    std::shared_ptr<LTreeWidgetItem> item = itemAtRow(row);
    if(item == nullptr || !isCellEditable(item, column)){
        return;
    }
    // 已有编辑时先提交，避免旧内容被覆盖
    if(m_editRow >= 0){
        commitEdit();
    }

    std::shared_ptr<LTreeItemEditor> editor =
            std::dynamic_pointer_cast<LTreeItemEditor>(m_editor);
    if(editor == nullptr){
        editor = makeInit<LTreeItemEditor>(m_viewport);
        editor->setName("树控件单元格编辑器");
        editor->sigCommit.connect(this, &LTreeWidget::commitEdit);
        editor->sigCancel.connect(this, &LTreeWidget::cancelEdit);
        m_editor = editor;
    }

    m_editRow = row;
    m_editColumn = column;
    editor->setText(item->text(column));

    // 单元格内容坐标 -> 视口坐标，编辑框宽度贴合该列，避免盖到相邻列
    const LPoint<int> offset = contentsOffset();
    const int x = columnX(column) - offset.m_x;
    const int y = row * rowHeight() + headerHeight() - offset.m_y;
    const int cellH = rowHeight();
    const int editH = std::max<int>(22, cellH);
    LRect<int> r(x, y - (editH - cellH) / 2, columnWidth(column), editH, false);
    editor->setGeometry(r);
    editor->setVisible(true);
    editor->setFocus();
    editor->moveCursorToEnd();
    m_viewport->update();
}

void LTreeWidget::commitEdit()
{
    if(m_editor == nullptr || m_editRow < 0){
        return;
    }
    std::shared_ptr<LTreeItemEditor> editor =
            std::dynamic_pointer_cast<LTreeItemEditor>(m_editor);
    const int row = m_editRow;
    const int column = m_editColumn;
    m_editRow = -1;
    m_editColumn = -1;
    // setVisible(false) 可能再次触发失焦提交，此时 m_editRow 已置 -1
    m_editor->setVisible(false);

    std::shared_ptr<LTreeWidgetItem> item = itemAtRow(row);
    if(editor != nullptr && item != nullptr && column >= 0){
        const std::wstring text = editor->text();
        if(item->text(column) != text){
            item->setText(column, text);
            sigItemTextChanged.emt(item, column);
        }
    }
    if(m_viewport != nullptr){
        m_viewport->update();
    }
    update();
}

void LTreeWidget::cancelEdit()
{
    if(m_editor != nullptr){
        m_editor->setVisible(false);
    }
    m_editRow = -1;
    m_editColumn = -1;
    if(m_viewport != nullptr){
        m_viewport->update();
    }
}

void LTreeWidget::scrollContentsBy(int dx, int dy)
{
    // 编辑框不跟随内容滚动，滚动前先提交
    if(m_editRow >= 0){
        commitEdit();
    }
    LAbstractScrollArea::scrollContentsBy(dx, dy);
}

// ---- 几何（内容坐标）----
int LTreeWidget::rowHeight() const
{
    const LFontMetrics fm = fontMetrics();
    const int iconH = std::max<int>(1, m_iconSize.m_y);
    const int textH = (int)fm.height();
    return std::max<int>(26, std::max<int>(iconH, textH) + 10);
}

int LTreeWidget::headerHeight() const
{
    if(!m_headerVisible){
        return 0;
    }
    const LFontMetrics fm = fontMetrics();
    return std::max<int>(24, (int)fm.height() + 10);
}

int LTreeWidget::columnX(int column) const
{
    int x = 0;
    for(int c = 0; c < column && c < (int)m_columnWidths.size(); ++c){
        x += columnWidth(c);
    }
    return x;
}

int LTreeWidget::columnAt(int x) const
{
    if(x < 0){
        return -1;
    }
    int acc = 0;
    for(int c = 0; c < m_columnCount; ++c){
        acc += columnWidth(c);
        if(x < acc){
            return c;
        }
    }
    return -1;
}

int LTreeWidget::contentWidth(int viewWidth) const
{
    return std::max<int>(viewWidth, columnX(m_columnCount));
}

int LTreeWidget::contentHeight(int viewWidth) const
{
    (void)viewWidth;
    return headerHeight() + visibleRowCount() * rowHeight();
}

LRect<int> LTreeWidget::rowRect(int row, int viewWidth) const
{
    return LRect<int>(0, row * rowHeight(), contentWidth(viewWidth), rowHeight(), false);
}

LRect<int> LTreeWidget::branchRect(int row) const
{
    std::shared_ptr<LTreeWidgetItem> item = itemAtRow(row);
    if(item == nullptr || item->isLeaf()){
        return LRect<int>();
    }
    const int x = 6 + item->depth() * m_indentation - 3;
    const int y = row * rowHeight() + (rowHeight() - 14) / 2;
    return LRect<int>(x, y, 14, 14, false);
}

int LTreeWidget::rowAt(const LPoint<int> &contentPos, int viewWidth) const
{
    (void)viewWidth;
    const int rh = rowHeight();
    if(rh <= 0 || contentPos.m_y < 0){
        return -1;
    }
    const int row = contentPos.m_y / rh;
    if(row < 0 || row >= (int)m_visible.size()){
        return -1;
    }
    return row;
}

LK_NAMESPACE_END
