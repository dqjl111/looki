#include <iostream>
#include <fstream>
#include <lkcore/ldebug.h>
#include "lkcore/io/lnamedvar.h"

#include <tchar.h>
#ifdef _WIN32
#else
#include <locale.h>
#include <langinfo.h>
#endif
#include "lknetwork/ludpsocket.h"
#include "lknetwork/ltcpserver.h"
#include "lkwidgets/widget/lmainwindow.h"
#include "lkwidgets/core/lapplication.h"
#include "lkgui/image/limagereader.h"
#include "lkwidgets/core/lboxlayout.h"
#include "lkwidgets/widget/lframe.h"
#include "lkwidgets/widget/llabel.h"
#include "lkwidgets/core/lgridlayout.h"
#include "lkgui/image/ltexture.h"
#include "lkwidgets/widget/lbutton.h"
#include "lkwidgets/widget/llineedit.h"
#include "lkwidgets/util/leditbase.h"
#include "lkgui/lplatformcursor.h"
#include "lkcore/time/ltimer.h"
#include "lkcore/3rd/nameof.h"
#include "lkwidgets/draw/lwidgetstyle.h"
#include "lkwidgets/widget/lmenu.h"
#include "lkwidgets/dialog/ldialog.h"
#include "lkwidgets/widget/lslider.h"
#include "lkwidgets/widget/ledititemview.h"
#include "lkwidgets/widget/ltextedit.h"
#include "lkwidgets/widget/lscrollarea.h"
#include "lkcore/io/lresource.h"
#include "lkcore/io/li18n.h"
#include "lkwidgets/dialog/lfiledialog.h"
#include "lkwidgets/widget/lcombobox.h"
#include "lkwidgets/widget/lcomboedit.h"
#include "lkwidgets/widget/lmenubar.h"
#include "lkwidgets/widget/lspinbox.h"
#include "lkwidgets/widget/lgroupbox.h"
#include "lkwidgets/widget/lstackedwidget.h"
#include "lkwidgets/widget/llistwidget.h"
#include "lkwidgets/widget/ltreewidget.h"
#include "lkwidgets/widget/ltablewidget.h"
#include "lkwidgets/widget/ltabbar.h"
#include "lkwidgets/widget/lsplitter.h"
#include "lkgraph/svg/lsvgrenderer.h"


#include "lkaudio/lsound.h"
#include "lksql/lipersist.h"
#include "edeviceuser.h"

#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <set>
#include <type_traits>

#include <format>
#include <chrono>


#if LOOKI_NAMESPACE
using namespace lk;
#endif


template<class T>
void printType(){
    lWarn() << NAMEOF_SHORT_TYPE(T);
}

//template<std::derived_from<LWidget> WidgetType>
//struct WithRole {
//    //using Type = Rc<WidgetType>;
//};


class LTestMainWindow : public LMainWindow
{
    int clickCount = 0;
    std::shared_ptr<LTextEdit> edit;
    std::shared_ptr<LLineEdit> lineEdit;
    std::shared_ptr<LStackedWidget> stack;
    std::shared_ptr<LStackedWidget> tabStack;
    std::shared_ptr<LListWidget> listWidget;
    std::shared_ptr<LTreeWidget> treeWidget;
    std::shared_ptr<LTableWidget> tableWidget;
    std::shared_ptr<LSound> sound;
    std::shared_ptr<LUdpSocket> udp;
    std::shared_ptr<LTcpServer> server;
    std::shared_ptr<LTcpSocket> peer;
    // std::shared_ptr<LSvgDocument> svgDoc;

    // 主题相关：单独设置过样式表的弹出菜单需要同步刷新
    std::vector<std::shared_ptr<LWidget>> m_popupMenus;
    std::string m_themeSheet;
    bool m_themeSheetRaw = true;
    std::shared_ptr<LTimer> m_themeTimer;

    // 菜单关闭后统一刷新，避免在菜单的模态循环里刷新被丢弃
    void refreshAfterTheme()
    {
        repaintTree(getPtrT<LTreeObj>());
        update();
    }

    // sheet 为空表示恢复默认外观（setStyleSheet 内部会先清除旧样式与缓存）
    void applyThemeSheet(const std::string &sheet, bool isRaw)
    {
        m_themeSheet = sheet;
        m_themeSheetRaw = isRaw;
        setStyleSheet(sheet, isRaw, false);
        for (const std::shared_ptr<LWidget> &w : m_popupMenus) {
            if (w != nullptr) {
                w->setStyleSheet(sheet, isRaw, false);
            }
        }
        update();
    }

    static void repaintTree(const std::shared_ptr<LTreeObj> &obj)
    {
        if (obj == nullptr) {
            return;
        }
        for (const std::shared_ptr<LTreeObj> &child : obj->children()) {
            if (child == nullptr) {
                continue;
            }
            std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(child);
            if (w != nullptr) {
                w->update();
            }
            repaintTree(child);
        }
    }

    // 独立窗口中显示的弹出菜单需要单独应用样式，并登记以便切换主题时同步
    void useThemeSheet(const std::shared_ptr<LWidget> &w)
    {
        if (w == nullptr) {
            return;
        }
        w->setStyleSheet(m_themeSheet, m_themeSheetRaw, false);
        m_popupMenus.push_back(w);
    }

