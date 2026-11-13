#ifndef LTREEWIDGET_H
#define LTREEWIDGET_H

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

class LTreeWidget;
class LTreeWidgetView;

/**
 * 树节点：第 0 列是树列（缩进 + 展开标记 + 图标 + 名称），
 * 其余列是属性列，通常用于叶子结点，例如 类型 / 大小 / 修改时间。
 *
 * 与 LListWidgetItem 一样派生 LActionBase + LIVar，
 * 因此图标、userData、选中状态和单击/双击信号都能直接复用。
 */
class LKWIDGETSDEF LTreeWidgetItem : public LActionBase, public LIVar
{
public:
    LTreeWidgetItem(const std::wstring &text,
                    std::shared_ptr<LTexture> icon = nullptr,
                    const LVariant &userData = LVariant());
    /** 一次给定多列文本，texts[0] 作为树列名称 */
    LTreeWidgetItem(const std::vector<std::wstring> &texts,
                    std::shared_ptr<LTexture> icon = nullptr,
                    const LVariant &userData = LVariant());

    // ---- 多列文本 ----
    int columnCount() const;
    void setColumnCount(int count);
    /** 列号越界返回空串；第 0 列与 LActionBase::text() 保持同步 */
    std::wstring text(int column = 0) const;
    void setText(int column, const std::wstring &t);
    void setText(const std::wstring &t) { setText(0, t); }

    // ---- 树结构 ----
    int addChild(const std::shared_ptr<LTreeWidgetItem> &child);
    void insertChild(int index, const std::shared_ptr<LTreeWidgetItem> &child);
    bool removeChild(const std::shared_ptr<LTreeWidgetItem> &child);
    std::shared_ptr<LTreeWidgetItem> child(int index) const;
    int childCount() const;
    int indexOfChild(const std::shared_ptr<LTreeWidgetItem> &child) const;
    std::shared_ptr<LTreeWidgetItem> parentItem() const;
    /** 顶层节点为 0，逐层加一 */
    int depth() const;
    bool isLeaf() const;

    bool isExpanded() const;
    void setExpanded(bool expanded);

    // ---- 可编辑性：行级 / 单元格级 ----
    /** 该节点整行是否允许编辑；默认 true，不继承给子节点 */
    void setEditable(bool editable);
    bool isEditable() const;
    /** 显式指定某个单元格可编辑/只读，优先级高于行级与列级默认 */
    void setCellEditable(int column, bool editable);
    /** 恢复该单元格为继承行级 + 列级规则 */
    void resetCellEditable(int column);
    /** 供控件判定：-1 继承、0 只读、1 可编辑 */
    int cellEditable(int column) const;

    LSignal<std::shared_ptr<LTreeWidgetItem>> sigTrigger;
    LSignal<std::shared_ptr<LTreeWidgetItem>> sigDoubleClick;

    virtual void activate(ActionEvent event) override;
    void triggerDoubleClick();

private:
    friend class LTreeWidget;
    std::vector<std::wstring> m_texts;
    std::vector<std::shared_ptr<LTreeWidgetItem>> m_children;
    std::weak_ptr<LTreeWidgetItem> m_parentItem;
    std::weak_ptr<LTreeWidget> m_tree;
    bool m_expanded = true;
    bool m_editable = true;
    /** 单元格显式设置：-1 继承、0 只读、1 可编辑 */
    std::vector<int> m_cellEditable;
};

/**
 * 树形列表控件：层级节点 + 表头 + 任意数量的属性列。
 * 结构参考 LListWidget，LAbstractScrollArea 提供滚动条与视口，
 * 内部视图负责绘制与鼠标命中；支持横向滚动查看超出视口的列。
 */
class LKWIDGETSDEF LTreeWidget : public LAbstractScrollArea
{
public:
    // 注册类型信息：样式表可按 LTreeWidget::item / ::section 匹配
    _LIVarTagEx(LTreeWidget, LWidget)

    LTreeWidget(std::shared_ptr<LWidget> parent = nullptr);

    // ---- 列与表头 ----
    int columnCount() const;
    void setColumnCount(int count);
    int columnWidth(int column) const;
    void setColumnWidth(int column, int width);
    std::vector<std::wstring> headerLabels() const;
    /** 标签数量大于当前列数时自动扩充列 */
    void setHeaderLabels(const std::vector<std::wstring> &labels);
    std::wstring headerLabel(int column) const;
    bool isHeaderVisible() const;
    void setHeaderVisible(bool visible);

    // ---- 树结构 ----
    int addTopLevelItem(const std::shared_ptr<LTreeWidgetItem> &item);
    std::shared_ptr<LTreeWidgetItem> addTopLevelItem(
            const std::wstring &text,
            std::shared_ptr<LTexture> icon = nullptr,
            const LVariant &userData = LVariant());
    int addChild(const std::shared_ptr<LTreeWidgetItem> &parent,
                 const std::shared_ptr<LTreeWidgetItem> &child);
    /** 便捷方法：直接按多列文本添加一个叶子结点 */
    std::shared_ptr<LTreeWidgetItem> addLeaf(
            const std::shared_ptr<LTreeWidgetItem> &parent,
            const std::vector<std::wstring> &texts,
            std::shared_ptr<LTexture> icon = nullptr,
            const LVariant &userData = LVariant());
    void clear();
    int topLevelItemCount() const;
    std::shared_ptr<LTreeWidgetItem> topLevelItem(int index) const;

