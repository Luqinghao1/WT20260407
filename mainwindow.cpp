/*
 * 文件名: mainwindow.cpp
 * 文件作用: 试井解释软件的主窗口类实现文件
 * 功能描述:
 * 1. 负责应用程序全局的UI加载、页面布局和各个子页面的实例化连接。
 * 2. 实现了左侧导航栏的逻辑控制、界面样式设置和页面切换功能。
 * 3. 实现了安全关闭拦截功能，确保用户在点击右上角关闭时可以保存工程数据。
 * 4. 优化了背景颜色的应用，彻底解决因缩放引起的导航区与功能区分割线错位的问题。
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "navbtn.h"
#include "wt_projectwidget.h"
#include "wt_datawidget.h"
#include "modelmanager.h"
#include "modelparameter.h"
#include "wt_plottingwidget.h"
#include "fittingpage.h"
#include "settingswidget.h"
#include "pressurederivativecalculator.h"
#include "multidatafittingpage.h"

#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QStandardItemModel>
#include <QTimer>
#include <QSpacerItem>
#include <QStackedWidget>
#include <cmath>
#include <QStatusBar>

/**
 * @brief 获取全局统一的消息提示框样式
 * @return QString 包含标准按钮、背景色、文字颜色的QSS字符串
 */
static QString getGlobalMessageBoxStyle()
{
    return "QMessageBox { background-color: #ffffff; color: #000000; }"
           "QLabel { color: #000000; background-color: transparent; }"
           "QPushButton { "
           "   color: #000000; "
           "   background-color: #f0f0f0; "
           "   border: 1px solid #c0c0c0; "
           "   border-radius: 3px; "
           "   padding: 5px 15px; "
           "   min-width: 60px;"
           "}"
           "QPushButton:hover { background-color: #e0e0e0; }"
           "QPushButton:pressed { background-color: #d0d0d0; }";
}

// 构造函数，初始化UI并设置窗口基础属性
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isProjectLoaded(false)
{
    ui->setupUi(this); // 加载UI文件
    this->setWindowTitle("PWT页岩油多级压裂水平井试井解释软件");
    this->setMinimumWidth(1024); // 限制最小宽度，防止缩放过度导致界面重叠

    init(); // 调用初始化函数，构建导航及子界面
}

// 析构函数，释放UI指针
MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * @brief 拦截主界面的全局关闭事件
 * @param event 传递过来的关闭事件对象
 * 功能说明：当用户点击右上角叉号退出软件时，提供“保存并退出”、“直接退出”、“取消”三种业务选择。
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    // 如果没有打开过项目，则直接放行，关闭软件
    if (!m_isProjectLoaded) {
        event->accept();
        return;
    }

    // 实例化并弹出确认提示框
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("退出系统");
    msgBox.setText("当前有项目正在运行，确定要退出吗？");
    msgBox.setInformativeText("建议在退出前保存当前项目。");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStyleSheet(getMessageBoxStyle());

    // 添加自定义的三个按钮
    QPushButton *saveExitBtn = msgBox.addButton("保存并退出", QMessageBox::YesRole);
    QPushButton *directExitBtn = msgBox.addButton("直接退出", QMessageBox::NoRole);
    msgBox.addButton("取消", QMessageBox::RejectRole);

    msgBox.exec(); // 阻塞执行对话框

    // 根据用户点击的按钮进行业务判断
    if (msgBox.clickedButton() == saveExitBtn) {
        // 用户选择保存并退出，触发单例模式的数据持久化保存
        ModelParameter::instance()->saveProject();
        event->accept();
    } else if (msgBox.clickedButton() == directExitBtn) {
        // 用户选择直接退出，丢弃未保存更改
        event->accept();
    } else {
        // 用户选择取消，忽略关闭事件，返回软件界面
        event->ignore();
    }
}

/**
 * @brief 核心初始化函数
 * 功能说明：初始化左侧导航栏、时间显示标签，并实例化右侧所有的功能子界面。
 */