    void applyDarkPalette(bool dark)
    {
        std::shared_ptr<LPalette> pal = LGuiApplication::palette();
        if (!dark) {
            pal->initDefaultPalette();
            return;
        }

        const auto setColor = [&pal](LPalette::ColorRole role, uint8_t r, uint8_t g, uint8_t b) {
            pal->setBrush(LPalette::All, role, LBrush(LColor(r, g, b)));
        };

        setColor(LPalette::Window,          32, 32, 32);   // 窗口底色，保持深
        setColor(LPalette::WindowText,     237, 237, 237);
        setColor(LPalette::Base,            43, 43, 43);   // 输入区比窗口亮一档
        setColor(LPalette::AlternateBase,   46, 46, 46);
        setColor(LPalette::Button,          58, 58, 58);   // 按钮明显亮于面板
        setColor(LPalette::ButtonText,     237, 237, 237);
        setColor(LPalette::Text,           237, 237, 237);
        setColor(LPalette::BrightText,     255, 255, 255);
        setColor(LPalette::Light,           86, 86, 86);
        setColor(LPalette::Midlight,        72, 72, 72);   // 悬停
        setColor(LPalette::Mid,             86, 86, 86);   // 边框，拉开对比
        setColor(LPalette::Dark,            48, 48, 48);   // 按下
        setColor(LPalette::Shadow,         104, 104, 104); // 滑块/标记描边
        setColor(LPalette::Highlight,       64, 138, 224);
        setColor(LPalette::HighlightedText, 255, 255, 255);
        setColor(LPalette::Link,           120, 180, 245);
        setColor(LPalette::LinkVisited,    170, 160, 235);
        setColor(LPalette::ToolTipBase,     58, 58, 58);
        setColor(LPalette::ToolTipText,    237, 237, 237);

        pal->setBrush(LPalette::Disabled, LPalette::Text,       LBrush(LColor(132, 132, 132)));
        pal->setBrush(LPalette::Disabled, LPalette::WindowText, LBrush(LColor(132, 132, 132)));
        pal->setBrush(LPalette::Disabled, LPalette::ButtonText, LBrush(LColor(132, 132, 132)));
        pal->setBrush(LPalette::Disabled, LPalette::Base,       LBrush(LColor(40, 40, 40)));
        pal->setBrush(LPalette::Disabled, LPalette::Button,     LBrush(LColor(46, 46, 46)));
    }

