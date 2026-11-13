#ifndef LLISTWIDGET_H
#define LLISTWIDGET_H

#include "lkwidgets/widget/labstractscrollarea.h"
#include "lkwidgets/util/laction.h"
#include "lkcore/livar.h"
#include "lkcore/lsignal.h"
#include <vector>

LK_NAMESPACE_BEGIN

class LListWidget;
class LListWidgetView;

/**
 * 列表项：与 LComboBoxItem 一样派生自 LActionBase，
 * 因此文本、图标、userData（如文件路径）、选中/可用状态都由 action 基类提供。
 * 每项自带单击、双击信号，文件浏览器之类的场景可以直接连到槽。
 */
class LKWIDGETSDEF LListWidgetItem : public LActionBase, public LIVar
{
public:
    LListWidgetItem(const std::wstring &text,
                    std::shared_ptr<LTexture> icon = nullptr,
                    const LVariant &userData = LVariant());

    /** 参数为项自身，槽里可直接读 text() / userData() */
    LSignal<std::shared_ptr<LListWidgetItem>> sigTrigger;
    LSignal<std::shared_ptr<LListWidgetItem>> sigDoubleClick;

    virtual void activate(ActionEvent event) override;
    void triggerDoubleClick();
};

/**
 * 列表控件，支持两种显示模式：
 *   ListMode —— 每行一项：图标在左、文字在右，纵向滚动
 *   IconMode —— 网格排列：图标在上、文字在下，按宽度自动分列
 *
 * 结构参考 LTextEdit：LAbstractScrollArea 提供滚动条与视口，
 * 内部视图 LListWidgetView 负责绘制与鼠标命中。
 */
class LKWIDGETSDEF LListWidget : public LAbstractScrollArea
{
public:
    // 注册类型信息：切换样式表时按类型名匹配选择器（LListWidget / LListWidget::item）
    _LIVarTagEx(LListWidget, LWidget)

    enum ViewMode {
        ListMode,
        IconMode
    };

    LListWidget(std::shared_ptr<LWidget> parent = nullptr);

    /** 添加一项并返回行号；userData 可携带文件路径、类型等自定义数据 */
    int addItem(const std::wstring &text,
                const std::shared_ptr<LTexture> &icon = nullptr,
                const LVariant &userData = LVariant());

    /** 直接添加构造好的项：可先连好信号再放入列表 */
    int addItem(const std::shared_ptr<LListWidgetItem> &item);

    /** 按行号取项，便于读取 userData 或连接项自身的信号 */
    std::shared_ptr<LListWidgetItem> item(int row) const;
    std::shared_ptr<LListWidgetItem> currentItem() const;
    int rowOf(const std::shared_ptr<LListWidgetItem> &item) const;

    void clear();
    int count() const;

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;

    void setIconSize(const LSize<int> &size);
    const LSize<int> &iconSize() const;

    /** IconMode 下的单元格大小 */
    void setGridSize(const LSize<int> &size);
    const LSize<int> &gridSize() const;

    void setCurrentRow(int row);
    int currentRow() const;

    /** 可编辑模式：开启后双击某一项即可就地编辑文本 */
    void setEditable(bool editable);
    bool isEditable() const;

    /** 进入编辑状态（可编辑模式下双击时自动调用） */
    void editItem(int row);
    /** 结束编辑并提交 / 放弃 */
    void commitEdit();
    void cancelEdit();

    void setItemText(int row, const std::wstring &text);
    std::wstring itemText(int row) const;

    // ---- 供内部视图使用 ----
    int rowHeight() const;
    int contentHeight(int viewWidth) const;
    LRect<int> itemRect(int row, int viewWidth) const;
    /** 项内文字所占的矩形（内容坐标），用于命中判断与就地编辑定位 */
    LRect<int> itemTextRect(int row, int viewWidth) const;
    int rowAt(const LPoint<int> &contentPos, int viewWidth) const;
    /** 兼容旧接口，等价于 item(row) */
    std::shared_ptr<LListWidgetItem> itemData(int row) const;

    LSignal<int> sigItemClicked;
    LSignal<int> sigItemDoubleClicked;
    /** 编辑提交后发出，参数为行号（文本可通过 itemText() 取） */
    LSignal<int> sigItemTextChanged;

protected:
    virtual void initSpecific() override;
    virtual void initViewport() override;
    virtual LSize<int> sizeHint() const override;

    std::vector<std::shared_ptr<LListWidgetItem>> m_items;
    ViewMode m_viewMode = ListMode;
    LSize<int> m_iconSize{24, 24};
    LSize<int> m_gridSize{96, 96};
    int m_currentRow = -1;

    bool m_editable = false;
    int m_editRow = -1;
    std::shared_ptr<LWidget> m_editor;

    std::weak_ptr<LListWidgetView> m_view;
};

LK_NAMESPACE_END

#endif // LLISTWIDGET_H