void MainWindow::init()
{
    // 循环创建7个左侧导航按钮
    for(int i = 0 ; i < 7; i++)
    {
        NavBtn* btn = new NavBtn(ui->widgetNav);
        btn->setMinimumWidth(110);
        btn->setIndex(i); // 设置绑定的页面索引
        btn->setStyleSheet("color: black;"); // 默认字体颜色设为黑色

        // 根据索引配置按钮的图标和文字名称
        switch (i) {
        case 0:
            btn->setPicName("border-image: url(:/new/prefix1/Resource/X0.png);", tr("项目"));
            btn->setClickedStyle(); // 默认选中第0个按钮(项目页)
            ui->stackedWidget->setCurrentIndex(0);
            break;
        case 1: btn->setPicName("border-image: url(:/new/prefix1/Resource/X1.png);", tr("数据")); break;
        case 2: btn->setPicName("border-image: url(:/new/prefix1/Resource/X2.png);", tr("图表")); break;
        case 3: btn->setPicName("border-image: url(:/new/prefix1/Resource/X3.png);", tr("模型")); break;
        case 4: btn->setPicName("border-image: url(:/new/prefix1/Resource/X4.png);", tr("拟合")); break;
        case 5: btn->setPicName("border-image: url(:/new/prefix1/Resource/X5.png);", tr("多数据")); break;
        case 6: btn->setPicName("border-image: url(:/new/prefix1/Resource/X6.png);", tr("设置")); break;
        }
        // 将按钮名称和指针插入哈希表管理
        m_NavBtnMap.insert(btn->getName(), btn);
        ui->verticalLayoutNav->addWidget(btn); // 将按钮添加到导航区垂直布局中

        // 连接导航按钮的点击信号到页面切换槽函数
        connect(btn, &NavBtn::sigClicked, [=](QString name)
                {
                    int targetIndex = m_NavBtnMap.value(name)->getIndex();

                    // 检查逻辑：如果没有打开工程项目，禁止进入除项目、设置外的核心页面
                    if ((targetIndex >= 1 && targetIndex <= 5) && !m_isProjectLoaded) {
                        QMessageBox msgBox;
                        msgBox.setWindowTitle("提示");
                        msgBox.setText("当前无活动项目，请先在“项目”界面新建或打开一个项目！");
                        msgBox.setIcon(QMessageBox::Warning);
                        msgBox.setStyleSheet(getMessageBoxStyle());
                        msgBox.exec();
                        return;
                    }

                    // 遍历所有导航按钮，恢复未被点击按钮的默认样式
                    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
                    while (item != m_NavBtnMap.end()) {
                        if(item.key() != name) ((NavBtn*)(item.value()))->setNormalStyle();
                        item++;
                    }

                    // 切换右侧StackedWidget的页面索引
                    ui->stackedWidget->setCurrentIndex(targetIndex);

                    // 如果点击的是图表页，主动触发一次数据同步
                    if (name == tr("图表")) {
                        onTransferDataToPlotting();
                    }
                });
    }

    // 在导航栏最下方添加一个弹簧，将所有导航按钮往上顶
    QSpacerItem* verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    ui->verticalLayoutNav->addSpacerItem(verticalSpacer);

    // 初始化左下角的时间标签
    ui->labelTime->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss").replace(" ","\n"));

    // 连接定时器更新时间的槽函数
    connect(&m_timer, &QTimer::timeout, [=] {
        ui->labelTime->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").replace(" ","\n"));
        ui->labelTime->setStyleSheet("color: black;");
    });
    m_timer.start(1000); // 设置定时器频率为1秒

    // ---- 以下为实例化并装载右侧各个功能页面的流程 ----

    // 1. 初始化项目管理页面
    m_ProjectWidget = new WT_ProjectWidget(ui->pageMonitor);
    ui->verticalLayoutMonitor->addWidget(m_ProjectWidget);
    connect(m_ProjectWidget, &WT_ProjectWidget::projectOpened, this, &MainWindow::onProjectOpened);
    connect(m_ProjectWidget, &WT_ProjectWidget::projectClosed, this, &MainWindow::onProjectClosed);
    connect(m_ProjectWidget, &WT_ProjectWidget::fileLoaded, this, &MainWindow::onFileLoaded);

    // 2. 初始化数据编辑页面
    m_DataEditorWidget = new WT_DataWidget(ui->pageHand);
    ui->verticalLayoutHandle->addWidget(m_DataEditorWidget);
    connect(m_DataEditorWidget, &WT_DataWidget::fileChanged, this, &MainWindow::onFileLoaded);
    connect(m_DataEditorWidget, &WT_DataWidget::dataChanged, this, &MainWindow::onDataEditorDataChanged);

    // 3. 初始化图表展示页面
    m_PlottingWidget = new WT_PlottingWidget(ui->pageData);
    ui->verticalLayout_2->addWidget(m_PlottingWidget);
    connect(m_PlottingWidget, &WT_PlottingWidget::viewExportedFile, this, &MainWindow::onViewExportedFile);

    // 4. 初始化模型参数页面
    m_ModelManager = new ModelManager(this);
    m_ModelManager->initializeModels(ui->pageParamter);
    connect(m_ModelManager, &ModelManager::calculationCompleted, this, &MainWindow::onModelCalculationCompleted);

    // 5. 初始化单图拟合页面
    if (ui->pageFitting && ui->verticalLayoutFitting) {
        m_FittingPage = new FittingPage(ui->pageFitting);
        ui->verticalLayoutFitting->addWidget(m_FittingPage);
        m_FittingPage->setModelManager(m_ModelManager);
    }

    // 6. 初始化多数据预测/拟合页面
    if (ui->pagePrediction && ui->verticalLayoutPrediction) {
        multidatafittingpage* multiPage = new multidatafittingpage(ui->pagePrediction);
        multiPage->setObjectName("MultiDataFittingPage");
        ui->verticalLayoutPrediction->addWidget(multiPage);
        multiPage->setModelManager(m_ModelManager);
    }

    // 7. 初始化系统设置页面
    m_SettingsWidget = new SettingsWidget(ui->pageAlarm);
    ui->verticalLayout_3->addWidget(m_SettingsWidget);
    connect(m_SettingsWidget, &SettingsWidget::settingsChanged, this, &MainWindow::onSystemSettingsChanged);

    // 调用各个页面的延后初始化空接口(用于扩展)
    initProjectForm();
    initDataEditorForm();
    initModelForm();
    initPlottingForm();
    initFittingForm();
    initPredictionForm();
}