    void switchTheme(bool dark)
    {
        // 1. 应用目标主题的调色板
        applyDarkPalette(dark);

        // 2. 一次调用完成切换：setStyleSheet 内部会先清除旧样式与渲染缓存，
        //    再解析新样式；传入空串则表示恢复默认外观
        applyThemeSheet(dark ? std::string(":/resfile/style.css") : std::string(), !dark);

        setWindowTitle(dark ? L"LOOKI 示例 - 暗色风格" : L"LOOKI 示例 - 默认风格");

        // 4. 立即刷新一次，并在菜单关闭后再刷新一次（菜单模态循环中提交的
        //    重绘请求可能被丢弃，导致切回浅色时残留旧像素）
        repaintTree(getPtrT<LTreeObj>());
        update();

        if (m_themeTimer == nullptr) {
            m_themeTimer = std::make_shared<LTimer>();
            m_themeTimer->sigTimeout.connect(this, &LTestMainWindow::refreshAfterTheme,
                                             ConnectionType::Queued);
        }
        m_themeTimer->start(std::chrono::milliseconds(80), true);
    }

protected:
    virtual void initSpecific() override{
        std::shared_ptr<LTexture> icon = std::make_shared<LTexture>(":/resfile/logo.png");
        setBaseSize(800, 500);
        setWindowIcon(icon);
        // setStyleSheet(":/resfile/style.css");

        std::shared_ptr<LVBoxLayout> layout = std::make_shared<LVBoxLayout>();
        layout->setName("Main layout");
        setLayout(layout);


        std::shared_ptr<LMenuBar> menuBar = makeInit<LMenuBar>();
        menuBar->setName("菜单栏");
        // menuBar->setMinimumHeight(40);
        for(int i=0; i<4; ++i)
        {
            std::shared_ptr<LMenu> menu = makeInit<LMenu>();
            menu->setWName(L"Menu" + std::to_wstring(i));

            std::shared_ptr<LSignalAction> test1 = std::make_shared<LSignalAction>(L"test1-" + std::to_wstring(i));
            test1->setWName(test1->text());
            test1->sigTrigger.connect(this, &LTestMainWindow::onSubAction1);
            menu->addAction(test1);

            std::shared_ptr<LSignalAction> test2 = std::make_shared<LSignalAction>(L"test2-" + std::to_wstring(i));
            test2->setWName(test2->text());
            test2->sigTrigger.connect(this, &LTestMainWindow::onSubAction2);
            menu->addAction(test2);

            std::shared_ptr<LSignalAction> test3 = std::make_shared<LSignalAction>(L"test3-" + std::to_wstring(i));
            test3->setWName(test3->text());
            test3->sigTrigger.connect(this, &LTestMainWindow::onSubAction3);
            menu->addAction(test3);

            menuBar->addMenu(menu);
        }

        // 外观切换菜单：默认风格 / 暗色风格
        {
            std::shared_ptr<LMenu> themeMenu = makeInit<LMenu>();
            themeMenu->setWName(L"外观");

            std::shared_ptr<LSignalAction> lightAct = std::make_shared<LSignalAction>(L"默认风格");
            lightAct->setWName(lightAct->text());
            lightAct->sigTrigger.connect(this, &LTestMainWindow::onThemeLight);
            themeMenu->addAction(lightAct);

            std::shared_ptr<LSignalAction> darkAct = std::make_shared<LSignalAction>(L"暗色风格");
            darkAct->setWName(darkAct->text());
            darkAct->sigTrigger.connect(this, &LTestMainWindow::onThemeDark);
            themeMenu->addAction(darkAct);

            menuBar->addMenu(themeMenu);
        }

        layout->addWidget(menuBar);
        std::shared_ptr<LTabBar> tabBar = makeInit<LTabBar>();
        tabBar->setName("标签栏");
        for(int i=0; i<4; ++i){
            std::wstring t = std::format(L"第{}项", i+1);
            tabBar->addItem(t);
        }
        //tabBar->setCanCloseAction(true);
        layout->addWidget(tabBar);

        // 标签栏对应的页面容器：用 tab 切换页面
        tabStack = makeInit<LStackedWidget>();
        tabStack->setName("标签页堆层");
        tabStack->setMinimumHeight(230);

        // 第1页：文本框（直接作为堆叠页，不再套一层容器）
        {
            edit = makeInit<LTextEdit>();
            edit->setName("文本框");
            edit->viewport()->setName("文本视图");
            tabStack->addWidget(edit);
        }

        // 第2页：LListWidget（列表 / 图标两种模式）
        {
            std::shared_ptr<LWidget> page = makeInit<LWidget>();
            page->setName("标签页2");

            std::shared_ptr<LVBoxLayout> pageLayout = std::make_shared<LVBoxLayout>();
            pageLayout->setName("标签页2布局");
            page->setLayout(pageLayout);

            std::shared_ptr<LHBoxLayout> btnRow = std::make_shared<LHBoxLayout>();
            btnRow->setName("listViewModeRow");

            std::shared_ptr<LButton> listBtn = makeInit<LButton>(L"列表模式");
            listBtn->setWName(L"列表模式");
            listBtn->setMinimumHeight(32);
            listBtn->sigClicked.connect(this, &LTestMainWindow::onListViewMode);
            btnRow->addWidget(listBtn);

            std::shared_ptr<LButton> iconBtn = makeInit<LButton>(L"图标模式");
            iconBtn->setWName(L"图标模式");
            iconBtn->setMinimumHeight(32);
            iconBtn->sigClicked.connect(this, &LTestMainWindow::onIconViewMode);
            btnRow->addWidget(iconBtn);

            pageLayout->addItem(btnRow);

            listWidget = makeInit<LListWidget>();
            listWidget->setName("列表控件");
            listWidget->setMinimumHeight(150);
            std::shared_ptr<LTexture> itemIcon = std::make_shared<LTexture>(":/resfile/logo.png");
            for(int n = 0; n < 24; ++n){
                // 列表项本身就是 action：可携带 userData（如文件路径），
                // 也能单独连接单击/双击信号，将来做文件浏览器可直接复用
                std::shared_ptr<LListWidgetItem> item = std::make_shared<LListWidgetItem>(
                        std::format(L"列表项 {}", n + 1), itemIcon, LVariant(n + 1));
                item->sigTrigger.connect(this, &LTestMainWindow::onListItemTrigger);
                listWidget->addItem(item);
            }
            listWidget->setCurrentRow(0);
            listWidget->setEditable(true);
            pageLayout->addWidget(listWidget);

            tabStack->addWidget(page);
        }

        // 第3页：LTreeWidget，分支节点展示层级，叶子结点携带多个属性列
        {
            std::shared_ptr<LWidget> page = makeInit<LWidget>();
            page->setName("标签页3");
            std::shared_ptr<LVBoxLayout> pageLayout = std::make_shared<LVBoxLayout>();
            page->setLayout(pageLayout);

            treeWidget = makeInit<LTreeWidget>();
            treeWidget->setName("树控件");
            treeWidget->setMinimumHeight(150);
            treeWidget->setColumnCount(4);
            treeWidget->setHeaderLabels({L"名称", L"类型", L"大小", L"修改时间"});
            treeWidget->setColumnWidth(0, 220);
            treeWidget->setColumnWidth(1, 120);
            treeWidget->setColumnWidth(2, 90);
            treeWidget->setColumnWidth(3, 140);
            // 可编辑性：控件总开关；第 0 列默认只读，这里放开名称列
            treeWidget->setEditable(true);
            treeWidget->setColumnEditable(0, true);
            treeWidget->sigItemClicked.connect(this, &LTestMainWindow::onTreeItemClicked);
            treeWidget->sigItemTextChanged.connect(this, &LTestMainWindow::onTreeItemTextChanged);

            std::shared_ptr<LTexture> treeIcon = std::make_shared<LTexture>(":/resfile/logo.png");
            std::shared_ptr<LTreeWidgetItem> root = treeWidget->addTopLevelItem(L"项目文件", treeIcon);

            std::shared_ptr<LTreeWidgetItem> src = std::make_shared<LTreeWidgetItem>(L"src", treeIcon);
            treeWidget->addChild(root, src);
            std::shared_ptr<LTreeWidgetItem> mainFile = treeWidget->addLeaf(src,
                    std::vector<std::wstring>{L"main.cpp", L"C++ 源文件", L"12 KB", L"2026-11-13"});
            // 单元格级：大小列只读，覆盖列级/行级默认
            mainFile->setCellEditable(2, false);
            treeWidget->addLeaf(src, std::vector<std::wstring>{
                    L"ltreewidget.cpp", L"C++ 源文件", L"28 KB", L"2026-11-13"});

            std::shared_ptr<LTreeWidgetItem> res = std::make_shared<LTreeWidgetItem>(L"res", treeIcon);
            treeWidget->addChild(root, res);
            treeWidget->addLeaf(res, std::vector<std::wstring>{
                    L"style.css", L"样式表", L"3 KB", L"2026-11-10"});
            treeWidget->addLeaf(root, std::vector<std::wstring>{
                    L"README.md", L"Markdown", L"2 KB", L"2026-11-12"});

            std::shared_ptr<LTreeWidgetItem> build = treeWidget->addTopLevelItem(L"构建输出", treeIcon);
            // 行级：该节点整行只读（不继承给子节点）
            build->setEditable(false);
            treeWidget->addLeaf(build, std::vector<std::wstring>{
                    L"lktestwin.exe", L"可执行文件", L"1.4 MB", L"2026-11-13"});

            treeWidget->setCurrentRow(0);
            pageLayout->addWidget(treeWidget);
            tabStack->addWidget(page);
        }

        // 第4页：LTableWidget，每个 item 一个单元格，支持相邻合并与可控编辑
        {
            std::shared_ptr<LWidget> page = makeInit<LWidget>();
            page->setName("标签页4");
            std::shared_ptr<LVBoxLayout> pageLayout = std::make_shared<LVBoxLayout>();
            page->setLayout(pageLayout);

            tableWidget = makeInit<LTableWidget>();
            tableWidget->setName("表格控件");
            tableWidget->setMinimumHeight(150);
            tableWidget->setRowColumnCount(6, 5);
            tableWidget->setHorizontalHeaderLabels({L"名称", L"类型", L"大小", L"状态", L"备注"});
            tableWidget->setColumnWidth(0, 130);
            tableWidget->setColumnWidth(1, 100);
            tableWidget->setColumnWidth(2, 80);
            tableWidget->setColumnWidth(3, 90);
            tableWidget->setColumnWidth(4, 120);
            // 尺寸策略：名称列按内容自适应，备注列拉伸填满，其余列可拖动表头分隔线调整
            tableWidget->setColumnResizeMode(0, LTableResizeMode::ResizeToContents);
            tableWidget->setColumnResizeMode(4, LTableResizeMode::Stretch);
            // 行级高度：标题行单独加高
            tableWidget->setRowHeight(0, 34);

            // 相邻单元格合并：第 1 行前两列合并；第 3~4 行的“状态”列纵向合并
            tableWidget->mergeCells(0, 0, 1, 2);
            tableWidget->mergeCells(2, 3, 2, 1);

            const std::vector<std::vector<std::wstring>> data = {
                    {L"项目清单",  L"",          L"",      L"",      L""},
                    {L"main.cpp",  L"C++ 源文件", L"12 KB", L"已完成", L""},
                    {L"style.css", L"样式表",    L"3 KB",  L"进行中", L"待评审"},
                    {L"logo.png",  L"图片",      L"24 KB", L"",      L"待替换"},
                    {L"README.md", L"Markdown",  L"2 KB",  L"已完成", L""},
                    {L"合计",      L"",          L"41 KB", L"",      L""}
            };
            std::shared_ptr<LTexture> cellIcon =
                    std::make_shared<LTexture>(":/resfile/logo.png");
            for(int r = 0; r < (int)data.size(); ++r){
                for(int c = 0; c < (int)data[r].size(); ++c){
                    if(tableWidget->isSpanned(r, c)){
                        continue;
                    }
                    // 一个单元格一个 item；名称列的条目带图标
                    std::shared_ptr<LTexture> icon =
                            (c == 0 && r >= 1 && r <= 4) ? cellIcon : nullptr;
                    tableWidget->setItem(r, c,
                            std::make_shared<LTableWidgetItem>(data[r][c], icon));
                }
            }

            // 可编辑性：控件总开关 + 列 + 行 + 单元格
            tableWidget->setEditable(true);
            tableWidget->setColumnEditable(4, false);   // 备注列只读
            tableWidget->setRowEditable(5, false);      // 合计行只读
            tableWidget->item(1, 2)->setEditable(false); // main.cpp 的“大小”只读
            tableWidget->sigCellChanged.connect(this, &LTestMainWindow::onTableCellChanged);
            tableWidget->setCurrentCell(1, 0);

            pageLayout->addWidget(tableWidget);
            tabStack->addWidget(page);
        }

        tabBar->sigCurrentChanged.connect(this, &LTestMainWindow::onTabChanged);
        layout->addWidget(tabStack);
        tabStack->setCurrentIndex(0);

        // 堆叠页首帧不会被父控件带到绘制（父重绘会跳过子控件区域），需要显式触发一次
        if(edit != nullptr){
            edit->update();
        }

        //std::shared_mutex
        std::shared_ptr<LHBoxLayout> btnLayout = std::make_shared<LHBoxLayout>();
        btnLayout->setName("btnLayout");
        std::shared_ptr<LGroupBox> groupBox = makeInit<LGroupBox>(L"组测试");
        // groupBox->setMinimumHeight(80);
        groupBox->setCheckable(true);
        groupBox->setChecked(true);

        groupBox->setLayout(btnLayout);

        layout->addWidget(groupBox);

        std::shared_ptr<LButton> btn1 = makeInit<LButton>(L"复制");
        btn1->setWName(btn1->text());
        btn1->setMinimumHeight(40);

        std::shared_ptr<LButton> btn2 = makeInit<LButton>(L"粘贴");
        btn2->setWName(btn2->text());
        btn2->setMinimumHeight(40);

        std::shared_ptr<LButton> btn3 = makeInit<LButton>(L"剪切");
        btn3->setWName(btn3->text());
        btn3->setMinimumHeight(40);

        btn1->sigClicked.connect(this, &LTestMainWindow::onBtn1);
        btn2->sigClicked.connect(this, &LTestMainWindow::onBtn2);
        btn3->sigClicked.connect(this, &LTestMainWindow::onBtn3);

        btnLayout->setSpacing(3);
        btnLayout->addWidget(btn1);
        btnLayout->addWidget(btn2);
        btnLayout->addWidget(btn3);

        std::shared_ptr<LMenu> menu = makeInit<LMenu>();
        menu->setWName(L"Test menu");


        std::shared_ptr<LSignalAction> test1 = std::make_shared<LSignalAction>(L"保存文本");
        test1->setWName(test1->text());
        test1->sigTrigger.connect(this, &LTestMainWindow::onAction1);
        menu->addAction(test1);

        std::shared_ptr<LSignalAction> test2 = std::make_shared<LSignalAction>(L"打开音乐");
        test2->setWName(test2->text());
        test2->sigTrigger.connect(this, &LTestMainWindow::onAction2);
        menu->addAction(test2);

        std::shared_ptr<LSignalAction> test3 = std::make_shared<LSignalAction>(L"数据库测试");
        test3->setWName(test2->text());
        test3->sigTrigger.connect(this, &LTestMainWindow::onAction3);
        menu->addAction(test3);


        std::shared_ptr<LSignalAction> test4 = std::make_shared<LSignalAction>(L"打开SVG");
        test4->setWName(test4->text());
        test4->sigTrigger.connect(this, &LTestMainWindow::onAction4);
        menu->addAction(test4);

        {
            std::shared_ptr<LMenu> subMenu = makeInit<LMenu>();
            useThemeSheet(subMenu);
            subMenu->setWName(L"submenu");
            std::shared_ptr<LSignalAction> subTest1 = std::make_shared<LSignalAction>(L"打开TCP服务");
            subTest1->setWName(subTest1->text());
            subTest1->sigTrigger.connect(this, &LTestMainWindow::onSubAction3);
            subMenu->addAction(subTest1);

            std::shared_ptr<LSignalAction> subTest2 = std::make_shared<LSignalAction>(L"发送TCP数据");
            subTest2->setWName(subTest2->text());
            subTest2->sigTrigger.connect(this, &LTestMainWindow::onSubAction4);
            subMenu->addAction(subTest2);

            std::shared_ptr<LSignalAction> test3 = std::make_shared<LSignalAction>(L"TCP测试");
            test3->setWName(test3->text());
            test3->setWidget(subMenu, true);
            menu->addAction(test3);
        }


        {
            std::shared_ptr<LMenu> subMenu = makeInit<LMenu>();
            useThemeSheet(subMenu);
            subMenu->setWName(L"submenu");
            std::shared_ptr<LSignalAction> subTest1 = std::make_shared<LSignalAction>(L"打开UDP服务");
            subTest1->setWName(subTest1->text());
            subTest1->sigTrigger.connect(this, &LTestMainWindow::onSubAction1);
            subMenu->addAction(subTest1);

            std::shared_ptr<LSignalAction> subTest2 = std::make_shared<LSignalAction>(L"发送UDP数据");
            subTest2->setWName(subTest2->text());
            subTest2->sigTrigger.connect(this, &LTestMainWindow::onSubAction2);
            subMenu->addAction(subTest2);

            std::shared_ptr<LSignalAction> test4 = std::make_shared<LSignalAction>(L"UDP测试");
            test4->setWName(test4->text());
            test4->setWidget(subMenu, true);
            menu->addAction(test4);
        }



        std::shared_ptr<LMenuButton> btn4 = makeInit<LMenuButton>(L"中国测试1");
        btn4->setWName(btn4->text());
        useThemeSheet(menu);
        btn4->setMenu(menu);
        btnLayout->addWidget(btn4);

        std::shared_ptr<LCheckBox> btn5 = makeInit<LCheckBox>(L"中国测试2");
        btn5->setWName(btn5->text());
        btnLayout->addWidget(btn5);

        std::shared_ptr<LHBoxLayout> radioLayout = std::make_shared<LHBoxLayout>();
        radioLayout->setName("radioLayout");
        layout->addItem(radioLayout);

        std::shared_ptr<LRadioButton> sigs[3];

        stack = makeInit<LStackedWidget>();
        stack->setName("堆层");
        for(int i=0; i<3; ++i){
            std::wstring t = std::format(L"第{}页", i+1);

            std::shared_ptr<LLabel> page = makeInit<LLabel>(t);
            page->setWName(t);
            page->setMaximumHeight(40);
            stack->addWidget(page);

            std::shared_ptr<LRadioButton> radio = makeInit<LRadioButton>(t);
            radio->setWName(L"单选：" + t);
            radio->setMinimumHeight(40);
            radioLayout->addWidget(radio);

            sigs[i] = radio;
        }

        stack->setMaximumHeight(40);

        sigs[0]->setChecked(true);
        sigs[0]->sigToggled.connect(this, &LTestMainWindow::onRadio1);
        sigs[1]->sigToggled.connect(this, &LTestMainWindow::onRadio2);
        sigs[2]->sigToggled.connect(this, &LTestMainWindow::onRadio3);

        std::shared_ptr<LComboBox> combo = makeInit<LComboBox>();
        combo->setName("选择栏");
        combo->menu()->setMaximumHeight(130);
        //combo->menu()->setUseScrollBar(false);
        radioLayout->addWidget(combo);
        for(int i=0; i<100; ++i){
            combo->addItem(std::wstring(L"选项") + std::to_wstring(i));
        }
        radioLayout->addWidget(combo);
        layout->addWidget(stack);

        std::shared_ptr<LHBoxLayout> hLayoutOther = std::make_shared<LHBoxLayout>();
        hLayoutOther->setName("hLayoutOther");
        layout->addItem(hLayoutOther);

        std::shared_ptr<LSpinBox> spinBox = makeInit<LSpinBox>();
        spinBox->setName("数值栏");
        hLayoutOther->addWidget(spinBox);
        spinBox->setMinimumHeight(40);


        std::shared_ptr<LComboEdit> comboEdit = makeInit<LComboEdit>();
        comboEdit->setName("可选编辑栏");
        comboEdit->setMinimumHeight(40);
        for(int i=0; i<4; ++i){
            comboEdit->addItem(std::wstring(L"选项") + std::to_wstring(i));
        }
        hLayoutOther->addWidget(comboEdit);

        lineEdit = makeInit<LLineEdit>();
        lineEdit->setName("编辑栏");
        layout->addWidget(lineEdit);
        lineEdit->setMinimumHeight(40);

        std::shared_ptr<LSplitter> splitter = makeInit<LSplitter>(LOrientation::Horizontal);
        for(int i=0; i<3; ++i){
            std::wstring t = std::format(L"第{}页", i+1);

            std::shared_ptr<LLabel> page = makeInit<LLabel>(t);
            page->setWName(t);
            page->setFixedHeight(40);
            splitter->addWidget(page);
        }

        std::shared_ptr<LSlider> f5 = makeInit<LSlider>();
        // f5->setTickPosition(LSlider::TicksBothSides);
        splitter->addWidget(f5);

        layout->addWidget(splitter);
    }


public:
    LTestMainWindow(std::shared_ptr<LWidget> parent):LMainWindow(parent, 0)
    {
        m_leftMargin = m_rightMargin = m_topMargin = m_bottomMargin = 3;
    }


