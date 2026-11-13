#ifndef LTABLEWIDGET_H
#define LTABLEWIDGET_H

#include "lkwidgets/widget/labstractscrollarea.h"
#include "lkwidgets/util/laction.h"
#include "lkcore/livar.h"
#include "lkcore/lsignal.h"
#include "lkcore/lvariant.h"
#include "lkgui/lrect.h"
#include "lkgui/lsize.h"
#include <vector>
#include <memory>

LK_NAMESPACE_BEGIN

class LTableWidget;
class LTableWidgetView;

/**
 * 一个 item 就是一个单元格：文本、图标、userData 都由 LActionBase 提供。
 * 表格按 row × column 的网格持有 item；合并区域内只有左上角的 item 会被显示/编辑。
 */
class LKWIDGETSDEF LTableWidgetItem : public LActionBase, public LIVar
{
public:
    LTableWidgetItem(const std::wstring &text = std::wstring(),
                     std::shared_ptr<LTexture> icon = nullptr,
                     const LVariant &userData = LVariant());

    /** 单元格级可编辑性，默认 true */
    void setEditable(bool editable);
    bool isEditable() const;

    LSignal<std::shared_ptr<LTableWidgetItem>> sigTrigger;
    LSignal<std::shared_ptr<LTableWidgetItem>> sigDoubleClick;

    virtual void activate(ActionEvent event) override;
    void triggerDoubleClick();

private:
    bool m_editable = true;
};

/** 表头某一列/行的尺寸策略，语义与 Qt 的 QHeaderView::ResizeMode 对应 */
enum class LTableResizeMode
{
    Fixed,              // 固定，用户不能拖动
    Interactive,        // 用户可拖动表头分隔线调整（默认）
    Stretch,            // 按可用空间平均分配
    ResizeToContents,   // 按单元格内容自适应
};

/**
 * 表格控件：网格单元格 + 行列头 + 邻近单元格合并 + 就地编辑。
 * 结构与 LListWidget / LTreeWidget 一致：LAbstractScrollArea 提供滚动条与视口，
 * 内部视图负责绘制与鼠标命中；列头固定在顶部、行头固定在左侧。
 *
 * 注：以 LPoint<int> 返回的行列坐标统一为 m_x = 行号、m_y = 列号，
 *     (-1,-1) 表示无有效单元格。
 */
class LKWIDGETSDEF LTableWidget : public LAbstractScrollArea
{
public:
    _LIVarTagEx(LTableWidget, LWidget)

    LTableWidget(std::shared_ptr<LWidget> parent = nullptr);

    // ---- 网格规模与单元格 ----
    int rowCount() const;
    int columnCount() const;
    void setRowCount(int rows);
    void setColumnCount(int columns);
    void setRowColumnCount(int rows, int columns);

    void setItem(int row, int column, const std::shared_ptr<LTableWidgetItem> &item);
    std::shared_ptr<LTableWidgetItem> item(int row, int column) const;
    /** 空单元格会在写入时自动创建一个 item */
    void setItemText(int row, int column, const std::wstring &text);
    std::wstring itemText(int row, int column) const;
    void clearContents();
    void clear();

    // ---- 相邻单元格合并 ----
    /** 把 (row,column) 起的 rowSpan × columnSpan 区域合并成一个单元格 */
    bool mergeCells(int row, int column, int rowSpan, int columnSpan);
    /** 拆开以 (row,column) 为左上角的合并区 */
    bool splitCells(int row, int column);
    void clearSpans();
    /** 覆盖该位置的合并区参数；无合并时返回自身与 1×1 */
    bool spanOf(int row, int column, int &originRow, int &originColumn,
                int &rowSpan, int &columnSpan) const;
    /** 返回该位置所代表单元格的左上角坐标 */
    LPoint<int> originAt(int row, int column) const;
    /** 是否被别的合并区覆盖（不能显示、不能编辑） */
    bool isSpanned(int row, int column) const;
    bool isSpanOrigin(int row, int column) const;

    // ---- 表头 ----
    void setHorizontalHeaderLabels(const std::vector<std::wstring> &labels);
    void setVerticalHeaderLabels(const std::vector<std::wstring> &labels);
    std::wstring horizontalHeaderLabel(int column) const;
    std::wstring verticalHeaderLabel(int row) const;
    void setHeaderVisible(bool visible);
    bool isHeaderVisible() const;

    // ---- 尺寸：手动设置 / 策略 / 按内容自适应 ----
    void setRowHeight(int height);
    void setRowHeight(int row, int height);
    int rowHeight() const;
    int rowHeight(int row) const;
    void setColumnWidth(int column, int width);
    int columnWidth(int column) const;