// 预留的扩展接口函数
void MainWindow::initProjectForm() {}
void MainWindow::initDataEditorForm() {}
void MainWindow::initModelForm() {}
void MainWindow::initPlottingForm() {}
void MainWindow::initFittingForm() {}
void MainWindow::initPredictionForm() {}

/**
 * @brief 项目成功打开/新建后的槽函数
 * @param isNew 标识是新建项目还是加载原有项目
 * 功能说明：项目载入后，初始化全局状态并向下游所有模块广播数据更新事件。
 */
void MainWindow::onProjectOpened(bool isNew)
{
    m_isProjectLoaded = true; // 激活工程状态

    // 通知模型管理器更新基础参数
    if (m_ModelManager) m_ModelManager->updateAllModelsBasicParameters();

    if (m_DataEditorWidget) {
        // 如果是读取已有项目，将历史数据表加载出来
        if (!isNew) m_DataEditorWidget->loadFromProjectData();

        // 将加载出的数据模型指针传递给拟合页面和多数据页面
        if (m_FittingPage) m_FittingPage->setProjectDataModels(m_DataEditorWidget->getAllDataModels());

        multidatafittingpage* multiPage = ui->pagePrediction->findChild<multidatafittingpage*>("MultiDataFittingPage");
        if (multiPage) {
            multiPage->setProjectDataModels(m_DataEditorWidget->getAllDataModels());
        }
    }

    if (m_FittingPage) {
        m_FittingPage->updateBasicParameters();
        m_FittingPage->loadAllFittingStates();
    }

    // 通知图表页加载数据
    if (m_PlottingWidget) m_PlottingWidget->loadProjectData();

    // 更新导航按钮状态到当前页
    updateNavigationState();

    // 弹出对应的成功提示框
    QString title = isNew ? "新建项目成功" : "加载项目成功";
    QString text = isNew ? "新项目已创建。\n基础参数已初始化，您可以开始进行数据录入或模型计算。"
                         : "项目文件加载完成。\n历史参数、数据及图表分析状态已完整恢复。";

    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStyleSheet(getMessageBoxStyle());
    msgBox.exec();
}

/**
 * @brief 项目关闭时的槽函数
 * 功能说明：关闭工程，清空内存数据，并重置所有页面的分析状态。
 */