    void onThemeLight()
    {
        switchTheme(false);
    }

    void onThemeDark()
    {
        switchTheme(true);
    }

    void onBtn1(bool){
        edit->copy();
    }

    void onBtn2(bool){
        edit->paste();

    }

    void onBtn3(bool){
        edit->cut();
    }

    void onSubAction1(){
        lWarn() << "Test sub action1";
        if(udp == nullptr){
            udp = std::make_shared<LUdpSocket>(3333);
            lWarn() << "Open udp: " << 3333;
            udp->sigReadyRead.connect(this, &LTestMainWindow::onUdpRead);
        }
    }

    void onUdpRead(){
        if(udp != nullptr){
            std::string ipAddr;
            uint16_t port;
            size_t bytes = udp->bytesAvaliable();
            while(bytes >0){
                std::vector<char> buf(bytes+1, char(0));
                const int recvLen = udp->recvfrom(buf.data(), (int)bytes, ipAddr, port);
                if(recvLen > 0){
                    lWarn() << "Recieve:" << buf.data() << ", from:" << ipAddr << ":" << port;
                    bytes = udp->bytesAvaliable();
                }else{
                    // recvLen == 0 且错误码为 0：只是没有数据（对端不可达的伪错误已被过滤）
                    if (udp->lastError() != 0) {
                        lWarn() << "Recieve failed:" << udp->lastError();
                    }
                    break;
                }
            }
        }
    }

