#include "ltablewidget.h"
#include "lkwidgets/widget/llineedit.h"
#include "lkwidgets/util/leditbase.h"
#include "lkwidgets/draw/ldrawutil.h"
#include "lkgui/paint/lpainter.h"
#include "lkgui/paint/lline.h"
#include "lkgui/image/ltexture.h"
#include "lkgui/text/lfontmetrics.h"
#include "lkgui/lcursor.h"
#include "lkgui/lguiapplication.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <string>
#include <utility>

LK_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// LTableWidgetItem：一个 item 对应一个单元格
// ---------------------------------------------------------------------------
LTableWidgetItem::LTableWidgetItem(const std::wstring &text,
                                   std::shared_ptr<LTexture> icon,
                                   const LVariant &userData)
    : LActionBase(text, icon, userData)
{
    m_isListItem = true;
}

void LTableWidgetItem::setEditable(bool editable)
{
    m_editable = editable;
}

bool LTableWidgetItem::isEditable() const
{
    return m_editable;
}

void LTableWidgetItem::activate(ActionEvent event)
{
    LActionBase::activate(event);
    if(event == Trigger){
        sigTrigger.emt(getPtrT<LTableWidgetItem>());
    }
}

void LTableWidgetItem::triggerDoubleClick()
{
    sigDoubleClick.emt(getPtrT<LTableWidgetItem>());
}

// ---------------------------------------------------------------------------
// 单元格就地编辑框
// ---------------------------------------------------------------------------
class LTableCellEditor : public LLineEdit
{
public:
    explicit LTableCellEditor(std::shared_ptr<LWidget> parent)
        : LLineEdit(parent)
    {
    }

    LSignal<> sigCommit;
    LSignal<> sigCancel;

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
    int focusFrameWidth() const override { return 1; }