void MainWindow::onProjectClosed()
{
    m_isProjectLoaded = false;
    m_hasValidData = false;

    // 清空下游各个子模块的数据
    if (m_DataEditorWidget) m_DataEditorWidget->clearAllData();
    if (m_PlottingWidget) m_PlottingWidget->clearAllPlots();
    if (m_FittingPage) m_FittingPage->resetAnalysis();
    if (m_ModelManager) m_ModelManager->clearCache();
    // 清空单例中的全局数据
    ModelParameter::instance()->resetAllData();

    multidatafittingpage* multiPage = ui->pagePrediction->findChild<multidatafittingpage*>("MultiDataFittingPage");
    if (multiPage) {
        multiPage->setProjectDataModels(QMap<QString, QStandardItemModel*>());
    }

    // 强行跳转回索引0(项目页)
    ui->stackedWidget->setCurrentIndex(0);
    updateNavigationState();

    QMessageBox msgBox;
    msgBox.setWindowTitle("提示");
    msgBox.setText("项目已保存并关闭。");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStyleSheet(getMessageBoxStyle());
    msgBox.exec();
}

/**
 * @brief 当有外部文件被加载进系统时的槽函数
 * @param filePath 文件的绝对路径
 * @param fileType 文件类型(扩展名或自动)
 */
void MainWindow::onFileLoaded(const QString& filePath, const QString& fileType)
{
    // 如果无工程强行打开数据，予以警告并拦截
    if (!m_isProjectLoaded) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("警告");
        msgBox.setText("请先创建或打开项目！");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(getMessageBoxStyle());
        msgBox.exec();
        return;
    }

    // 跳转至数据编辑页(索引1)
    ui->stackedWidget->setCurrentIndex(1);

    // 同步更新左侧导航栏的高亮显示
    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
    while (item != m_NavBtnMap.end()) {
        ((NavBtn*)(item.value()))->setNormalStyle();
        if(item.key() == tr("数据")) ((NavBtn*)(item.value()))->setClickedStyle();
        item++;
    }

    // 调用数据编辑器加载该文件
    if (m_DataEditorWidget && sender() != m_DataEditorWidget) {
        m_DataEditorWidget->loadData(filePath, fileType);
    }

    // 将最新载入的数据模型同步至分析及预测模块
    if (m_DataEditorWidget) {
        if (m_FittingPage) {
            m_FittingPage->setProjectDataModels(m_DataEditorWidget->getAllDataModels());
        }

        multidatafittingpage* multiPage = ui->pagePrediction->findChild<multidatafittingpage*>("MultiDataFittingPage");
        if (multiPage) {
            multiPage->setProjectDataModels(m_DataEditorWidget->getAllDataModels());
        }
    }

    m_hasValidData = true;
    // 延迟1秒通知图表区准备刷新数据，防止阻塞UI线程
    QTimer::singleShot(1000, this, &MainWindow::onDataReadyForPlotting);
}

/**
 * @brief 在图表内导出处理后文件，随后提供预览跳转的回调
 * @param filePath 导出的文件路径
 */
void MainWindow::onViewExportedFile(const QString& filePath)
{
    // 切换至数据展示页(索引1)
    ui->stackedWidget->setCurrentIndex(1);

    // 更新导航样式为数据项高亮
    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
    while (item != m_NavBtnMap.end()) {
        ((NavBtn*)(item.value()))->setNormalStyle();
        if(item.key() == tr("数据")) ((NavBtn*)(item.value()))->setClickedStyle();
        item++;
    }

    // 在数据编辑器中打开这个导出的文件
    if (m_DataEditorWidget) {
        m_DataEditorWidget->loadData(filePath, "auto");
    }
}

// 预留的图表分析完成回调，暂不处理
void MainWindow::onPlotAnalysisCompleted(const QString &analysisType, const QMap<QString, double> &results) { }

// 数据载入完毕后触发向图表模块输送数据
void MainWindow::onDataReadyForPlotting() {
    transferDataFromEditorToPlotting();
}

// 手动触发将数据模块内容强制覆盖到图表区
void MainWindow::onTransferDataToPlotting()
{
    if (!hasDataLoaded()) return;
    transferDataFromEditorToPlotting();
}