    void onSubAction2(){
        lWarn() << "Test sub action2";
        if(udp != nullptr){
            std::string test = "Test udp send to";
            lWarn() << test << 4444;
            udp->sendto(test.data(), test.length(), "127.0.0.1", 4444);
        }
    }

    void onSubAction3(){
        lWarn() << "Test sub action3";
        if(server == nullptr){
            server = std::make_shared<LTcpServer>(3334);

            server->sigNewConnection.connect(this, &LTestMainWindow::onNewConnection);
            if(server->listen() == 0){
                lWarn() << "Open tcp: " << 3334;
            }else{
                lWarn() << "Open tcp error: " << server->lastError();
            }
        }
    }

    void onTcpRead(){
        if(peer != nullptr){
            size_t bytes = peer->bytesAvaliable();

            // 限制单次回调内的读取次数，避免底层可读字节数与实际读数不同步时死循环卡住界面
            int guard = 0;
            while(bytes > 0 && guard++ < 64){
                std::vector<char> buf(bytes + 1, char(0));
                const int n = peer->recv(buf.data(), (int)bytes);
                if(n > 0){
                    lWarn() << "Recieve:" << buf.data();
                    bytes = peer->bytesAvaliable();
                }else{
                    // 非阻塞模式下没有数据属于正常情况，不算错误
                    const auto err = peer->lastError();
#ifdef _WIN32
                    const bool wouldBlock = (err == WSAEWOULDBLOCK);
#else
                    const bool wouldBlock = (err == EAGAIN || err == EWOULDBLOCK);
#endif
                    if (!wouldBlock && err != 0) {
                        lWarn() << "Recieve failed:" << err;
                    }
                    break;
                }
            }
        }
    }

