#include "llistwidget.h"
#include "lkwidgets/widget/llineedit.h"
#include "lkwidgets/draw/ldrawutil.h"
#include "lkgui/paint/lpainter.h"
#include "lkgui/image/ltexture.h"
#include "lkgui/text/lfontmetrics.h"
#include <algorithm>
#include <chrono>
#include <cmath>

LK_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// LListWidgetItem
// ---------------------------------------------------------------------------
LListWidgetItem::LListWidgetItem(const std::wstring &text,
                                 std::shared_ptr<LTexture> icon,
                                 const LVariant &userData)
    : LActionBase(text, icon, userData)
{
    m_isListItem = true;
}

void LListWidgetItem::activate(ActionEvent event)
{
    LActionBase::activate(event);
    if(event == Trigger){
        sigTrigger.emt(getPtrT<LListWidgetItem>());
    }
}

void LListWidgetItem::triggerDoubleClick()
{
    sigDoubleClick.emt(getPtrT<LListWidgetItem>());
}

// ---------------------------------------------------------------------------
// 就地编辑框：回车提交、Esc 取消、失焦提交
// ---------------------------------------------------------------------------
class LListItemEditor : public LLineEdit
{
public:
    explicit LListItemEditor(std::shared_ptr<LWidget> parent)
        : LLineEdit(parent)
    {
    }

    LSignal<> sigCommit;
    LSignal<> sigCancel;

    /** 把光标移到文本末尾；setText 后光标停在开头，直接输入会插到最前面 */
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
    // 就地编辑框只覆盖一行文字，焦点描边用 1px，避免看起来比列表项厚重
    int focusFrameWidth() const override { return 1; }

