#ifndef LLISTWIDGET_H
#define LLISTWIDGET_H

#include "lkwidgets/widget/labstractscrollarea.h"
#include "lkcore/lsignal.h"
#include <vector>

LK_NAMESPACE_BEGIN

class LListWidget;
class LListWidgetView;

/** 列表项数据 */
struct LListWidgetItemData
{
    std::wstring m_text;
    std::shared_ptr<LTexture> m_icon;
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

    /** 添加一项，返回其行号 */
    int addItem(const std::wstring &text, const std::shared_ptr<LTexture> &icon = nullptr);
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
    const LListWidgetItemData *itemData(int row) const;

    LSignal<int> sigItemClicked;
    LSignal<int> sigItemDoubleClicked;
    /** 编辑提交后发出，参数为行号（文本可通过 itemText() 取） */
    LSignal<int> sigItemTextChanged;

protected:
    virtual void initSpecific() override;
    virtual void initViewport() override;
    virtual LSize<int> sizeHint() const override;

    std::vector<LListWidgetItemData> m_items;
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