    void onNewConnection(){
        if(server != nullptr){
            LSockaddr address{};
            peer = server->accept(address);
            if(peer != nullptr){
                lWarn() << "New tcp connection.";
                peer->sigReadyRead.connect(this, &LTestMainWindow::onTcpRead);
            }
        }
    }

    void onSubAction4(){
        lWarn() << "Test sub action3";
        if(peer != nullptr){
            std::string test = "Test tcp send";
            peer->send(test.data(), test.length());
        }
    }


    void onAction1(){
        lWarn() << "Test action1";
        std::wstring saveFile = LFileDialog::getSaveFile(L"1234.txt", L"*.txt");
        if(!saveFile.empty()){
            lWarn() << saveFile;

        }
    }

    void onAction2(){
        lWarn() << "Test action2";
        std::wstring openFile = LFileDialog::getOpenFile(L"", L"*.wav;;*.mp3;;*.ogg");
        if(!openFile.empty()){
            lWarn() << openFile;
            lineEdit->setText(openFile);
            sound = std::make_shared<LSound>(openFile);
            if(sound != nullptr){
                sound->setVolume(0.2f);
                sound->play();
            }
        }
    }

    void onAction3(){
        lWarn() << "Test action3";
        std::shared_ptr<LSqlDatabase> db = std::make_shared<LSqlDatabase>("lksqlite");
        std::filesystem::path dbPath("test.db");
        if(dbPath.is_relative()){
            dbPath = std::filesystem::absolute(dbPath);
        }

        lWarn() << "Db path:" << dbPath.string();

        std::filesystem::path pPath= dbPath.parent_path();
        if(std::filesystem::exists(pPath) == false){
            if(std::filesystem::create_directory(pPath) == false){
                lWarn() << "Create path failed:" << pPath.string();
            }
        }

        db->m_dbName = dbPath.string();
        if(db->open()){
            LSqlQuery q("", db);

            std::list<std::string> tables = db->tables();

            if(tables.empty()){
                std::string sqlCmd = "CREATE TABLE E_DEVICE_USER ("\
                                        "DEVICE_ID varchar(30) PRIMARY KEY,"\
                                        "USER_CODE varchar(20),"\
                                        "PASSWORD varchar(100),"\
                                        "AUTO_LOGIN tinyint NOT NULL);";

                if( q.exec(sqlCmd) ) // success
                {
                }
                else // failed
                {
                    lWarn() << q.lastError().toString() << ": " << q.lastQuery();
                }
            }


            for(const std::string& table : tables){
                lWarn() << "Has table:" << table;
            }

            EDeviceUser user;
            std::vector<std::shared_ptr<EDeviceUser>> users = user.tlSelectAllFromDB<EDeviceUser>(db, user.getSelectSQL());

            if(users.empty()){
                user.device_id = "111";
                user.user_code = "admin";
                user.password = "123";
                user.auto_login = 1;
                user.saveData(db);
            }

            for(std::shared_ptr<EDeviceUser> user : users){
                lWarn() << "User:" << user->user_code;
            }


        }else{
            lWarn() << "Open db failed:" << db->lastError().toString();
        }
    }