    // ---- 展开 / 折叠 ----
    void expandItem(const std::shared_ptr<LTreeWidgetItem> &item);
    void collapseItem(const std::shared_ptr<LTreeWidgetItem> &item);
    void setItemExpanded(const std::shared_ptr<LTreeWidgetItem> &item, bool expanded);
    void expandAll();
    void collapseAll();

    // ---- 选中与可见行 ----
    std::shared_ptr<LTreeWidgetItem> currentItem() const;
    void setCurrentItem(const std::shared_ptr<LTreeWidgetItem> &item);
    int currentRow() const;
    void setCurrentRow(int row);
    /** 当前按展开状态展开出来的可见行数量 */
    int visibleRowCount() const;
    std::shared_ptr<LTreeWidgetItem> itemAtRow(int row) const;
    int rowOf(const std::shared_ptr<LTreeWidgetItem> &item) const;

    // ---- 外观 ----
    void setIconSize(const LSize<int> &size);
    const LSize<int> &iconSize() const;
    /** 每层缩进的像素数 */
    void setIndentation(int indent);
    int indentation() const;

    // ---- 就地编辑与可编辑性 ----
    /** 控件总开关；关闭时任何单元格都不可编辑 */
    void setEditable(bool editable);
    bool isEditable() const;
    /** 列级开关：默认第 0 列只读（双击留给展开/折叠），其余列可编辑 */
    void setColumnEditable(int column, bool editable);
    bool isColumnEditable(int column) const;
    /** 最终判定：总开关 && (单元格显式设置 > 行级 && 列级) */
    bool isCellEditable(const std::shared_ptr<LTreeWidgetItem> &item, int column) const;
    bool isCellEditable(int row, int column) const;
    void editItem(int row, int column);
    void commitEdit();
    void cancelEdit();

    // ---- 供内部视图使用（内容坐标：不含表头高度与滚动偏移）----
    int rowHeight() const;
    int headerHeight() const;
    int contentHeight(int viewWidth) const;
    int contentWidth(int viewWidth) const;
    int columnX(int column) const;
    int columnAt(int x) const;
    LRect<int> rowRect(int row, int viewWidth) const;
    /** 展开标记的命中区域；叶子结点返回无效矩形 */
    LRect<int> branchRect(int row) const;
    int rowAt(const LPoint<int> &contentPos, int viewWidth) const;
    void toggleExpanded(int row);
    /** 节点文本/结构/展开状态变化后的刷新入口 */
    void onItemChanged();

    LSignal<int> sigItemClicked;
    LSignal<int> sigItemDoubleClicked;
    LSignal<int> sigHeaderClicked;
    LSignal<std::shared_ptr<LTreeWidgetItem>> sigCurrentItemChanged;
    LSignal<std::shared_ptr<LTreeWidgetItem>> sigItemExpanded;
    LSignal<std::shared_ptr<LTreeWidgetItem>> sigItemCollapsed;
    /** 单元格文本提交后发出：参数为节点与列号 */
    LSignal<std::shared_ptr<LTreeWidgetItem>, int> sigItemTextChanged;

protected:
    virtual void initSpecific() override;
    virtual void initViewport() override;
    virtual LSize<int> sizeHint() const override;
    virtual void scrollContentsBy(int dx, int dy) override;

private:
    void registerSubTree(const std::shared_ptr<LTreeWidgetItem> &item);
    void releaseSubTree(const std::shared_ptr<LTreeWidgetItem> &item);
    void collect(const std::shared_ptr<LTreeWidgetItem> &item);
    void walkAll(const std::shared_ptr<LTreeWidgetItem> &item, bool expanded);
    void rebuildVisible();
    void refresh();

    std::vector<std::shared_ptr<LTreeWidgetItem>> m_roots;
    std::vector<std::shared_ptr<LTreeWidgetItem>> m_visible;
    std::vector<std::wstring> m_headers{std::wstring()};
    std::vector<int> m_columnWidths{200};
    /** 列级可编辑性：0 只读 / 1 可编辑，第 0 列默认只读 */
    std::vector<int> m_columnEditable{0};
    int m_columnCount = 1;
    bool m_headerVisible = true;
    LSize<int> m_iconSize{16, 16};
    int m_indentation = 18;
    int m_currentRow = -1;

    bool m_editable = false;
    int m_editRow = -1;
    int m_editColumn = -1;
    std::shared_ptr<LWidget> m_editor;

    std::weak_ptr<LTreeWidgetView> m_view;
};

LK_NAMESPACE_END

#endif // LTREEWIDGET_H