    void keyPressEvent(std::shared_ptr<LKeyEvent> e) override
    {
        const uint32_t key = e->key();
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

namespace {

// 视图不是 native 窗口，LWidget::setCursor 依赖 mouse-under 状态，动态切换常不生效；
// 用应用级覆盖光标，移到表头/行头分隔线上才能稳定看到调整形状
void applyTableCursor(LCursorShape shape)
{
    if(shape == LCursorShape::Arrow){
        LGuiApplication::restoreOverrideCursor(LCursor(LCursorShape::Arrow));
    }else{
        LGuiApplication::setOverrideCursor(LCursor(shape));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// 内部视图：绘制与鼠标命中
// ---------------------------------------------------------------------------
class LTableWidgetView : public LWidget
{
public:
    explicit LTableWidgetView(std::shared_ptr<LWidget> parent)
        : LWidget(parent)
    {
    }

    void updateScrollBars()
    {
        std::shared_ptr<LTableWidget> owner = parentT<LTableWidget>();
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        if(owner == nullptr || area == nullptr){
            return;
        }
        const int availW = std::max<int>(1, width() - owner->headerWidth());
        const int availH = std::max<int>(1, height() - owner->headerHeight());
        const int contentW = owner->columnX(owner->columnCount());
        const int contentH = owner->rowCount() * owner->rowHeight();

        std::shared_ptr<LScrollBar> vBar = area->verticalScrollBar();
        if(vBar != nullptr){
            vBar->setRange(0, std::max<int>(0, contentH - availH));
            vBar->setPageStep(availH);
            vBar->setSingleStep(std::max<int>(1, owner->rowHeight()));
            vBar->setSliderPosition(vBar->value());
        }
        std::shared_ptr<LScrollBar> hBar = area->horizontalScrollBar();
        if(hBar != nullptr){
            hBar->setRange(0, std::max<int>(0, contentW - availW));
            hBar->setPageStep(availW);
            hBar->setSingleStep(20);
            hBar->setSliderPosition(hBar->value());
        }
    }

    /** 视口坐标 -> 单元格内容坐标（不含行列头） */
    LPoint<int> toContent(const LPoint<int> &pos) const
    {
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        std::shared_ptr<LTableWidget> owner = parentT<LTableWidget>();
        LPoint<int> p = pos;
        if(area != nullptr){
            p.m_x += area->contentsOffset().m_x;
            p.m_y += area->contentsOffset().m_y;
        }
        if(owner != nullptr){
            p.m_x -= owner->headerWidth();
            p.m_y -= owner->headerHeight();
        }
        return p;
    }

    /** 命中列头分隔线（±3px）且该列为 Interactive 时返回列号 */
    int columnBoundaryAt(const LPoint<int> &pos)
    {
        std::shared_ptr<LTableWidget> owner = parentT<LTableWidget>();
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        if(owner == nullptr){
            return -1;
        }
        const int hh = owner->headerHeight();
        const int offX = area != nullptr ? area->contentsOffset().m_x : 0;
        if(hh <= 0 || pos.m_y >= hh || pos.m_x < owner->headerWidth()){
            return -1;
        }
        for(int c = 0; c < owner->columnCount(); ++c){
            const int bx = owner->headerWidth() + owner->columnX(c) - offX
                    + owner->columnWidth(c);
            if(std::abs(pos.m_x - bx) <= 3
                    && owner->columnResizeMode(c) != LTableResizeMode::Fixed){
                return c;
            }
        }
        return -1;
    }

    /** 命中行头分隔线（±3px）且该行为 Interactive 时返回行号 */
    int rowBoundaryAt(const LPoint<int> &pos)
    {
        std::shared_ptr<LTableWidget> owner = parentT<LTableWidget>();
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        if(owner == nullptr){
            return -1;
        }
        const int hw = owner->headerWidth();
        const int hh = owner->headerHeight();
        const int offY = area != nullptr ? area->contentsOffset().m_y : 0;
        if(hw <= 0 || pos.m_x >= hw || pos.m_y < hh){
            return -1;
        }
        for(int r = 0; r < owner->rowCount(); ++r){
            const int by = hh + owner->rowY(r) - offY + owner->rowHeight(r);
            if(std::abs(pos.m_y - by) <= 3
                    && owner->rowResizeMode(r) != LTableResizeMode::Fixed){
                return r;
            }
        }
        return -1;
    }

protected:
    void paintEvent(std::shared_ptr<LPaintEvent> e) override
    {
        (void)e;
        std::shared_ptr<LTableWidget> owner = parentT<LTableWidget>();
        if(owner == nullptr){
            return;
        }
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        const int offX = area != nullptr ? area->contentsOffset().m_x : 0;
        const int offY = area != nullptr ? area->contentsOffset().m_y : 0;
        const int vw = width();
        const int vh = height();
        const int hw = owner->headerWidth();
        const int hh = owner->headerHeight();

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
        const LColor headerC = pal->brush(LPalette::Button)->m_color;
        // 表头/行头分隔线用略深的颜色，1px 即可，便于看清列边界
        const LColor headLineC(mix(baseC.m_red, textC.m_red, 0.36),
                               mix(baseC.m_green, textC.m_green, 0.36),
                               mix(baseC.m_blue, textC.m_blue, 0.36));

        auto fillRect = [&p](const LRect<int> &r, const LColor &c) {
            p.setBrush(c, LBrush::SolidPattern);
            p.setPen(LPenStyle::No);
            p.drawRect(r);
        };
        auto strokeRect = [&p](const LRect<int> &r, const LColor &c) {
            p.setPen(c, LPenStyle::Solid, 1);
            p.drawLine(LLine<int>(r.m_x, r.m_y, r.x2(), r.m_y));
            p.drawLine(LLine<int>(r.m_x, r.y2(), r.x2(), r.y2()));
            p.drawLine(LLine<int>(r.m_x, r.m_y, r.m_x, r.y2()));
            p.drawLine(LLine<int>(r.x2(), r.m_y, r.x2(), r.y2()));
        };

        // ---- 列头 ----
        if(hh > 0){
            p.save();
            p.setClipRect(LRect<int>(hw, 0, std::max<int>(1, vw - hw), hh, false),
                          LClipOper::Intersect);
            fillRect(LRect<int>(hw, 0, vw - hw, hh, false), headerC);
            const double hBaseline = (hh - fm.height()) / 2.0 + fm.ascent();
            for(int c = 0; c < owner->columnCount(); ++c){
                const int x = hw + owner->columnX(c) - offX;
                const int w = owner->columnWidth(c);
                if(x + w <= hw || x >= vw){
                    continue;
                }
                // 分隔线画在每列右边界
                const int bx = x + w;
                p.setPen(headLineC, LPenStyle::Solid, 1);
                p.drawLine(LLine<int>(bx, 0, bx, hh));
                p.setPen(textC, LPenStyle::Solid, 1);
                p.save();
                p.setClipRect(LRect<int>(x, 0, w, hh, false), LClipOper::Intersect);
                p.drawText(LPoint<double>(x + 8, hBaseline),
                           owner->horizontalHeaderLabel(c));
                p.restore();
            }
            p.setPen(borderC, LPenStyle::Solid, 1);
            p.drawLine(LLine<int>(hw, hh - 1, vw, hh - 1));
            p.restore();
        }

        // ---- 行头 ----
        if(hw > 0){
            p.save();
            p.setClipRect(LRect<int>(0, hh, hw, std::max<int>(1, vh - hh), false),
                          LClipOper::Intersect);
            fillRect(LRect<int>(0, hh, hw, vh - hh, false), headerC);
            for(int r = 0; r < owner->rowCount(); ++r){
                const int rh = owner->rowHeight(r);
                const int y = hh + owner->rowY(r) - offY;
                if(y + rh <= hh || y >= vh){
                    continue;
                }
                const double hBaseline = (rh - fm.height()) / 2.0 + fm.ascent();
                // 行分隔线画在每行下边界
                const int by = y + rh;
                p.setPen(headLineC, LPenStyle::Solid, 1);
                p.drawLine(LLine<int>(0, by, hw, by));
                p.setPen(textC, LPenStyle::Solid, 1);
                p.save();
                p.setClipRect(LRect<int>(0, y, hw, rh, false), LClipOper::Intersect);
                p.drawText(LPoint<double>(6, y + hBaseline),
                           owner->verticalHeaderLabel(r));
                p.restore();
            }
            p.setPen(borderC, LPenStyle::Solid, 1);
            p.drawLine(LLine<int>(hw - 1, hh, hw - 1, vh));
            p.restore();
        }

        // ---- 左上角 ----
        if(hw > 0 && hh > 0){
            p.save();
            fillRect(LRect<int>(0, 0, hw, hh, false), headerC);
            strokeRect(LRect<int>(0, 0, hw, hh, false), borderC);
            p.restore();
        }

        // ---- 单元格（合并区只由左上角绘制一次）----
        p.save();
        p.setClipRect(LRect<int>(hw, hh, std::max<int>(1, vw - hw),
                                 std::max<int>(1, vh - hh), false),
                      LClipOper::Intersect);
        for(int row = 0; row < owner->rowCount(); ++row){
            for(int col = 0; col < owner->columnCount(); ++col){
                LRect<int> vr = owner->visualCellRect(row, col);
                if(!vr.isValid()){
                    continue;   // 被合并区覆盖
                }
                LRect<int> r(vr.m_x + hw - offX, vr.m_y + hh - offY,
                             vr.m_width, vr.m_height, false);
                if(r.y2() < hh || r.m_y > vh || r.x2() < hw || r.m_x > vw){
                    continue;
                }

                p.save();
                const bool selected = (row == owner->currentRow()
                                       && col == owner->currentColumn());
                const bool hover = (row == m_hoverRow && col == m_hoverColumn);
                uint16_t itemState = (uint16_t)LUiState::Enabled;
                if(selected){
                    itemState |= (uint16_t)LUiState::Selected;
                }
                if(hover){
                    itemState |= (uint16_t)LUiState::Hover;
                }
                std::shared_ptr<LRenderRule> rule =
                        owner->elementStyle().getRenderRule(LUiElementType::Item, itemState);
                bool hasBackground = false;
                if(rule != nullptr && rule->hasDrawable()){
                    rule->drawRule(&p, r);
                    hasBackground = true;
                }
                if(!hasBackground && selected){
                    fillRect(r.adjusted(1, 1, -1, -1), selC);
                }
                strokeRect(r, gridC);

                bool hasForeground = false;
                if(rule != nullptr){
                    hasForeground = (rule->applyForeground(&p) & 0x01) != 0;
                }
                if(!hasForeground){
                    p.setPen(textC, LPenStyle::Solid, 1);
                }

                std::shared_ptr<LTableWidgetItem> item = owner->item(row, col);
                if(item != nullptr){
                    const double baseline = r.m_y + (r.m_height - fm.height()) / 2.0
                            + fm.ascent();
                    double tx = r.m_x + 6;
                    if(item->icon() != nullptr){
                        const double iy = r.m_y + (r.m_height - 16) / 2.0;
                        p.drawPixmap(LPoint<double>(tx, iy),
                                     item->icon()->getPixmap(LSize<int>(16, 16),
                                                             LTexture::Normal, true));
                        tx += 20;
                    }
                    p.save();
                    p.setClipRect(r, LClipOper::Intersect);
                    p.drawText(LPoint<double>(tx, baseline), item->text());
                    p.restore();
                }
                p.restore();
            }
        }
        p.restore();
    }

    void resizeEvent(std::shared_ptr<LResizeEvent> e) override
    {
        // 视口尺寸变化后，Stretch / 最后一列拉伸需要重新分配
        if(std::shared_ptr<LTableWidget> owner = parentT<LTableWidget>()){
            owner->onItemChanged();
        }else{
            updateScrollBars();
        }
        LWidget::resizeEvent(e);
    }

    void mouseMoveEvent(std::shared_ptr<LMouseEvent> e) override
    {
        std::shared_ptr<LTableWidget> owner = parentT<LTableWidget>();
        if(owner == nullptr){
            e->ignore();
            return;
        }
        // 拖动表头分隔线调整列宽 / 行高
        if(m_dragColumn >= 0){
            owner->setColumnWidth(m_dragColumn,
                                  m_dragStartSize + (e->pos().m_x - m_dragStartPos));
            e->accept();
            return;
        }
        if(m_dragRow >= 0){
            owner->setRowHeight(m_dragRow,
                                m_dragStartSize + (e->pos().m_y - m_dragStartPos));
            e->accept();
            return;
        }

        // 分隔线悬停：切换成调整光标
        const int boundaryColumn = columnBoundaryAt(e->pos());
        const int boundaryRow = boundaryColumn >= 0 ? -1 : rowBoundaryAt(e->pos());
        LCursorShape shape = LCursorShape::Arrow;
        if(boundaryColumn >= 0){
            shape = LCursorShape::SizeHor;
        }else if(boundaryRow >= 0){
            shape = LCursorShape::SizeVer;
        }
        if(shape != m_cursorShape){
            m_cursorShape = shape;
            applyTableCursor(shape);
        }

        int row = -1;
        int col = -1;
        if(e->pos().m_x >= owner->headerWidth()
                && e->pos().m_y >= owner->headerHeight()){
            LPoint<int> cell = owner->cellAt(toContent(e->pos()));
            row = cell.m_x;
            col = cell.m_y;
        }
        if(row != m_hoverRow || col != m_hoverColumn){
            m_hoverRow = row;
            m_hoverColumn = col;
            update();
        }
        LWidget::mouseMoveEvent(e);
    }

    void mousePressEvent(std::shared_ptr<LMouseEvent> e) override
    {
        std::shared_ptr<LTableWidget> owner = parentT<LTableWidget>();
        if(owner == nullptr || e->button() != LMouseButton::Left){
            e->ignore();
            return;
        }
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        const int offX = area != nullptr ? area->contentsOffset().m_x : 0;
        const int offY = area != nullptr ? area->contentsOffset().m_y : 0;
        const int hw = owner->headerWidth();
        const int hh = owner->headerHeight();
        const LPoint<int> pos = e->pos();

        // 拖动表头分隔线调整列宽 / 行高（仅 Interactive 列/行）
        const int dragCol = columnBoundaryAt(pos);
        if(dragCol >= 0){
            // 手动拖动即视为接管该列尺寸，改为 Interactive，避免被策略重算覆盖
            if(owner->columnResizeMode(dragCol) != LTableResizeMode::Interactive){
                owner->setColumnResizeMode(dragCol, LTableResizeMode::Interactive);
            }
            m_dragColumn = dragCol;
            m_dragStartPos = pos.m_x;
            m_dragStartSize = owner->columnWidth(dragCol);
            m_cursorShape = LCursorShape::SizeHor;
            applyTableCursor(LCursorShape::SizeHor);
            e->accept();
            return;
        }
        const int dragRow = rowBoundaryAt(pos);
        if(dragRow >= 0){
            if(owner->rowResizeMode(dragRow) != LTableResizeMode::Interactive){
                owner->setRowResizeMode(dragRow, LTableResizeMode::Interactive);
            }
            m_dragRow = dragRow;
            m_dragStartPos = pos.m_y;
            m_dragStartSize = owner->rowHeight(dragRow);
            m_cursorShape = LCursorShape::SizeVer;
            applyTableCursor(LCursorShape::SizeVer);
            e->accept();
            return;
        }

        // 列头 / 行头点击
        if(hh > 0 && pos.m_y < hh){
            if(hw > 0 && pos.m_x < hw){
                e->ignore();
                return;
            }
            const int c = owner->columnAt(pos.m_x - hw + offX);
            if(c >= 0){
                owner->sigHorizontalHeaderClicked.emt(c);
                e->accept();
                return;
            }
            e->ignore();
            return;
        }
        if(hw > 0 && pos.m_x < hw){
            const int r = owner->rowAt(pos.m_y - hh + offY);
            if(r >= 0){
                owner->sigVerticalHeaderClicked.emt(r);
                e->accept();
                return;
            }
            e->ignore();
            return;
        }

        LPoint<int> cell = owner->cellAt(toContent(pos));
        const int row = cell.m_x;
        const int col = cell.m_y;
        if(row < 0 || col < 0){
            e->ignore();
            return;
        }

        owner->setCurrentCell(row, col);
        owner->sigCellClicked.emt(row, col);
        std::shared_ptr<LTableWidgetItem> item = owner->item(row, col);
        if(item != nullptr){
            item->activate(LActionBase::Trigger);
        }

        // 与 LListWidget / LTreeWidget 一致：自行判定双击
        const auto now = std::chrono::steady_clock::now();
        const bool isDoubleClick = (row == m_lastRow && col == m_lastCol)
                && (now - m_lastTime < std::chrono::milliseconds(500));
        if(isDoubleClick){
            m_lastRow = -1;
            m_lastCol = -1;
            m_lastTime = {};
            owner->sigCellDoubleClicked.emt(row, col);
            if(item != nullptr){
                item->triggerDoubleClick();
            }
            if(owner->isCellEditable(row, col)){
                owner->editItem(row, col);
            }
        }else{
            m_lastRow = row;
            m_lastCol = col;
            m_lastTime = now;
        }
        e->accept();
    }

    void mouseReleaseEvent(std::shared_ptr<LMouseEvent> e) override
    {
        if(m_dragColumn >= 0 || m_dragRow >= 0){
            m_dragColumn = -1;
            m_dragRow = -1;
            m_cursorShape = LCursorShape::Arrow;
            applyTableCursor(LCursorShape::Arrow);
            e->accept();
            return;
        }
        LWidget::mouseReleaseEvent(e);
    }

    void leaveEvent(std::shared_ptr<LEvent> e) override
    {
        // 全局覆盖光标必须成对恢复，鼠标离开视图时还原
        if(m_cursorShape != LCursorShape::Arrow){
            m_cursorShape = LCursorShape::Arrow;
            applyTableCursor(LCursorShape::Arrow);
        }
        LWidget::leaveEvent(e);
    }

    int m_hoverRow = -1;
    int m_hoverColumn = -1;
    int m_lastRow = -1;
    int m_lastCol = -1;
    std::chrono::steady_clock::time_point m_lastTime{};

    int m_dragColumn = -1;
    int m_dragRow = -1;
    int m_dragStartPos = 0;
    int m_dragStartSize = 0;
    LCursorShape m_cursorShape = LCursorShape::Arrow;
};

// ---------------------------------------------------------------------------
// LTableWidget
// ---------------------------------------------------------------------------
LTableWidget::LTableWidget(std::shared_ptr<LWidget> parent)
    : LAbstractScrollArea(parent)
{
}

void LTableWidget::initSpecific()
{
    LAbstractScrollArea::initSpecific();
    setVerticalScrollBarPolicy(LScrollPolicy::AsNeeded);
    setHorizontalScrollBarPolicy(LScrollPolicy::AsNeeded);
    // 视图会跟随本控件的 mouse tracking，避免无按键的移动事件被丢弃
    setMouseTracking(true);
}

void LTableWidget::initViewport()
{
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    std::shared_ptr<LTableWidgetView> view = makeInit<LTableWidgetView>(q);
    view->setBackgroundRole(LPalette::Base);
    view->setAutoFillBackground(true);
    // 没有按键的鼠标移动默认会被丢弃，开了跟踪才能在悬停分隔线时切换光标/高亮
    view->setMouseTracking(true);
    view->addEventFilter(q);
    m_viewport = view;
    m_view = view;
}

LSize<int> LTableWidget::sizeHint() const
{
    return LSize<int>(480, 240);
}

void LTableWidget::resizeEvent(std::shared_ptr<LResizeEvent> e)
{
    LAbstractScrollArea::resizeEvent(e);
    // 控件尺寸变化后重新应用 Stretch / 最后一行铺开等策略
    refresh();
}

// ---- 网格 ----
int LTableWidget::indexOf(int row, int column) const
{
    return row * m_columnCount + column;
}

void LTableWidget::rebuildRowOffsets()
{
    m_rowOffsets.assign((size_t)m_rowCount + 1, 0);
    for(int r = 0; r < m_rowCount; ++r){
        m_rowOffsets[r + 1] = m_rowOffsets[r] + rowHeight(r);
    }
}

void LTableWidget::layoutSections()
{
    if(m_inLayout){
        return;
    }
    m_inLayout = true;

    // ResizeToContents：按内容重算列宽
    for(int c = 0; c < m_columnCount; ++c){
        if(m_columnModes[c] == LTableResizeMode::ResizeToContents){
            m_columnWidths[c] = columnContentWidth(c);
        }
    }

    // Stretch：把可用宽度平均分给 Stretch 列
    int stretchCount = 0;
    int usedWidth = 0;
    for(int c = 0; c < m_columnCount; ++c){
        if(m_columnModes[c] == LTableResizeMode::Stretch){
            ++stretchCount;
        }else{
            usedWidth += columnWidth(c);
        }
    }
    // 用控件宽度推算可用宽：viewport 的几何在布局过程中可能还没更新到最终值
    int viewW = width();
    if(std::shared_ptr<LScrollBar> vBar = verticalScrollBar()){
        if(vBar->isVisible()){
            viewW -= vBar->width();
        }
    }
    const int avail = std::max<int>(0, viewW - headerWidth());
    if(stretchCount > 0 && viewW > 0){
        const int each = std::max<int>(40, (avail - usedWidth) / stretchCount);
        for(int c = 0; c < m_columnCount; ++c){
            if(m_columnModes[c] == LTableResizeMode::Stretch){
                m_columnWidths[c] = each;
            }
        }
    }

    // 最后一列填满剩余宽度（无条件收敛，窗口变窄时也能缩回）
    if(m_stretchLastColumn && m_columnCount > 0 && viewW > 0){
        const int last = m_columnCount - 1;
        if(m_columnModes[last] != LTableResizeMode::Stretch
                && m_columnModes[last] != LTableResizeMode::ResizeToContents){
            const int target = std::max<int>(40, avail - columnX(last));
            if(m_columnWidths[last] != target){
                m_columnWidths[last] = target;
            }
        }
    }

    // ---- 行策略：ResizeToContents / Stretch ----
    bool rowSizesChanged = false;
    for(int r = 0; r < m_rowCount; ++r){
        if(m_rowModes[r] == LTableResizeMode::ResizeToContents){
            const int h = rowContentHeight(r);
            if(m_rowHeights[r] != h){
                m_rowHeights[r] = h;
                rowSizesChanged = true;
            }
        }
    }

    int stretchRows = 0;
    int usedHeight = 0;
    for(int r = 0; r < m_rowCount; ++r){
        if(m_rowModes[r] == LTableResizeMode::Stretch){
            ++stretchRows;
        }else{
            usedHeight += rowHeight(r);
        }
    }
    // 视口高度里已经扣掉横向滚动条，直接用视口高度
    const int viewH = m_viewport != nullptr ? m_viewport->height() : height();
    const int availH = std::max<int>(0, viewH - headerHeight());
    if(stretchRows > 0 && viewH > 0){
        const int each = std::max<int>(20, (availH - usedHeight) / stretchRows);
        for(int r = 0; r < m_rowCount; ++r){
            if(m_rowModes[r] == LTableResizeMode::Stretch && m_rowHeights[r] != each){
                m_rowHeights[r] = each;
                rowSizesChanged = true;
            }
        }
    }
    if(rowSizesChanged){
        rebuildRowOffsets();
    }

    // 最后一行填满剩余高度（无条件收敛，避免视口变大后留下过高的旧值）
    if(m_stretchLastRow && m_rowCount > 0 && viewH > 0){
        const int last = m_rowCount - 1;
        if(m_rowModes[last] != LTableResizeMode::Stretch
                && m_rowModes[last] != LTableResizeMode::ResizeToContents){
            const int target = std::max<int>(rowHeight(), availH - m_rowOffsets[last]);
            if(m_rowHeights[last] != target){
                m_rowHeights[last] = target;
                rebuildRowOffsets();
            }
        }
    }

    m_inLayout = false;
}

void LTableWidget::refresh()
{
    if(m_editRow >= 0){
        commitEdit();
    }
    layoutSections();
    if(std::shared_ptr<LTableWidgetView> view = m_view.lock()){
        view->updateScrollBars();
        view->update();
    }else if(m_viewport != nullptr){
        m_viewport->update();
    }
    update();
}

void LTableWidget::onItemChanged()
{
    refresh();
}

void LTableWidget::resizeGrid(int rows, int columns)
{
    rows = std::max<int>(0, rows);
    columns = std::max<int>(0, columns);
    if(rows == m_rowCount && columns == m_columnCount){
        return;
    }
    cancelEdit();

    const int oldRows = m_rowCount;
    const int oldCols = m_columnCount;
    std::vector<std::shared_ptr<LTableWidgetItem>> next(
            (size_t)rows * (size_t)columns, nullptr);
    for(int r = 0; r < std::min<int>(rows, oldRows); ++r){
        for(int c = 0; c < std::min<int>(columns, oldCols); ++c){
            next[(size_t)r * columns + c] = m_items[indexOf(r, c)];
        }
    }
    m_items = std::move(next);
    m_rowCount = rows;
    m_columnCount = columns;

    m_columnWidths.resize(columns);
    m_columnModes.resize(columns, LTableResizeMode::Interactive);
    for(int c = oldCols; c < columns; ++c){
        m_columnWidths[c] = 100;
        m_columnModes[c] = LTableResizeMode::Interactive;
    }
    m_rowHeights.resize(rows, rowHeight());
    m_rowModes.resize(rows, LTableResizeMode::Interactive);
    m_columnEditable.resize(columns, 1);
    m_rowEditable.resize(rows, 1);
    m_hHeaders.resize(columns);
    m_vHeaders.resize(rows);
    rebuildRowOffsets();

    // 丢弃超出新网格的合并区
    m_spans.erase(std::remove_if(m_spans.begin(), m_spans.end(), [&](const Span &s) {
        return s.row + s.rowSpan > rows || s.column + s.columnSpan > columns;
    }), m_spans.end());

    if(m_currentRow >= rows || m_currentColumn >= columns){
        m_currentRow = -1;
        m_currentColumn = -1;
    }
    refresh();
}

int LTableWidget::rowCount() const
{
    return m_rowCount;
}

int LTableWidget::columnCount() const
{
    return m_columnCount;
}

void LTableWidget::setRowCount(int rows)
{
    resizeGrid(rows, m_columnCount);
}

void LTableWidget::setColumnCount(int columns)
{
    resizeGrid(m_rowCount, columns);
}

void LTableWidget::setRowColumnCount(int rows, int columns)
{
    resizeGrid(rows, columns);
}

void LTableWidget::setItem(int row, int column,
                           const std::shared_ptr<LTableWidgetItem> &item)
{
    if(row < 0 || column < 0 || row >= m_rowCount || column >= m_columnCount){
        return;
    }
    m_items[indexOf(row, column)] = item;
    refresh();
}

std::shared_ptr<LTableWidgetItem> LTableWidget::item(int row, int column) const
{
    if(row < 0 || column < 0 || row >= m_rowCount || column >= m_columnCount){
        return nullptr;
    }
    return m_items[indexOf(row, column)];
}

std::shared_ptr<LTableWidgetItem> LTableWidget::ensureItem(int row, int column)
{
    std::shared_ptr<LTableWidgetItem> it = item(row, column);
    if(it == nullptr){
        it = std::make_shared<LTableWidgetItem>();
        m_items[indexOf(row, column)] = it;
    }
    return it;
}

void LTableWidget::setItemText(int row, int column, const std::wstring &text)
{
    if(row < 0 || column < 0 || row >= m_rowCount || column >= m_columnCount){
        return;
    }
    std::shared_ptr<LTableWidgetItem> it = ensureItem(row, column);
    if(it->text() != text){
        it->setText(text);
        onItemChanged();
    }
}

std::wstring LTableWidget::itemText(int row, int column) const
{
    std::shared_ptr<LTableWidgetItem> it = item(row, column);
    return it != nullptr ? it->text() : std::wstring();
}

void LTableWidget::clearContents()
{
    cancelEdit();
    for(auto &it : m_items){
        it.reset();
    }
    refresh();
}

void LTableWidget::clear()
{
    clearContents();
    m_spans.clear();
    m_currentRow = -1;
    m_currentColumn = -1;
    refresh();
}

// ---- 合并 ----
const LTableWidget::Span *LTableWidget::spanCovering(int row, int column) const
{
    for(const Span &s : m_spans){
        if(row >= s.row && row < s.row + s.rowSpan
                && column >= s.column && column < s.column + s.columnSpan){
            return &s;
        }
    }
    return nullptr;
}

const LTableWidget::Span *LTableWidget::spanStartingAt(int row, int column) const
{
    for(const Span &s : m_spans){
        if(s.row == row && s.column == column){
            return &s;
        }
    }
    return nullptr;
}

bool LTableWidget::spanOf(int row, int column, int &originRow, int &originColumn,
                          int &rowSpan, int &columnSpan) const
{
    const Span *s = spanCovering(row, column);
    if(s == nullptr){
        originRow = row;
        originColumn = column;
        rowSpan = 1;
        columnSpan = 1;
        return false;
    }
    originRow = s->row;
    originColumn = s->column;
    rowSpan = s->rowSpan;
    columnSpan = s->columnSpan;
    return true;
}

LPoint<int> LTableWidget::originAt(int row, int column) const
{
    const Span *s = spanCovering(row, column);
    if(s == nullptr){
        return LPoint<int>(row, column);
    }
    return LPoint<int>(s->row, s->column);
}

bool LTableWidget::isSpanned(int row, int column) const
{
    const Span *s = spanCovering(row, column);
    return s != nullptr && (s->row != row || s->column != column);
}

bool LTableWidget::isSpanOrigin(int row, int column) const
{
    const Span *s = spanStartingAt(row, column);
    return s != nullptr && (s->rowSpan > 1 || s->columnSpan > 1);
}

bool LTableWidget::mergeCells(int row, int column, int rowSpan, int columnSpan)
{
    if(row < 0 || column < 0 || rowSpan < 1 || columnSpan < 1
            || row + rowSpan > m_rowCount || column + columnSpan > m_columnCount){
        return false;
    }
    // 与区域外的已有合并区相交则失败
    for(const Span &s : m_spans){
        const bool startsInside = s.row >= row && s.row < row + rowSpan
                && s.column >= column && s.column < column + columnSpan;
        const bool intersects = s.row < row + rowSpan && s.row + s.rowSpan > row
                && s.column < column + columnSpan && s.column + s.columnSpan > column;
        if(intersects && !startsInside){
            return false;
        }
    }
    // 移除起点落在区域内的旧合并区（允许在同一区域重新合并）
    m_spans.erase(std::remove_if(m_spans.begin(), m_spans.end(), [&](const Span &s) {
        return s.row >= row && s.row < row + rowSpan
                && s.column >= column && s.column < column + columnSpan;
    }), m_spans.end());
    if(rowSpan > 1 || columnSpan > 1){
        m_spans.push_back(Span{row, column, rowSpan, columnSpan});
    }
    if(m_currentRow >= 0 && m_currentColumn >= 0){
        LPoint<int> o = originAt(m_currentRow, m_currentColumn);
        m_currentRow = o.m_x;
        m_currentColumn = o.m_y;
    }
    cancelEdit();
    refresh();
    return true;
}

bool LTableWidget::splitCells(int row, int column)
{
    for(auto it = m_spans.begin(); it != m_spans.end(); ++it){
        if(it->row == row && it->column == column){
            m_spans.erase(it);
            refresh();
            return true;
        }
    }
    return false;
}

void LTableWidget::clearSpans()
{
    if(!m_spans.empty()){
        m_spans.clear();
        refresh();
    }
}

// ---- 表头 ----
void LTableWidget::setHorizontalHeaderLabels(const std::vector<std::wstring> &labels)
{
    m_hHeaders = labels;
    m_hHeaders.resize(m_columnCount);
    refresh();
}

void LTableWidget::setVerticalHeaderLabels(const std::vector<std::wstring> &labels)
{
    m_vHeaders = labels;
    m_vHeaders.resize(m_rowCount);
    refresh();
}

std::wstring LTableWidget::horizontalHeaderLabel(int column) const
{
    if(column < 0 || column >= (int)m_hHeaders.size()){
        return std::wstring();
    }
    if(!m_hHeaders[column].empty()){
        return m_hHeaders[column];
    }
    return L"列 " + std::to_wstring(column + 1);
}

std::wstring LTableWidget::verticalHeaderLabel(int row) const
{
    if(row < 0 || row >= (int)m_vHeaders.size()){
        return std::wstring();
    }
    if(!m_vHeaders[row].empty()){
        return m_vHeaders[row];
    }
    return std::to_wstring(row + 1);
}

void LTableWidget::setHeaderVisible(bool visible)
{
    if(m_headerVisible != visible){
        m_headerVisible = visible;
        refresh();
    }
}

bool LTableWidget::isHeaderVisible() const
{
    return m_headerVisible;
}

// ---- 尺寸：手动设置 / 策略 / 按内容自适应 ----
void LTableWidget::setRowHeight(int height)
{
    m_rowHeight = std::max<int>(20, height);
    for(int r = 0; r < (int)m_rowHeights.size(); ++r){
        m_rowHeights[r] = m_rowHeight;
    }
    rebuildRowOffsets();
    refresh();
}

void LTableWidget::setRowHeight(int row, int height)
{
    if(row < 0 || row >= m_rowCount){
        return;
    }
    m_rowHeights[row] = std::max<int>(20, height);
    rebuildRowOffsets();
    refresh();
}

int LTableWidget::rowHeight() const
{
    return std::max<int>(20, m_rowHeight);
}

int LTableWidget::rowHeight(int row) const
{
    if(row < 0 || row >= (int)m_rowHeights.size()){
        return rowHeight();
    }
    return std::max<int>(20, m_rowHeights[row]);
}

void LTableWidget::setColumnWidth(int column, int width)
{
    if(column < 0 || column >= m_columnCount){
        return;
    }
    m_columnWidths[column] = std::max<int>(20, width);
    refresh();
}

int LTableWidget::columnWidth(int column) const
{
    if(column < 0 || column >= (int)m_columnWidths.size()){
        return 0;
    }
    return std::max<int>(20, m_columnWidths[column]);
}

void LTableWidget::setColumnResizeMode(int column, LTableResizeMode mode)
{
    if(column < 0 || column >= m_columnCount){
        return;
    }
    m_columnModes[column] = mode;
    refresh();
}

void LTableWidget::setColumnResizeMode(LTableResizeMode mode)
{
    for(int c = 0; c < m_columnCount; ++c){
        m_columnModes[c] = mode;
    }
    refresh();
}

LTableResizeMode LTableWidget::columnResizeMode(int column) const
{
    if(column < 0 || column >= (int)m_columnModes.size()){
        return LTableResizeMode::Interactive;
    }
    return m_columnModes[column];
}

void LTableWidget::setRowResizeMode(int row, LTableResizeMode mode)
{
    if(row < 0 || row >= m_rowCount){
        return;
    }
    m_rowModes[row] = mode;
    refresh();
}

void LTableWidget::setRowResizeMode(LTableResizeMode mode)
{
    for(int r = 0; r < m_rowCount; ++r){
        m_rowModes[r] = mode;
    }
    refresh();
}

LTableResizeMode LTableWidget::rowResizeMode(int row) const
{
    if(row < 0 || row >= (int)m_rowModes.size()){
        return LTableResizeMode::Interactive;
    }
    return m_rowModes[row];
}

void LTableWidget::setStretchLastColumn(bool stretch)
{
    if(m_stretchLastColumn != stretch){
        m_stretchLastColumn = stretch;
        refresh();
    }
}

bool LTableWidget::stretchLastColumn() const
{
    return m_stretchLastColumn;
}

void LTableWidget::setStretchLastRow(bool stretch)
{
    if(m_stretchLastRow != stretch){
        m_stretchLastRow = stretch;
        refresh();
    }
}

bool LTableWidget::stretchLastRow() const
{
    return m_stretchLastRow;
}

int LTableWidget::columnContentWidth(int column) const
{
    if(column < 0 || column >= m_columnCount){
        return 0;
    }
    const LFontMetrics fm = fontMetrics();
    int maxW = 40;
    for(int r = 0; r < m_rowCount; ++r){
        if(isSpanned(r, column)){
            continue;   // 被合并区覆盖的单元格不参与
        }
        std::shared_ptr<LTableWidgetItem> it = item(r, column);
        if(it == nullptr){
            continue;
        }
        int w = (int)fm.horizontalAdvance(it->text()) + 16;
        if(it->icon() != nullptr){
            w += 20;
        }
        maxW = std::max(maxW, w);
    }
    return maxW;
}

int LTableWidget::rowContentHeight(int row) const
{
    (void)row;
    const LFontMetrics fm = fontMetrics();
    return std::max<int>(24, (int)fm.height() + 10);
}

void LTableWidget::resizeColumnToContents(int column)
{
    if(column < 0 || column >= m_columnCount){
        return;
    }
    m_columnWidths[column] = columnContentWidth(column);
    refresh();
}

void LTableWidget::resizeColumnsToContents()
{
    for(int c = 0; c < m_columnCount; ++c){
        m_columnWidths[c] = columnContentWidth(c);
    }
    // 跨列合并区放不下时，把差额补到该区最后一列
    const LFontMetrics fm = fontMetrics();
    for(const Span &s : m_spans){
        if(s.columnSpan <= 1){
            continue;
        }
        std::shared_ptr<LTableWidgetItem> it = item(s.row, s.column);
        if(it == nullptr){
            continue;
        }
        int need = (int)fm.horizontalAdvance(it->text()) + 16;
        if(it->icon() != nullptr){
            need += 20;
        }
        int cur = 0;
        for(int k = 0; k < s.columnSpan; ++k){
            cur += columnWidth(s.column + k);
        }
        if(need > cur){
            m_columnWidths[s.column + s.columnSpan - 1] += need - cur;
        }
    }
    refresh();
}

void LTableWidget::resizeRowToContents(int row)
{
    if(row < 0 || row >= m_rowCount){
        return;
    }
    m_rowHeights[row] = rowContentHeight(row);
    rebuildRowOffsets();
    refresh();
}

void LTableWidget::resizeRowsToContents()
{
    for(int r = 0; r < m_rowCount; ++r){
        m_rowHeights[r] = rowContentHeight(r);
    }
    rebuildRowOffsets();
    refresh();
}

int LTableWidget::headerHeight() const
{
    if(!m_headerVisible){
        return 0;
    }
    const LFontMetrics fm = fontMetrics();
    return std::max<int>(24, (int)fm.height() + 10);
}

int LTableWidget::headerWidth() const
{
    if(!m_headerVisible){
        return 0;
    }
    const LFontMetrics fm = fontMetrics();
    const int digits = (int)fm.horizontalAdvance(std::to_wstring(std::max<int>(1, m_rowCount)));
    return std::max<int>(36, digits + 16);
}

// ---- 就地编辑与可编辑性 ----
void LTableWidget::setEditable(bool editable)
{
    m_editable = editable;
    if(!editable){
        cancelEdit();
    }
}

bool LTableWidget::isEditable() const
{
    return m_editable;
}

void LTableWidget::setRowEditable(int row, bool editable)
{
    if(row < 0 || row >= m_rowCount){
        return;
    }
    m_rowEditable[row] = editable ? 1 : 0;
    if(!editable && m_editRow == row){
        cancelEdit();
    }
}

bool LTableWidget::isRowEditable(int row) const
{
    if(row < 0 || row >= (int)m_rowEditable.size()){
        return false;
    }
    return m_rowEditable[row] != 0;
}

void LTableWidget::setColumnEditable(int column, bool editable)
{
    if(column < 0 || column >= m_columnCount){
        return;
    }
    m_columnEditable[column] = editable ? 1 : 0;
    if(!editable && m_editColumn == column){
        cancelEdit();
    }
}

bool LTableWidget::isColumnEditable(int column) const
{
    if(column < 0 || column >= (int)m_columnEditable.size()){
        return false;
    }
    return m_columnEditable[column] != 0;
}

bool LTableWidget::isCellEditable(int row, int column) const
{
    if(!m_editable || row < 0 || column < 0
            || row >= m_rowCount || column >= m_columnCount){
        return false;
    }
    // 合并区只从左上角编辑
    if(isSpanned(row, column)){
        return false;
    }
    if(!isRowEditable(row) || !isColumnEditable(column)){
        return false;
    }
    std::shared_ptr<LTableWidgetItem> it = item(row, column);
    return it == nullptr || it->isEditable();
}

void LTableWidget::editItem(int row, int column)
{
    if(!isCellEditable(row, column) || m_viewport == nullptr){
        return;
    }
    if(m_editRow >= 0){
        commitEdit();
    }

    std::shared_ptr<LTableCellEditor> editor =
            std::dynamic_pointer_cast<LTableCellEditor>(m_editor);
    if(editor == nullptr){
        editor = makeInit<LTableCellEditor>(m_viewport);
        editor->setName("表格单元格编辑器");
        editor->sigCommit.connect(this, &LTableWidget::commitEdit);
        editor->sigCancel.connect(this, &LTableWidget::cancelEdit);
        m_editor = editor;
    }

    m_editRow = row;
    m_editColumn = column;
    std::shared_ptr<LTableWidgetItem> it = item(row, column);
    editor->setText(it != nullptr ? it->text() : std::wstring());

    // 合并区编辑时覆盖整个合并后的矩形；宽度严格贴合单元格，避免盖到相邻列
    const LPoint<int> offset = contentsOffset();
    LRect<int> vr = visualCellRect(row, column);
    const int editH = std::max<int>(22, vr.m_height);
    LRect<int> r(vr.m_x + headerWidth() - offset.m_x,
                 vr.m_y + headerHeight() - offset.m_y - (editH - vr.m_height) / 2,
                 vr.m_width, editH, false);
    editor->setGeometry(r);
    editor->setVisible(true);
    editor->setFocus();
    editor->moveCursorToEnd();
    m_viewport->update();
}

void LTableWidget::commitEdit()
{
    if(m_editor == nullptr || m_editRow < 0){
        return;
    }
    std::shared_ptr<LTableCellEditor> editor =
            std::dynamic_pointer_cast<LTableCellEditor>(m_editor);
    const int row = m_editRow;
    const int column = m_editColumn;
    m_editRow = -1;
    m_editColumn = -1;
    m_editor->setVisible(false);

    if(editor != nullptr){
        const std::wstring text = editor->text();
        std::shared_ptr<LTableWidgetItem> it = item(row, column);
        if(it == nullptr){
            if(!text.empty()){
                it = ensureItem(row, column);
                it->setText(text);
                sigCellChanged.emt(it, row, column);
            }
        }else if(it->text() != text){
            it->setText(text);
            sigCellChanged.emt(it, row, column);
        }
    }
    if(m_viewport != nullptr){
        m_viewport->update();
    }
    update();
}

void LTableWidget::cancelEdit()
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

void LTableWidget::scrollContentsBy(int dx, int dy)
{
    if(m_editRow >= 0){
        commitEdit();
    }
    LAbstractScrollArea::scrollContentsBy(dx, dy);
}

// ---- 选中 ----
LPoint<int> LTableWidget::currentCell() const
{
    return LPoint<int>(m_currentRow, m_currentColumn);
}

int LTableWidget::currentRow() const
{
    return m_currentRow;
}

int LTableWidget::currentColumn() const
{
    return m_currentColumn;
}

std::shared_ptr<LTableWidgetItem> LTableWidget::currentItem() const
{
    return item(m_currentRow, m_currentColumn);
}

void LTableWidget::setCurrentCell(int row, int column)
{
    if(row < -1 || column < -1 || row >= m_rowCount || column >= m_columnCount){
        return;
    }
    if(row >= 0 && column >= 0){
        LPoint<int> o = originAt(row, column);
        row = o.m_x;
        column = o.m_y;
    }
    if(m_currentRow == row && m_currentColumn == column){
        return;
    }
    m_currentRow = row;
    m_currentColumn = column;
    sigCurrentCellChanged.emt(row, column);
    if(m_viewport != nullptr){
        m_viewport->update();
    }
}

// ---- 几何（内容坐标）----
int LTableWidget::columnX(int column) const
{
    int x = 0;
    for(int c = 0; c < column && c < (int)m_columnWidths.size(); ++c){
        x += columnWidth(c);
    }
    return x;
}

int LTableWidget::rowY(int row) const
{
    if(row <= 0){
        return 0;
    }
    if(row >= (int)m_rowOffsets.size()){
        return m_rowOffsets.empty() ? 0 : m_rowOffsets.back();
    }
    return m_rowOffsets[row];
}

int LTableWidget::columnAt(int x) const
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

int LTableWidget::rowAt(int y) const
{
    if(y < 0 || m_rowOffsets.size() < 2){
        return -1;
    }
    auto it = std::upper_bound(m_rowOffsets.begin(), m_rowOffsets.end(), y);
    if(it == m_rowOffsets.begin() || it == m_rowOffsets.end()){
        return -1;
    }
    const int row = (int)(it - m_rowOffsets.begin()) - 1;
    if(row < 0 || row >= m_rowCount){
        return -1;
    }
    return row;
}

int LTableWidget::contentWidth(int viewWidth) const
{
    return std::max<int>(viewWidth, columnX(m_columnCount));
}

int LTableWidget::contentHeight(int viewWidth) const
{
    (void)viewWidth;
    return m_rowOffsets.empty() ? 0 : m_rowOffsets.back();
}

LRect<int> LTableWidget::cellRect(int row, int column) const
{
    return LRect<int>(columnX(column), rowY(row),
                      columnWidth(column), rowHeight(row), false);
}

LRect<int> LTableWidget::visualCellRect(int row, int column) const
{
    if(row < 0 || column < 0 || row >= m_rowCount || column >= m_columnCount){
        return LRect<int>();
    }
    const Span *s = spanCovering(row, column);
    if(s == nullptr){
        return cellRect(row, column);
    }
    if(s->row != row || s->column != column){
        return LRect<int>();   // 被合并区覆盖
    }
    const LRect<int> a = cellRect(s->row, s->column);
    const LRect<int> b = cellRect(s->row + s->rowSpan - 1,
                                  s->column + s->columnSpan - 1);
    return LRect<int>(a.m_x, a.m_y, b.x2() - a.m_x, b.y2() - a.m_y, false);
}

LPoint<int> LTableWidget::cellAt(const LPoint<int> &contentPos) const
{
    const int col = columnAt(contentPos.m_x);
    const int row = rowAt(contentPos.m_y);
    if(row < 0 || col < 0){
        return LPoint<int>(-1, -1);
    }
    LPoint<int> o = originAt(row, col);
    if(!visualCellRect(o.m_x, o.m_y).contains(contentPos)){
        return LPoint<int>(-1, -1);
    }
    return o;
}

LK_NAMESPACE_END