    void onAction4(){
        lWarn() << "Test action4";
        std::wstring openFile = LFileDialog::getOpenFile(L"", L"*.svg");
        if(!openFile.empty()){
            lWarn() << openFile;
            lineEdit->setText(openFile);
            std::shared_ptr<LSvgRenderer> svgRender = std::make_shared<LSvgRenderer>();
            svgRender->load(openFile);

            std::shared_ptr<LImage> image = std::make_shared<LImage>(300, 300, LImage::Format::ABGR32);
            LPainter p(image);
            // p.setPen(LPenStyle::Solid);
            // p.setBrush(LBrush::SolidPattern);
            svgRender->render(&p);
            image->save("D:\\test.png");
        }
    }

    void onTabChanged(int index)
    {
        if(tabStack != nullptr && index >= 0){
            tabStack->setCurrentIndex(index);





            // 注意：repaintTree 只刷新子控件，页面自身需要单独 update，
            // 否则滚动区域类页面（如文本框）不会绘制
            std::shared_ptr<LWidget> page = tabStack->widget(index);
            if(page != nullptr){
                page->update();
                repaintTree(page);
            }




        }
    }

    void onListItemTrigger(std::shared_ptr<LListWidgetItem> item)
    {
        if(item != nullptr){
            lWarn() << "List item:" << item->text();
        }
    }

    void onTreeItemClicked(int row)
    {
        if(treeWidget != nullptr){
            std::shared_ptr<LTreeWidgetItem> item = treeWidget->itemAtRow(row);
            if(item != nullptr){
                lWarn() << "Tree item:" << item->text(0);
            }
        }
    }

    void onTreeItemTextChanged(std::shared_ptr<LTreeWidgetItem> item, int column)
    {
        if(item != nullptr){
            lWarn() << "Tree item changed, column:" << column
                    << " text:" << item->text(column);
        }
    }

    void onTableCellChanged(std::shared_ptr<LTableWidgetItem> item, int row, int column)
    {
        if(item != nullptr){
            lWarn() << "Table cell changed:" << row << "," << column
                    << " text:" << item->text();
        }
    }

    void onListViewMode(bool){
        if(listWidget != nullptr){
            listWidget->setViewMode(LListWidget::ListMode);
        }
    }