/**
 * @brief 监听数据编辑区数据被修改的情况
 * 如果当前正处在图表页面，数据修改后实时触发图表的数据同步，做到所见即所得。
 */
void MainWindow::onDataEditorDataChanged()
{
    if (ui->stackedWidget->currentIndex() == 2) {
        transferDataFromEditorToPlotting();
    }
    m_hasValidData = hasDataLoaded();
}

// 预留的模型运算完成回调
void MainWindow::onModelCalculationCompleted(const QString &analysisType, const QMap<QString, double> &results) { }

/**
 * @brief 核心功能：将数据编辑器中的压降计算及导数预处理数据传输至拟合页面
 */
void MainWindow::transferDataToFitting()
{
    if (!m_FittingPage || !m_DataEditorWidget) return;

    QStandardItemModel* model = m_DataEditorWidget->getDataModel();
    if (!model || model->rowCount() == 0) return;

    QVector<double> tVec, pVec, dVec;
    double p_initial = 0.0;

    // 获取初试压力点，通常取第一条有效行
    for(int r = 0; r < model->rowCount(); ++r) {
        double p = model->index(r, 1).data().toDouble();
        if (std::abs(p) > 1e-6) { p_initial = p; break; }
    }

    // 遍历数据表格，求出压差和时间向量
    for(int r = 0; r < model->rowCount(); ++r) {
        double t = model->index(r, 0).data().toDouble();
        double p_raw = model->index(r, 1).data().toDouble();
        if (t > 0) {
            tVec.append(t);
            pVec.append(std::abs(p_raw - p_initial));
        }
    }

    // 如果满足三个点以上，进行导数计算(Bourdet算法)，否则全部填充0
    if (tVec.size() > 2) {
        dVec = PressureDerivativeCalculator::calculateBourdetDerivative(tVec, pVec, 0.1);
    } else {
        dVec.resize(tVec.size());
        dVec.fill(0.0);
    }

    // 将预处理数据注入拟合模块
    m_FittingPage->setObservedDataToCurrent(tVec, pVec, dVec);
}

/**
 * @brief 在状态栏显示运算进度
 * @param progress 百分比进度值 (0~100)
 */
void MainWindow::onFittingProgressChanged(int progress)
{
    if (this->statusBar()) {
        this->statusBar()->showMessage(QString("正在拟合... %1%").arg(progress));
        if(progress >= 100) this->statusBar()->showMessage("拟合完成", 5000);
    }
}

// 预留的设置项更新回调
void MainWindow::onSystemSettingsChanged() {}
void MainWindow::onPerformanceSettingsChanged() {}

// 获取底层编辑器使用的QStandardItemModel对象指针
QStandardItemModel* MainWindow::getDataEditorModel() const
{
    if (!m_DataEditorWidget) return nullptr;
    return m_DataEditorWidget->getDataModel();
}

// 获取当前编辑中文件的名称
QString MainWindow::getCurrentFileName() const
{
    if (!m_DataEditorWidget) return QString();
    return m_DataEditorWidget->getCurrentFileName();
}

// 判断是否有有效数据文件存在
bool MainWindow::hasDataLoaded()
{
    if (!m_DataEditorWidget) return false;
    return m_DataEditorWidget->hasData();
}

/**
 * @brief 内部方法：收集编辑器中所有的模型文件并发送至图表组件
 */
void MainWindow::transferDataFromEditorToPlotting()
{
    if (!m_DataEditorWidget || !m_PlottingWidget) return;
    QMap<QString, QStandardItemModel*> models = m_DataEditorWidget->getAllDataModels();
    m_PlottingWidget->setDataModels(models);
    if (!models.isEmpty()) m_hasValidData = true;
}

// 刷新导航按钮使得只有"项目"按钮保持高亮(重置操作)
void MainWindow::updateNavigationState()
{
    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
    while (item != m_NavBtnMap.end()) {
        ((NavBtn*)(item.value()))->setNormalStyle();
        if(item.key() == tr("项目")) ((NavBtn*)(item.value()))->setClickedStyle();
        item++;
    }
}

// 获取全局样式配置供外部调用
QString MainWindow::getMessageBoxStyle() const
{
    return getGlobalMessageBoxStyle();
}
