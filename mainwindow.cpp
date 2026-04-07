/*
 * 文件名: mainwindow.cpp
 * 文件作用: 主窗口类实现文件
 * 功能描述:
 * 1. 负责应用程序的整体初始化和页面布局，建立各个子界面的连接。
 * 2. 实现了左侧导航栏的逻辑控制和页面切换。
 * 3. 【新增】统一了关闭事件(closeEvent)，确保点击右上角关闭按钮时弹出包含保存功能的提示框。
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isProjectLoaded(false)
{
    ui->setupUi(this);
    this->setWindowTitle("PWT页岩油多级压裂水平井试井解释软件");
    this->setMinimumWidth(1024);

    init();
}

MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * @brief 【核心增加】：全局拦截窗口的关闭请求
 * 提供“保存并退出”、“直接退出”、“取消”三种业务选择。
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    // 如果没有打开过项目，则直接放行，关闭软件
    if (!m_isProjectLoaded) {
        event->accept();
        return;
    }

    // 弹窗提示
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("退出系统");
    msgBox.setText("当前有项目正在运行，确定要退出吗？");
    msgBox.setInformativeText("建议在退出前保存当前项目。");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStyleSheet(getMessageBoxStyle());

    QPushButton *saveExitBtn = msgBox.addButton("保存并退出", QMessageBox::YesRole);
    QPushButton *directExitBtn = msgBox.addButton("直接退出", QMessageBox::NoRole);
    msgBox.addButton("取消", QMessageBox::RejectRole);

    msgBox.exec();

    // 业务判断逻辑
    if (msgBox.clickedButton() == saveExitBtn) {
        ModelParameter::instance()->saveProject(); // 持久化落地数据
        event->accept();
    } else if (msgBox.clickedButton() == directExitBtn) {
        event->accept();
    } else {
        event->ignore(); // 拦截窗口关闭，返回软件
    }
}

void MainWindow::init()
{
    for(int i = 0 ; i < 7; i++)
    {
        NavBtn* btn = new NavBtn(ui->widgetNav);
        btn->setMinimumWidth(110);
        btn->setIndex(i);
        btn->setStyleSheet("color: black;");

        switch (i) {
        case 0:
            btn->setPicName("border-image: url(:/new/prefix1/Resource/X0.png);", tr("项目"));
            btn->setClickedStyle();
            ui->stackedWidget->setCurrentIndex(0);
            break;
        case 1: btn->setPicName("border-image: url(:/new/prefix1/Resource/X1.png);", tr("数据")); break;
        case 2: btn->setPicName("border-image: url(:/new/prefix1/Resource/X2.png);", tr("图表")); break;
        case 3: btn->setPicName("border-image: url(:/new/prefix1/Resource/X3.png);", tr("模型")); break;
        case 4: btn->setPicName("border-image: url(:/new/prefix1/Resource/X4.png);", tr("拟合")); break;
        case 5: btn->setPicName("border-image: url(:/new/prefix1/Resource/X5.png);", tr("多数据")); break;
        case 6: btn->setPicName("border-image: url(:/new/prefix1/Resource/X6.png);", tr("设置")); break;
        }
        m_NavBtnMap.insert(btn->getName(), btn);
        ui->verticalLayoutNav->addWidget(btn);

        connect(btn, &NavBtn::sigClicked, [=](QString name)
                {
                    int targetIndex = m_NavBtnMap.value(name)->getIndex();

                    if ((targetIndex >= 1 && targetIndex <= 5) && !m_isProjectLoaded) {
                        QMessageBox msgBox;
                        msgBox.setWindowTitle("提示");
                        msgBox.setText("当前无活动项目，请先在“项目”界面新建或打开一个项目！");
                        msgBox.setIcon(QMessageBox::Warning);
                        msgBox.setStyleSheet(getMessageBoxStyle());
                        msgBox.exec();
                        return;
                    }

                    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
                    while (item != m_NavBtnMap.end()) {
                        if(item.key() != name) ((NavBtn*)(item.value()))->setNormalStyle();
                        item++;
                    }

                    ui->stackedWidget->setCurrentIndex(targetIndex);

                    if (name == tr("图表")) {
                        onTransferDataToPlotting();
                    }
                });
    }

    QSpacerItem* verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    ui->verticalLayoutNav->addSpacerItem(verticalSpacer);

    ui->labelTime->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss").replace(" ","\n"));
    connect(&m_timer, &QTimer::timeout, [=] {
        ui->labelTime->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").replace(" ","\n"));
        ui->labelTime->setStyleSheet("color: black;");
    });
    m_timer.start(1000);

    m_ProjectWidget = new WT_ProjectWidget(ui->pageMonitor);
    ui->verticalLayoutMonitor->addWidget(m_ProjectWidget);
    connect(m_ProjectWidget, &WT_ProjectWidget::projectOpened, this, &MainWindow::onProjectOpened);
    connect(m_ProjectWidget, &WT_ProjectWidget::projectClosed, this, &MainWindow::onProjectClosed);
    connect(m_ProjectWidget, &WT_ProjectWidget::fileLoaded, this, &MainWindow::onFileLoaded);

    m_DataEditorWidget = new WT_DataWidget(ui->pageHand);
    ui->verticalLayoutHandle->addWidget(m_DataEditorWidget);
    connect(m_DataEditorWidget, &WT_DataWidget::fileChanged, this, &MainWindow::onFileLoaded);
    connect(m_DataEditorWidget, &WT_DataWidget::dataChanged, this, &MainWindow::onDataEditorDataChanged);

    m_PlottingWidget = new WT_PlottingWidget(ui->pageData);
    ui->verticalLayout_2->addWidget(m_PlottingWidget);
    connect(m_PlottingWidget, &WT_PlottingWidget::viewExportedFile, this, &MainWindow::onViewExportedFile);

    m_ModelManager = new ModelManager(this);
    m_ModelManager->initializeModels(ui->pageParamter);
    connect(m_ModelManager, &ModelManager::calculationCompleted, this, &MainWindow::onModelCalculationCompleted);

    if (ui->pageFitting && ui->verticalLayoutFitting) {
        m_FittingPage = new FittingPage(ui->pageFitting);
        ui->verticalLayoutFitting->addWidget(m_FittingPage);
        m_FittingPage->setModelManager(m_ModelManager);
    }

    if (ui->pagePrediction && ui->verticalLayoutPrediction) {
        multidatafittingpage* multiPage = new multidatafittingpage(ui->pagePrediction);
        multiPage->setObjectName("MultiDataFittingPage");
        ui->verticalLayoutPrediction->addWidget(multiPage);
        multiPage->setModelManager(m_ModelManager);
    }

    m_SettingsWidget = new SettingsWidget(ui->pageAlarm);
    ui->verticalLayout_3->addWidget(m_SettingsWidget);
    connect(m_SettingsWidget, &SettingsWidget::settingsChanged, this, &MainWindow::onSystemSettingsChanged);

    initProjectForm();
    initDataEditorForm();
    initModelForm();
    initPlottingForm();
    initFittingForm();
    initPredictionForm();
}

void MainWindow::initProjectForm() {}
void MainWindow::initDataEditorForm() {}
void MainWindow::initModelForm() {}
void MainWindow::initPlottingForm() {}
void MainWindow::initFittingForm() {}
void MainWindow::initPredictionForm() {}

void MainWindow::onProjectOpened(bool isNew)
{
    m_isProjectLoaded = true;

    if (m_ModelManager) m_ModelManager->updateAllModelsBasicParameters();

    if (m_DataEditorWidget) {
        if (!isNew) m_DataEditorWidget->loadFromProjectData();

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

    if (m_PlottingWidget) m_PlottingWidget->loadProjectData();

    updateNavigationState();

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

void MainWindow::onProjectClosed()
{
    m_isProjectLoaded = false;
    m_hasValidData = false;

    if (m_DataEditorWidget) m_DataEditorWidget->clearAllData();
    if (m_PlottingWidget) m_PlottingWidget->clearAllPlots();
    if (m_FittingPage) m_FittingPage->resetAnalysis();
    if (m_ModelManager) m_ModelManager->clearCache();
    ModelParameter::instance()->resetAllData();

    multidatafittingpage* multiPage = ui->pagePrediction->findChild<multidatafittingpage*>("MultiDataFittingPage");
    if (multiPage) {
        multiPage->setProjectDataModels(QMap<QString, QStandardItemModel*>());
    }

    ui->stackedWidget->setCurrentIndex(0);
    updateNavigationState();

    QMessageBox msgBox;
    msgBox.setWindowTitle("提示");
    msgBox.setText("项目已保存并关闭。");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStyleSheet(getMessageBoxStyle());
    msgBox.exec();
}

void MainWindow::onFileLoaded(const QString& filePath, const QString& fileType)
{
    if (!m_isProjectLoaded) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("警告");
        msgBox.setText("请先创建或打开项目！");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(getMessageBoxStyle());
        msgBox.exec();
        return;
    }

    ui->stackedWidget->setCurrentIndex(1);

    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
    while (item != m_NavBtnMap.end()) {
        ((NavBtn*)(item.value()))->setNormalStyle();
        if(item.key() == tr("数据")) ((NavBtn*)(item.value()))->setClickedStyle();
        item++;
    }

    if (m_DataEditorWidget && sender() != m_DataEditorWidget) {
        m_DataEditorWidget->loadData(filePath, fileType);
    }

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
    QTimer::singleShot(1000, this, &MainWindow::onDataReadyForPlotting);
}

void MainWindow::onViewExportedFile(const QString& filePath)
{
    ui->stackedWidget->setCurrentIndex(1);

    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
    while (item != m_NavBtnMap.end()) {
        ((NavBtn*)(item.value()))->setNormalStyle();
        if(item.key() == tr("数据")) ((NavBtn*)(item.value()))->setClickedStyle();
        item++;
    }

    if (m_DataEditorWidget) {
        m_DataEditorWidget->loadData(filePath, "auto");
    }
}

void MainWindow::onPlotAnalysisCompleted(const QString &analysisType, const QMap<QString, double> &results) { }

void MainWindow::onDataReadyForPlotting() {
    transferDataFromEditorToPlotting();
}

void MainWindow::onTransferDataToPlotting()
{
    if (!hasDataLoaded()) return;
    transferDataFromEditorToPlotting();
}

void MainWindow::onDataEditorDataChanged()
{
    if (ui->stackedWidget->currentIndex() == 2) {
        transferDataFromEditorToPlotting();
    }
    m_hasValidData = hasDataLoaded();
}

void MainWindow::onModelCalculationCompleted(const QString &analysisType, const QMap<QString, double> &results) { }

void MainWindow::transferDataToFitting()
{
    if (!m_FittingPage || !m_DataEditorWidget) return;

    QStandardItemModel* model = m_DataEditorWidget->getDataModel();
    if (!model || model->rowCount() == 0) return;

    QVector<double> tVec, pVec, dVec;
    double p_initial = 0.0;

    for(int r = 0; r < model->rowCount(); ++r) {
        double p = model->index(r, 1).data().toDouble();
        if (std::abs(p) > 1e-6) { p_initial = p; break; }
    }

    for(int r = 0; r < model->rowCount(); ++r) {
        double t = model->index(r, 0).data().toDouble();
        double p_raw = model->index(r, 1).data().toDouble();
        if (t > 0) {
            tVec.append(t);
            pVec.append(std::abs(p_raw - p_initial));
        }
    }

    if (tVec.size() > 2) {
        dVec = PressureDerivativeCalculator::calculateBourdetDerivative(tVec, pVec, 0.1);
    } else {
        dVec.resize(tVec.size());
        dVec.fill(0.0);
    }

    m_FittingPage->setObservedDataToCurrent(tVec, pVec, dVec);
}

void MainWindow::onFittingProgressChanged(int progress)
{
    if (this->statusBar()) {
        this->statusBar()->showMessage(QString("正在拟合... %1%").arg(progress));
        if(progress >= 100) this->statusBar()->showMessage("拟合完成", 5000);
    }
}

void MainWindow::onSystemSettingsChanged() {}
void MainWindow::onPerformanceSettingsChanged() {}

QStandardItemModel* MainWindow::getDataEditorModel() const
{
    if (!m_DataEditorWidget) return nullptr;
    return m_DataEditorWidget->getDataModel();
}

QString MainWindow::getCurrentFileName() const
{
    if (!m_DataEditorWidget) return QString();
    return m_DataEditorWidget->getCurrentFileName();
}

bool MainWindow::hasDataLoaded()
{
    if (!m_DataEditorWidget) return false;
    return m_DataEditorWidget->hasData();
}

void MainWindow::transferDataFromEditorToPlotting()
{
    if (!m_DataEditorWidget || !m_PlottingWidget) return;
    QMap<QString, QStandardItemModel*> models = m_DataEditorWidget->getAllDataModels();
    m_PlottingWidget->setDataModels(models);
    if (!models.isEmpty()) m_hasValidData = true;
}

void MainWindow::updateNavigationState()
{
    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
    while (item != m_NavBtnMap.end()) {
        ((NavBtn*)(item.value()))->setNormalStyle();
        if(item.key() == tr("项目")) ((NavBtn*)(item.value()))->setClickedStyle();
        item++;
    }
}

QString MainWindow::getMessageBoxStyle() const
{
    return getGlobalMessageBoxStyle();
}