    /** 列宽策略；默认 Interactive，可直接拖动表头分隔线 */
    void setColumnResizeMode(int column, LTableResizeMode mode);
    void setColumnResizeMode(LTableResizeMode mode);
    LTableResizeMode columnResizeMode(int column) const;
    /** 行高策略；默认 Interactive，可直接拖动行头分隔线 */
    void setRowResizeMode(int row, LTableResizeMode mode);
    void setRowResizeMode(LTableResizeMode mode);
    LTableResizeMode rowResizeMode(int row) const;
    /** 最后一列自动填满剩余宽度（类似 Qt 的 stretchLastSection） */
    void setStretchLastColumn(bool stretch);
    bool stretchLastColumn() const;
    /** 最后一行自动填满剩余高度；用得少，保留以备需要 */
    void setStretchLastRow(bool stretch);
    bool stretchLastRow() const;

    /** 按内容重算单列 / 全部列宽 */
    void resizeColumnToContents(int column);
    void resizeColumnsToContents();
    /** 按内容重算单行 / 全部行高 */
    void resizeRowToContents(int row);
    void resizeRowsToContents();

    // ---- 就地编辑与可编辑性 ----
    /** 控件总开关，默认 false；关闭时任何单元格都不可编辑 */
    void setEditable(bool editable);
    bool isEditable() const;
    void setRowEditable(int row, bool editable);
    bool isRowEditable(int row) const;
    void setColumnEditable(int column, bool editable);
    bool isColumnEditable(int column) const;
    /** 最终判定：总开关 && 行 && 列 && item，且合并区只能从左上角编辑 */
    bool isCellEditable(int row, int column) const;
    void editItem(int row, int column);
    void commitEdit();
    void cancelEdit();

    // ---- 选中 ----
    LPoint<int> currentCell() const;
    int currentRow() const;
    int currentColumn() const;
    std::shared_ptr<LTableWidgetItem> currentItem() const;
    void setCurrentCell(int row, int column);

    // ---- 供内部视图使用（内容坐标：不含行头/列头与滚动偏移）----
    int headerHeight() const;
    int headerWidth() const;
    int contentWidth(int viewWidth) const;
    int contentHeight(int viewWidth) const;
    int columnX(int column) const;
    int rowY(int row) const;
    int columnAt(int x) const;
    int rowAt(int y) const;
    LRect<int> cellRect(int row, int column) const;
    /** 含合并扩展的实际矩形；被覆盖单元格返回无效矩形 */
    LRect<int> visualCellRect(int row, int column) const;
    /** 命中测试，返回合并区左上角坐标 */
    LPoint<int> cellAt(const LPoint<int> &contentPos) const;
    void onItemChanged();

    LSignal<int, int> sigCellClicked;
    LSignal<int, int> sigCellDoubleClicked;
    LSignal<std::shared_ptr<LTableWidgetItem>, int, int> sigCellChanged;
    LSignal<int, int> sigCurrentCellChanged;
    LSignal<int> sigHorizontalHeaderClicked;
    LSignal<int> sigVerticalHeaderClicked;

protected:
    virtual void initSpecific() override;
    virtual void initViewport() override;
    virtual LSize<int> sizeHint() const override;
    virtual void resizeEvent(std::shared_ptr<LResizeEvent> event) override;
    virtual void scrollContentsBy(int dx, int dy) override;

private:
    struct Span {
        int row = 0;
        int column = 0;
        int rowSpan = 1;
        int columnSpan = 1;
    };

    void resizeGrid(int rows, int columns);
    void refresh();
    /** 按各列/行的策略重新布局尺寸（Stretch / ResizeToContents / 最后一列拉伸） */
    void layoutSections();
    void rebuildRowOffsets();
    int columnContentWidth(int column) const;
    int rowContentHeight(int row) const;
    int indexOf(int row, int column) const;
    const Span *spanCovering(int row, int column) const;
    const Span *spanStartingAt(int row, int column) const;
    std::shared_ptr<LTableWidgetItem> ensureItem(int row, int column);

    int m_rowCount = 0;
    int m_columnCount = 0;
    std::vector<std::shared_ptr<LTableWidgetItem>> m_items;
    std::vector<Span> m_spans;
    std::vector<std::wstring> m_hHeaders;
    std::vector<std::wstring> m_vHeaders;
    std::vector<int> m_columnWidths;
    std::vector<LTableResizeMode> m_columnModes;
    std::vector<int> m_rowHeights;
    std::vector<int> m_rowOffsets{0};
    std::vector<LTableResizeMode> m_rowModes;
    std::vector<int> m_rowEditable;
    std::vector<int> m_columnEditable;
    int m_rowHeight = 28;
    bool m_stretchLastColumn = false;
    bool m_stretchLastRow = false;
    bool m_inLayout = false;
    bool m_headerVisible = true;
    bool m_editable = false;

    int m_currentRow = -1;
    int m_currentColumn = -1;
    int m_editRow = -1;
    int m_editColumn = -1;
    std::shared_ptr<LWidget> m_editor;

    std::weak_ptr<LTableWidgetView> m_view;
};

LK_NAMESPACE_END

#endif // LTABLEWIDGET_H