    void keyPressEvent(std::shared_ptr<LKeyEvent> e) override
    {
        const uint32_t key = e->key();
        // 0x01000004 = Return，0x01000005 = Enter，0x01000000 = Escape
        if(key == 0x01000004 || key == 0x01000005){
            e->accept();
            sigCommit.emt();
            return;
        }
        if(key == 0x01000000){
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
// 内部视图：负责绘制与鼠标命中
// ---------------------------------------------------------------------------
class LListWidgetView : public LWidget
{
public:
    explicit LListWidgetView(std::shared_ptr<LWidget> parent)
        : LWidget(parent)
    {
    }

    void updateScrollBars()
    {
        std::shared_ptr<LListWidget> owner = parentT<LListWidget>();
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        if(owner == nullptr || area == nullptr){
            return;
        }

        const int contentH = owner->contentHeight(width());
        const int viewH = height();

        std::shared_ptr<LScrollBar> vBar = area->verticalScrollBar();
        vBar->setRange(0, std::max(0, contentH - viewH));
        vBar->setPageStep(std::max(1, viewH));
        vBar->setSingleStep(std::max(1, owner->rowHeight()));
        vBar->setSliderPosition(vBar->value());

        std::shared_ptr<LScrollBar> hBar = area->horizontalScrollBar();
        hBar->setRange(0, 0);
        hBar->setPageStep(std::max(1, width()));
    }

protected:
    void paintEvent(std::shared_ptr<LPaintEvent> e) override
    {
        (void)e;
        std::shared_ptr<LListWidget> owner = parentT<LListWidget>();
        if(owner == nullptr){
            return;
        }
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        const int offsetY = area != nullptr ? area->contentsOffset().m_y : 0;

        LPainter p(paintDevice());
        std::shared_ptr<LPalette> pal = palette();

        p.fillRect(rect().toRectF(), pal->brush(LPalette::Base));

        const int viewWidth = width();
        const int count = owner->count();
        const int current = owner->currentRow();

        for(int i = 0; i < count; ++i){
            LRect<int> r = owner->itemRect(i, viewWidth);
            r.translate(0, -offsetY);
            if(r.y2() < 0 || r.m_y > height()){
                continue;
            }

            p.save();
            p.setRenderHints(LRenderHint::Antialiasing, true);

            // 样式表：按 LListWidget::item 的规则绘制（支持 :hover / :selected 伪状态）
            uint16_t itemState = (uint16_t)LUiState::Enabled;
            if(i == current){
                itemState |= (uint16_t)LUiState::Selected;
            }
            if(i == m_hoverRow){
                itemState |= (uint16_t)LUiState::Hover;
            }

            std::shared_ptr<LRenderRule> rule =
                    owner->elementStyle().getRenderRule(LUiElementType::Item, itemState);
            bool hasBackground = false;
            bool hasForeground = false;
            if(rule != nullptr){
                if(rule->hasDrawable()){
                    rule->drawRule(&p, r);
                    hasBackground = true;
                }
                hasForeground = (rule->applyForeground(&p) & 0x01) != 0;
            }

            // 没有样式规则时使用默认的选中高亮
            if(!hasBackground && i == current){
                const LColor hl = pal->brush(LPalette::Highlight)->m_color;
                const LColor base = pal->brush(LPalette::Base)->m_color;
                const auto mix = [](uint8_t a, uint8_t b) {
                    return uint8_t(a + (int(b) - int(a)) * 0.18);
                };
                const LColor bg(mix(base.m_red, hl.m_red),
                                mix(base.m_green, hl.m_green),
                                mix(base.m_blue, hl.m_blue));
                p.setBrush(bg, LBrush::SolidPattern);
                p.setPen(LPenStyle::No);
                p.drawRoundedRect(r.toRectF().adjusted(2, 2, -2, -2), 4, 4, LSizeMode::Absolute);
            }

            std::shared_ptr<LListWidgetItem> data = owner->itemData(i);
            if(data != nullptr){
                const LSize<int> iconSize = owner->iconSize();
                if(owner->viewMode() == LListWidget::ListMode){
                    double x = r.m_x + 10;
                    if(data->icon() != nullptr){
                        const double iy = r.m_y + (r.m_height - iconSize.m_y) / 2.0;
                        p.drawPixmap(LPoint<double>(x, iy),
                                     data->icon()->getPixmap(iconSize, LTexture::Normal, true));
                        x += iconSize.m_x + 10;
                    }
                    // 选中项刚用 No 画笔填充过背景，这里必须显式指定实心画笔，
                    // 否则只改颜色而样式仍是 No，文字不会被绘制；样式表指定了前景色则以它为准
                    if(!hasForeground){
                        p.setPen(pal->brush(LPalette::Text)->m_color, LPenStyle::Solid, 1);
                    }
                    LRect<int> tr = owner->itemTextRect(i, viewWidth);
                    tr.translate(0, -offsetY);
                    // 基于坐标点的 drawText 固定文字起点，超出文本区域的部分自行裁剪；
                    // 段落方向沿用 painter 状态（LPainterState 默认为 Auto），
                    // LTR 文本不会触发 bidi 重排，无需强制方向
                    const LFontMetrics fm = fontMetrics();
                    const double baseline = tr.m_y + (tr.m_height - fm.height()) / 2.0 + fm.ascent();
                    p.save();
                    p.setClipRect(tr.toRectF(), LClipOper::Intersect);
                    p.drawText(LPoint<double>(tr.m_x, baseline), data->text());
                    p.restore();
                }else{
                    const double cx = r.m_x + r.m_width / 2.0;
                    const double iy = r.m_y + 10;
                    if(data->icon() != nullptr){
                        p.drawPixmap(LPoint<double>(cx - iconSize.m_x / 2.0, iy),
                                     data->icon()->getPixmap(iconSize, LTexture::Normal, true));
                    }
                    if(!hasForeground){
                        p.setPen(pal->brush(LPalette::Text)->m_color, LPenStyle::Solid, 1);
                    }
                    LRect<int> tr = owner->itemTextRect(i, viewWidth);
                    tr.translate(0, -offsetY);
                    const LFontMetrics fm = fontMetrics();
                    const double textW = fm.horizontalAdvance(data->text());
                    const double baseline = tr.m_y + (tr.m_height - fm.height()) / 2.0 + fm.ascent();
                    // 文本超出单元宽度时不能再居中：起点会落到文本区左侧之外，
                    // 裁剪后看到的是文本中间一段。此时从左边界开始，尾部由裁剪忽略
                    const double tx = textW >= tr.m_width
                            ? tr.m_x
                            : tr.m_x + (tr.m_width - textW) / 2.0;
                    p.save();
                    p.setClipRect(tr.toRectF(), LClipOper::Intersect);
                    p.drawText(LPoint<double>(tx, baseline), data->text());
                    p.restore();
                }
            }

            p.restore();
        }
    }

    void resizeEvent(std::shared_ptr<LResizeEvent> e) override
    {
        updateScrollBars();
        LWidget::resizeEvent(e);
    }

    void mouseMoveEvent(std::shared_ptr<LMouseEvent> e) override
    {
        std::shared_ptr<LListWidget> owner = parentT<LListWidget>();
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        if(owner == nullptr){
            e->ignore();
            return;
        }

        LPoint<int> pos = e->pos();
        if(area != nullptr){
            pos.m_y += area->contentsOffset().m_y;
        }
        const int row = owner->rowAt(pos, width());
        if(row != m_hoverRow){
            m_hoverRow = row;
            update();
        }
        LWidget::mouseMoveEvent(e);
    }

    void mousePressEvent(std::shared_ptr<LMouseEvent> e) override
    {
        std::shared_ptr<LListWidget> owner = parentT<LListWidget>();
        std::shared_ptr<LAbstractScrollArea> area = parentT<LAbstractScrollArea>();
        if(owner == nullptr || e->button() != LMouseButton::Left){
            e->ignore();
            return;
        }

        LPoint<int> pos = e->pos();
        if(area != nullptr){
            pos.m_y += area->contentsOffset().m_y;
        }
        const int row = owner->rowAt(pos, width());
        if(row >= 0){
            owner->setCurrentRow(row);
            owner->sigItemClicked.emt(row);

            std::shared_ptr<LListWidgetItem> item = owner->item(row);
            if(item != nullptr){
                // 项本身就是一个 action：单击激活（文件浏览器可在此打开目录/文件）
                item->activate(LActionBase::Trigger);
            }

            // 自行判定双击：不依赖系统的 MouseButtonDblClick 事件，
            // 避免某些窗口/控件组合下双击消息不派发到视图
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

                // 只有落在文字上才进入编辑，双击图标等其它位置不触发
                if(owner->isEditable() && owner->itemTextRect(row, width()).contains(pos)){
                    owner->editItem(row);
                }
            }else{
                m_lastClickRow = row;
                m_lastClickTime = now;
            }

            e->accept();
            return;
        }
        e->ignore();
    }

    int m_hoverRow = -1;
    int m_lastClickRow = -1;
    std::chrono::steady_clock::time_point m_lastClickTime{};
};

// ---------------------------------------------------------------------------
// LListWidget
// ---------------------------------------------------------------------------
LListWidget::LListWidget(std::shared_ptr<LWidget> parent)
    : LAbstractScrollArea(parent)
{
    // 注意：滚动条在 initScroll() 之后才存在，不能在这里设置策略
}

void LListWidget::initSpecific()
{
    LAbstractScrollArea::initSpecific();
    setVerticalScrollBarPolicy(LScrollPolicy::AsNeeded);
    setHorizontalScrollBarPolicy(LScrollPolicy::AlwaysOff);

    // 显式设置默认尺寸，避免类内初始化在某些构造路径下未生效
    if(m_iconSize.isNull()){
        m_iconSize = LSize<int>(20, 20);
    }
    if(m_gridSize.isNull()){
        m_gridSize = LSize<int>(96, 96);
    }
}

void LListWidget::initViewport()
{
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    std::shared_ptr<LListWidgetView> view = makeInit<LListWidgetView>(q);
    view->setBackgroundRole(LPalette::Base);
    view->setAutoFillBackground(true);
    view->addEventFilter(q);
    m_viewport = view;
    m_view = view;
}

LSize<int> LListWidget::sizeHint() const
{
    return LSize<int>(240, 200);
}

int LListWidget::addItem(const std::wstring &text,
                         const std::shared_ptr<LTexture> &icon,
                         const LVariant &userData)
{
    return addItem(std::make_shared<LListWidgetItem>(text, icon, userData));
}

int LListWidget::addItem(const std::shared_ptr<LListWidgetItem> &item)
{
    if(item == nullptr){
        return -1;
    }
    m_items.push_back(item);

    if(std::shared_ptr<LListWidgetView> view = m_view.lock()){
        view->updateScrollBars();
    }else if(m_viewport != nullptr){
        m_viewport->update();
    }
    update();
    return (int)m_items.size() - 1;
}

std::shared_ptr<LListWidgetItem> LListWidget::item(int row) const
{
    if(row < 0 || row >= (int)m_items.size()){
        return nullptr;
    }
    return m_items[row];
}

std::shared_ptr<LListWidgetItem> LListWidget::currentItem() const
{
    return item(m_currentRow);
}

int LListWidget::rowOf(const std::shared_ptr<LListWidgetItem> &item) const
{
    for(int i = 0; i < (int)m_items.size(); ++i){
        if(m_items[i] == item){
            return i;
        }
    }
    return -1;
}

void LListWidget::clear()
{
    cancelEdit();
    m_items.clear();
    m_currentRow = -1;
    if(std::shared_ptr<LListWidgetView> view = m_view.lock()){
        view->updateScrollBars();
    }
    update();
}

int LListWidget::count() const
{
    return (int)m_items.size();
}

void LListWidget::setViewMode(ViewMode mode)
{
    if(m_viewMode != mode){
        cancelEdit();
        m_viewMode = mode;
        if(std::shared_ptr<LListWidgetView> view = m_view.lock()){
            view->updateScrollBars();
            view->update();
        }
        update();
    }
}

LListWidget::ViewMode LListWidget::viewMode() const
{
    return m_viewMode;
}

void LListWidget::setIconSize(const LSize<int> &size)
{
    if(m_iconSize != size){
        m_iconSize = size;
        if(std::shared_ptr<LListWidgetView> view = m_view.lock()){
            view->updateScrollBars();
            view->update();
        }
    }
}

const LSize<int> &LListWidget::iconSize() const
{
    return m_iconSize;
}

void LListWidget::setGridSize(const LSize<int> &size)
{
    if(m_gridSize != size){
        m_gridSize = size;
        if(std::shared_ptr<LListWidgetView> view = m_view.lock()){
            view->updateScrollBars();
            view->update();
        }
    }
}

const LSize<int> &LListWidget::gridSize() const
{
    return m_gridSize;
}

void LListWidget::setCurrentRow(int row)
{
    if(row < -1 || row >= (int)m_items.size()){
        return;
    }
    if(m_currentRow != row){
        // 同步 action 的选中状态，将来样式表/自定义绘制可按 uiState() 区分
        if(m_currentRow >= 0 && m_currentRow < (int)m_items.size()){
            m_items[m_currentRow]->setSelected(false);
        }
        m_currentRow = row;
        if(row >= 0 && row < (int)m_items.size()){
            m_items[row]->setSelected(true);
        }
        if(m_viewport != nullptr){
            m_viewport->update();
        }
    }
}

int LListWidget::currentRow() const
{
    return m_currentRow;
}

void LListWidget::setEditable(bool editable)
{
    m_editable = editable;
    if(!editable){
        cancelEdit();
    }
}

bool LListWidget::isEditable() const
{
    return m_editable;
}

void LListWidget::editItem(int row)
{
    if(!m_editable || row < 0 || row >= (int)m_items.size() || m_viewport == nullptr){
        return;
    }

    std::shared_ptr<LListItemEditor> editor = std::dynamic_pointer_cast<LListItemEditor>(m_editor);
    if(editor == nullptr){
        editor = makeInit<LListItemEditor>(m_viewport);
        editor->setName("列表项编辑器");
        editor->sigCommit.connect(this, &LListWidget::commitEdit);
        editor->sigCancel.connect(this, &LListWidget::cancelEdit);
        m_editor = editor;
    }

    m_editRow = row;
    editor->setText(m_items[row]->text());

    // 编辑框贴合文字区域（内容坐标 -> 控件坐标，减掉滚动偏移）
    const LPoint<int> offset = contentsOffset();
    LRect<int> r = itemTextRect(row, m_viewport->width());
    if(!r.isValid()){
        r = itemRect(row, m_viewport->width());
    }
    r.translate(0, -offset.m_y);
    r = r.adjusted(-4, -2, 4, 2);
    if(r.m_height < 22){
        r.setHeight(22);
    }
    editor->setGeometry(r);
    editor->setVisible(true);
    editor->setFocus();
    // 光标置于末尾：双击编辑通常是接着已有文本继续输入，
    // 若停在开头，用户敲空格会变成前导空格，文字看起来整体右移
    editor->moveCursorToEnd();
    m_viewport->update();
}

void LListWidget::commitEdit()
{
    if(m_editor == nullptr || m_editRow < 0){
        return;
    }

    std::shared_ptr<LListItemEditor> editor = std::dynamic_pointer_cast<LListItemEditor>(m_editor);
    const int row = m_editRow;
    m_editRow = -1;
    if(m_editor != nullptr){
        m_editor->setVisible(false);
    }

    if(editor != nullptr && row >= 0 && row < (int)m_items.size()){
        const std::wstring text = editor->text();
        if(m_items[row]->text() != text){
            m_items[row]->setText(text);
            sigItemTextChanged.emt(row);
        }
    }
    if(m_viewport != nullptr){
        m_viewport->update();
    }
    update();
}

void LListWidget::cancelEdit()
{
    if(m_editor != nullptr){
        m_editor->setVisible(false);
    }
    m_editRow = -1;
    if(m_viewport != nullptr){
        m_viewport->update();
    }
}

void LListWidget::setItemText(int row, const std::wstring &text)
{
    if(row < 0 || row >= (int)m_items.size()){
        return;
    }
    if(m_items[row]->text() != text){
        m_items[row]->setText(text);
        if(m_viewport != nullptr){
            m_viewport->update();
        }
    }
}

std::wstring LListWidget::itemText(int row) const
{
    std::shared_ptr<LListWidgetItem> data = item(row);
    return data != nullptr ? data->text() : std::wstring();
}

std::shared_ptr<LListWidgetItem> LListWidget::itemData(int row) const
{
    return item(row);
}

int LListWidget::rowHeight() const
{
    const LFontMetrics fm = fontMetrics();
    const int iconH = std::max<int>(1, m_iconSize.m_y);
    // 下限保证图标完整显示，避免字体度量异常时行高过小
    return std::max<int>(38, std::max<int>(iconH, fm.height()) + 12);
}

int LListWidget::contentHeight(int viewWidth) const
{
    const int margin = 6;
    if(m_items.empty()){
        return margin * 2;
    }

    if(m_viewMode == ListMode){
        return margin * 2 + (int)m_items.size() * rowHeight();
    }

    const int gw = std::max(1, m_gridSize.m_x);
    const int cols = std::max(1, viewWidth / gw);
    const int rows = ((int)m_items.size() + cols - 1) / cols;
    return margin * 2 + rows * m_gridSize.m_y;
}

LRect<int> LListWidget::itemRect(int row, int viewWidth) const
{
    const int margin = 6;
    if(row < 0 || row >= (int)m_items.size()){
        return LRect<int>();
    }

    if(m_viewMode == ListMode){
        // 注意：LRect 的 4 参构造默认按坐标(x1,y1,x2,y2)解释，宽高需显式传 false
        return LRect<int>(margin, margin + row * rowHeight(),
                          std::max(1, viewWidth - 2 * margin), rowHeight(), false);
    }

    const int gw = std::max(1, m_gridSize.m_x);
    const int cols = std::max(1, viewWidth / gw);
    const int r = row / cols;
    const int c = row % cols;
    return LRect<int>(margin + c * gw, margin + r * m_gridSize.m_y,
                      gw, m_gridSize.m_y, false);
}

LRect<int> LListWidget::itemTextRect(int row, int viewWidth) const
{
    LRect<int> r = itemRect(row, viewWidth);
    if(!r.isValid()){
        return LRect<int>();
    }

    const LFontMetrics fm = fontMetrics();
    std::shared_ptr<LListWidgetItem> data = itemData(row);
    const bool hasIcon = (data != nullptr && data->icon() != nullptr);
    const int lineHeight = std::max<int>(fm.height(), 12);

    if(m_viewMode == ListMode){
        int x = r.m_x + 10;
        if(hasIcon){
            x += std::max<int>(1, m_iconSize.m_x) + 10;
        }
        const int y = r.m_y + (r.m_height - lineHeight) / 2;
        return LRect<int>(x, y, std::max(1, r.x2() - x - 8), lineHeight, false);
    }

    const int top = r.m_y + 10 + (hasIcon ? std::max<int>(1, m_iconSize.m_y) + 6 : 0);
    return LRect<int>(r.m_x + 4, top, std::max(1, r.m_width - 8), lineHeight, false);
}

int LListWidget::rowAt(const LPoint<int> &contentPos, int viewWidth) const
{
    for(int i = 0; i < (int)m_items.size(); ++i){
        if(itemRect(i, viewWidth).contains(contentPos)){
            return i;
        }
    }
    return -1;
}

LK_NAMESPACE_END