    void onIconViewMode(bool){
        if(listWidget != nullptr){
            listWidget->setViewMode(LListWidget::IconMode);
        }
    }

    void onRadio1(bool checked){
        if(checked){
            stack->setCurrentIndex(0);
        }
    }

    void onRadio2(bool checked){
        if(checked){
            stack->setCurrentIndex(1);
        }
    }

    void onRadio3(bool checked){
        if(checked){
            stack->setCurrentIndex(2);
        }
    }

};




int main(int argc, char* argv[]) {

#ifdef _WIN32
    LLoger::defaultLogEncode = GetACP() == CP_UTF8 ? "" : "gbk";
#else
    setlocale(LC_ALL, "");
    LLoger::defaultLogEncode = nl_langinfo(CODESET);
#endif


    // std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    // std::chrono::zoned_time x = std::chrono::zoned_time(std::chrono::current_zone(), now);

    // std::cout << "time:" <<now << std::endl;
    // std::cout << "time:" <<x << std::endl;

    // std::time_t now_tt = std::chrono::system_clock::to_time_t(now);
    // struct tm nowTm;
    // localtime_s(&nowTm, &now_tt);
    // std::cout <<"local tm:" <<  1900 +nowTm.tm_year << "-" <<  nowTm.tm_mon+1 << "-" <<  nowTm.tm_mday
    //           << " "<<  nowTm.tm_hour << ":" <<  nowTm.tm_min << ":" <<  nowTm.tm_sec << std::endl;

    // gmtime_s (&nowTm, &now_tt);
    // std::cout <<"gm tm:" <<  1900 +nowTm.tm_year << "-" <<  nowTm.tm_mon+1 << "-" <<  nowTm.tm_mday
    //           << " "<<  nowTm.tm_hour << ":" <<  nowTm.tm_min << ":" <<  nowTm.tm_sec << std::endl;

    // std::string t = std::format("{0:%F}T{0:%R}:{0:%OS}Z", now);
    // std::cout << "time:" <<t << std::endl;

    // std::chrono::system_clock::time_point o;


    // std::istringstream in{t};
    // in >> std::chrono::parse("%FT%TZ", o);
    // in.fail() ? std::cout << "Parse failed!\n" : std::cout << "Parse successed:"<< o << '\n';


    // std::cout << "UTC time:" <<std::format("{:%F %T %Z}", x) << std::endl;

    // std::chrono::zoned_time y("America/New_York", now);
    // std::cout << "UTC time:" <<std::format("{:%F %T %Z}", y) << std::endl;

    /*std::shared_ptr<LGradient> g = std::make_shared<LGradient>();
    // g->m_type = LGradient::Type::Linear;
    // g->m_linear.end.x = GRADIENT_STOPTABLE_SIZE;
    // g->m_linear.end.y = GRADIENT_STOPTABLE_SIZE;

    // g->m_spread = LGradient::Spread::Reflect;
    g->m_type = LGradient::Type::Radial;
    g->m_radial.center.radius = GRADIENT_STOPTABLE_SIZE/2;
    g->m_radial.center.x = GRADIENT_STOPTABLE_SIZE/2;
    g->m_radial.center.y = GRADIENT_STOPTABLE_SIZE/2;

    g->m_radial.focal.radius = 0;
    g->m_radial.focal.x = GRADIENT_STOPTABLE_SIZE/2;
    g->m_radial.focal.y = GRADIENT_STOPTABLE_SIZE/2;

    // g->m_spread = LGradient::Spread::Repeat;
    // g->m_type = LGradient::Type::Conical;
    // g->m_conical.angle = std::numbers::pi/4;
    // g->m_conical.center.x = GRADIENT_STOPTABLE_SIZE/2;
    // g->m_conical.center.y = GRADIENT_STOPTABLE_SIZE/2;

    g->m_stops.push_back(LGradientStop(0, LColor(LColorID::Red)));
    g->m_stops.push_back(LGradientStop(0.5, LColor(LColorID::Green)));
    g->m_stops.push_back(LGradientStop(1, LColor(LColorID::Blue)));
    std::shared_ptr<LImage> image = g->toImage(LSize<int>(300, 300));*/
    //image = image->scaled(LSize<int>(GRADIENT_STOPTABLE_SIZE, GRADIENT_STOPTABLE_SIZE/2));

    // std::shared_ptr<LImage> image = std::make_shared<LImage>(300, 300, LImage::Format::ABGR32);
    // LPainter p(image);
    // p.scale(3, 3);
    // std::shared_ptr<LFont> f = p.font();
    // std::shared_ptr<LFont> f= std::make_shared<LFont>(xf->family(), xf->pointSize()*4);
    // f->setPixelSize(f->pixelSize()*3);
    // p.setFont(f);
    // p.fillRect(image->rectF(), LColorID::White);
    // p.drawText(LPoint<double>(30, 30), L"点中国测试栅格绘图");
    //p.drawText(LPoint<double>(30, 30), L"中国");
    // std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    // path->addEllipse(image->rectF());


    std::shared_ptr<LApplication> app = makeInit<LApplication>(argc, argv);
    // p.fillPath(path, app->palette()->text());
    // image->save("E:\\test.png");

    // lWarn() << "确保输入输出流编码正常";

    std::shared_ptr<LMainWindow> win = makeInit<LTestMainWindow>(nullptr);
    win->setName("主窗口");
    win->show();
    int ret = app->exec();
    return ret;
}

