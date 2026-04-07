/*
 * 文件名: wt_projectwidget.cpp
 * 文件作用: 项目管理界面实现文件
 * 修复内容: 彻底解决 QComboBox 和 QDateEdit 下拉菜单白底白字、模糊不清的渲染问题。
 */

#include "wt_projectwidget.h"
#include "ui_wt_projectwidget.h"
#include "modelparameter.h"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QFileInfo>
#include <QDateTime>
#include <QSettings>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QAbstractButton>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleValidator>

// 【修复新增】：引入专属的视图控件头文件
#include <QListView>
#include <QCalendarWidget>

WT_ProjectWidget::WT_ProjectWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WT_ProjectWidget),
    m_isProjectOpen(false),
    m_currentMode(Mode_None)
{
    ui->setupUi(this);
    init();
}

WT_ProjectWidget::~WT_ProjectWidget()
{
    delete ui;
}

void WT_ProjectWidget::init()
{
    this->setStyleSheet("background-color: transparent;");

    QString btnStyle =
        "QToolButton { background-color: rgb(240, 248, 255); border-radius: 8px; border: 1px solid #D0E0F0; "
        "color: #333333; font-size: 13px; font-weight: bold; padding: 5px; min-width: 65px; min-height: 70px;}"
        "QToolButton:hover { background-color: #E6F7FF; border: 1px solid #85C1E9; }"
        "QToolButton:pressed { background-color: #D6EAF8; }";

    ui->btnNew->setIcon(QIcon(":/new/prefix1/Resource/PRO1.png"));
    ui->btnNew->setStyleSheet(btnStyle);
    connect(ui->btnNew, &QToolButton::clicked, this, &WT_ProjectWidget::onNewProjectClicked);

    ui->btnOpen->setIcon(QIcon(":/new/prefix1/Resource/PRO2.png"));
    ui->btnOpen->setStyleSheet(btnStyle);
    connect(ui->btnOpen, &QToolButton::clicked, this, &WT_ProjectWidget::onOpenProjectClicked);

    ui->btnClose->setIcon(QIcon(":/new/prefix1/Resource/PRO3.png"));
    ui->btnClose->setStyleSheet(btnStyle);
    connect(ui->btnClose, &QToolButton::clicked, this, &WT_ProjectWidget::onCloseProjectClicked);

    ui->btnExit->setIcon(QIcon(":/new/prefix1/Resource/PRO4.png"));
    ui->btnExit->setStyleSheet(btnStyle);
    connect(ui->btnExit, &QToolButton::clicked, this, &WT_ProjectWidget::onExitClicked);

    ui->label_recent->setStyleSheet("color: #333333; font-size: 16px; font-weight: bold; margin-top: 15px; margin-bottom: 8px;");

    ui->listWidget_recent->setStyleSheet(
        "QListWidget { background-color: #ffffff; border: 1px solid #D0D0D0; border-radius: 5px; outline: none; color: #000000; }"
        "QListWidget::item { padding: 10px; border-bottom: 1px solid #F0F0F0; color: #000000; }"
        "QListWidget::item:hover { background-color: #E6F7FF; color: #000000; }"
        "QListWidget::item:selected { background-color: #BAE7FF; color: #000000; font-weight: bold; }"
        );

    connect(ui->listWidget_recent, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onRecentProjectClicked(QListWidgetItem*)));
    connect(ui->listWidget_recent, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onRecentProjectDoubleClicked(QListWidgetItem*)));

    initRightPanel();
    loadRecentProjects();
}

void WT_ProjectWidget::initRightPanel()
{
    m_rightStackedWidget = new QStackedWidget(ui->widgetRight);
    QVBoxLayout* mainRightLayout = new QVBoxLayout(ui->widgetRight);
    mainRightLayout->setContentsMargins(0, 0, 0, 0);
    mainRightLayout->addWidget(m_rightStackedWidget);

    m_workflowPage = new QWidget();
    m_formPage = new QWidget();

    m_rightStackedWidget->addWidget(m_workflowPage);
    m_rightStackedWidget->addWidget(m_formPage);

    // ==========================================
    // 页面 0: 静态标准试井分析工作流
    // ==========================================
    QVBoxLayout* workflowLayout = new QVBoxLayout(m_workflowPage);
    workflowLayout->setContentsMargins(50, 40, 50, 40);
    workflowLayout->setSpacing(15);

    QLabel* titleLabel = new QLabel("标准试井分析工作流");
    titleLabel->setStyleSheet("font-size: 22px; font-weight: bold; color: #1C3C6A;");
    titleLabel->setAlignment(Qt::AlignCenter);
    workflowLayout->addWidget(titleLabel);

    struct FlowStep { QString title; QString desc; };
    QList<FlowStep> steps = {
        {"1. 项目初始化与数据加载", "新建工程，导入测压资料或常规测试的压力与产量数据。"},
        {"2. 数据预处理与特征识别", "压力数据平滑、降噪，提取对数压力导数，识别初始流动段特征。"},
        {"3. 储层解析模型构建", "根据测试特征，选择双孔、复合或多段压裂水平井等基础数学模型。"},
        {"4. 参数反演与历史拟合", "基于经典试井解析理论与数值求解器，完成全过程历史曲线拟合。"},
        {"5. 解释成果导出与报告评估", "输出储层渗透率、表皮系数、裂缝半长等核心参数及试井解释报告。"}
    };

    for (int i = 0; i < steps.size(); ++i) {
        QFrame* stepFrame = new QFrame();
        stepFrame->setStyleSheet("QFrame { background-color: #FFFFFF; border: 1px solid #E0E0E0; border-radius: 8px; }"
                                 "QFrame:hover { border: 1px solid #5C9DFF; background-color: #F8FBFF; }");
        QVBoxLayout* frameLayout = new QVBoxLayout(stepFrame);
        QLabel* stepTitle = new QLabel(steps[i].title);
        stepTitle->setStyleSheet("font-size: 15px; font-weight: bold; color: #333333; border: none; background: transparent;");
        QLabel* stepDesc = new QLabel(steps[i].desc);
        stepDesc->setStyleSheet("font-size: 12px; color: #555555; border: none; background: transparent;");
        frameLayout->addWidget(stepTitle); frameLayout->addWidget(stepDesc);
        workflowLayout->addWidget(stepFrame);
    }
    workflowLayout->addStretch();

    // ==========================================
    // 页面 1: 动态项目属性表单 (单页双列布局)
    // ==========================================
    QVBoxLayout* formLayout = new QVBoxLayout(m_formPage);
    formLayout->setContentsMargins(40, 20, 40, 20);
    formLayout->setSpacing(15);

    m_formTitleLabel = new QLabel("新建项目");
    m_formTitleLabel->setStyleSheet("font-size: 22px; font-weight: bold; color: #1C3C6A; margin-bottom: 5px;");
    m_formTitleLabel->setAlignment(Qt::AlignCenter);
    formLayout->addWidget(m_formTitleLabel);

    QFrame* contentFrame = new QFrame();
    contentFrame->setStyleSheet(
        "QFrame#contentFrame { border: 1px solid #D0D0D0; border-radius: 6px; background-color: #FFFFFF; }"
        "QLineEdit { border: 1px solid #B0B0B0; border-radius: 4px; padding: 4px; font-size: 13px; color: #333333; background: #FFFFFF; }"
        "QLineEdit:focus { border: 1px solid #5C9DFF; }"
        );
    contentFrame->setObjectName("contentFrame");

    QHBoxLayout* hLayout = new QHBoxLayout(contentFrame);
    hLayout->setContentsMargins(20, 20, 20, 20);
    hLayout->setSpacing(30);

    auto createLabel = [](const QString& text) {
        QLabel* l = new QLabel(text); l->setStyleSheet("font-weight: bold; color: #555555; font-size: 13px; border: none;");
        return l;
    };
    auto createSectionTitle = [](const QString& text) {
        QLabel* l = new QLabel(text); l->setStyleSheet("font-weight: bold; color: #1C3C6A; font-size: 15px; border: none; padding-top: 10px; padding-bottom: 5px;");
        return l;
    };
    QDoubleValidator* doubleVal = new QDoubleValidator(this);

    // ----------------------------------------------------
    // 左列：基本信息
    // ----------------------------------------------------
    QWidget* leftWidget = new QWidget();
    QGridLayout* leftGrid = new QGridLayout(leftWidget);
    leftGrid->setContentsMargins(0, 0, 0, 0); leftGrid->setVerticalSpacing(18);

    m_editProjName = new QLineEdit();
    m_editPath = new QLineEdit(); m_editPath->setReadOnly(true); m_editPath->setStyleSheet("background:#F5F5F5; color:#666;");
    m_btnBrowse = new QPushButton("..."); m_btnBrowse->setFixedWidth(35); m_btnBrowse->setStyleSheet("border: 1px solid #B0B0B0; border-radius: 4px; background: #E0E0E0;");
    connect(m_btnBrowse, &QPushButton::clicked, this, &WT_ProjectWidget::onBrowsePathClicked);
    QHBoxLayout* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(m_editPath); pathLayout->addWidget(m_btnBrowse); pathLayout->setContentsMargins(0,0,0,0);

    m_editOilField = new QLineEdit();
    m_editWell = new QLineEdit();
    m_editEngineer = new QLineEdit();

    // 【深度修复 1】：QDateEdit 及日历面板高清显示
    m_dateEdit = new QDateEdit(QDate::currentDate());
    m_dateEdit->setDisplayFormat("yyyy-MM-dd");
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setStyleSheet("QDateEdit { border: 1px solid #B0B0B0; border-radius: 4px; padding: 4px; font-size: 13px; color: #333333; background: #FFFFFF; }");

    QCalendarWidget* calendar = new QCalendarWidget(this);
    calendar->setStyleSheet(
        "QCalendarWidget QWidget { background-color: #FFFFFF; color: #333333; font-size: 12px; }"
        "QCalendarWidget QAbstractItemView:enabled { color: #333333; selection-background-color: #5C9DFF; selection-color: #FFFFFF; }"
        "QCalendarWidget QAbstractItemView:disabled { color: #AAAAAA; }"
        "QCalendarWidget QToolButton { color: #333333; background-color: transparent; border: none; font-weight: bold; margin: 2px; }"
        "QCalendarWidget QToolButton:hover { background-color: #E0E0E0; border-radius: 3px; }"
        "QCalendarWidget QMenu { background-color: #FFFFFF; color: #333333; border: 1px solid #CCCCCC; }"
        "QCalendarWidget QSpinBox { background-color: #FFFFFF; color: #333333; border: 1px solid #CCCCCC; }"
        );
    m_dateEdit->setCalendarWidget(calendar);

    // 【深度修复 2】：QComboBox 及下拉列表高清显示
    m_comboUnit = new QComboBox();
    m_comboUnit->addItems({"公制 (Metric / SI)", "英制 (Field Unit)"});
    m_comboUnit->setStyleSheet(
        "QComboBox { border: 1px solid #B0B0B0; border-radius: 4px; padding: 4px; font-size: 13px; color: #333333; background: #FFFFFF; }"
        "QComboBox:focus { border: 1px solid #5C9DFF; }"
        );

    QListView* comboListView = new QListView(m_comboUnit);
    comboListView->setStyleSheet(
        "QListView { background-color: #FFFFFF; color: #333333; font-size: 13px; border: 1px solid #B0B0B0; outline: none; }"
        "QListView::item { padding: 6px; }"
        "QListView::item:hover { background-color: #E6F7FF; color: #1C3C6A; font-weight: bold; }"
        "QListView::item:selected { background-color: #5C9DFF; color: #FFFFFF; font-weight: bold; }"
        );
    m_comboUnit->setView(comboListView);

    m_editComments = new QLineEdit();

    leftGrid->addWidget(createSectionTitle("基础信息配置"), 0, 0, 1, 2);
    leftGrid->addWidget(createLabel("项目名称:"), 1, 0); leftGrid->addWidget(m_editProjName, 1, 1);
    leftGrid->addWidget(createLabel("存放路径:"), 2, 0); leftGrid->addLayout(pathLayout, 2, 1);
    leftGrid->addWidget(createLabel("油田名称:"), 3, 0); leftGrid->addWidget(m_editOilField, 3, 1);
    leftGrid->addWidget(createLabel("井号:"), 4, 0);     leftGrid->addWidget(m_editWell, 4, 1);
    leftGrid->addWidget(createLabel("测试日期:"), 5, 0); leftGrid->addWidget(m_dateEdit, 5, 1);
    leftGrid->addWidget(createLabel("工程师:"), 6, 0);   leftGrid->addWidget(m_editEngineer, 6, 1);
    leftGrid->addWidget(createLabel("单位系统:"), 7, 0); leftGrid->addWidget(m_comboUnit, 7, 1);
    leftGrid->addWidget(createLabel("备注说明:"), 8, 0); leftGrid->addWidget(m_editComments, 8, 1);
    leftGrid->setRowStretch(9, 1);

    // ----------------------------------------------------
    // 中间：垂直分割线
    // ----------------------------------------------------
    QFrame* vLine = new QFrame();
    vLine->setFrameShape(QFrame::VLine);
    vLine->setFrameShadow(QFrame::Sunken);
    vLine->setStyleSheet("border: 1px solid #E0E0E0;");

    // ----------------------------------------------------
    // 右列：核心物理参数
    // ----------------------------------------------------
    QWidget* rightWidget = new QWidget();
    QGridLayout* rightGrid = new QGridLayout(rightWidget);
    rightGrid->setContentsMargins(0, 0, 0, 0); rightGrid->setVerticalSpacing(18);

    m_editRate = new QLineEdit(); m_editRate->setValidator(doubleVal);
    m_editThickness = new QLineEdit(); m_editThickness->setValidator(doubleVal);
    m_editPorosity = new QLineEdit(); m_editPorosity->setValidator(doubleVal);
    m_editRw = new QLineEdit(); m_editRw->setValidator(doubleVal);
    m_editL = new QLineEdit(); m_editL->setValidator(doubleVal);
    m_editNf = new QLineEdit(); m_editNf->setValidator(doubleVal);

    m_editCt = new QLineEdit(); m_editCt->setValidator(doubleVal);
    m_editMu = new QLineEdit(); m_editMu->setValidator(doubleVal);
    m_editB = new QLineEdit(); m_editB->setValidator(doubleVal);

    int rRow = 0;
    rightGrid->addWidget(createSectionTitle("油藏与井筒参数"), rRow++, 0, 1, 2);
    rightGrid->addWidget(createLabel("产/注量 (q) [m³/d]:"), rRow, 0); rightGrid->addWidget(m_editRate, rRow++, 1);
    rightGrid->addWidget(createLabel("有效厚度 (h) [m]:"), rRow, 0); rightGrid->addWidget(m_editThickness, rRow++, 1);
    rightGrid->addWidget(createLabel("孔隙度 (φ):"), rRow, 0); rightGrid->addWidget(m_editPorosity, rRow++, 1);
    rightGrid->addWidget(createLabel("井筒半径 (rw) [m]:"), rRow, 0); rightGrid->addWidget(m_editRw, rRow++, 1);
    rightGrid->addWidget(createLabel("水平井长 (L) [m]:"), rRow, 0); rightGrid->addWidget(m_editL, rRow++, 1);
    rightGrid->addWidget(createLabel("裂缝条数 (nf):"), rRow, 0); rightGrid->addWidget(m_editNf, rRow++, 1);

    rightGrid->addWidget(createSectionTitle("流体 PVT 参数"), rRow++, 0, 1, 2);
    rightGrid->addWidget(createLabel("综合压缩系数 (Ct) [MPa⁻¹]:"), rRow, 0); rightGrid->addWidget(m_editCt, rRow++, 1);
    rightGrid->addWidget(createLabel("流体粘度 (μ) [mPa·s]:"), rRow, 0); rightGrid->addWidget(m_editMu, rRow++, 1);
    rightGrid->addWidget(createLabel("体积系数 (B) [m³/m³]:"), rRow, 0); rightGrid->addWidget(m_editB, rRow++, 1);
    rightGrid->setRowStretch(rRow, 1);

    hLayout->addWidget(leftWidget, 1);
    hLayout->addWidget(vLine);
    hLayout->addWidget(rightWidget, 1);

    formLayout->addWidget(contentFrame);

    m_btnAction = new QPushButton("✔ 执行操作");
    m_btnAction->setStyleSheet(
        "QPushButton { background-color: #1C3C6A; color: #FFF; font-size: 16px; font-weight: bold; border-radius: 6px; padding: 12px; margin-top: 10px;}"
        "QPushButton:hover { background-color: #2A5A9A; }"
        "QPushButton:pressed { background-color: #142B4C; }"
        );
    connect(m_btnAction, &QPushButton::clicked, this, &WT_ProjectWidget::onActionButtonClicked);
    formLayout->addWidget(m_btnAction);

    m_rightStackedWidget->setCurrentWidget(m_workflowPage);
}

void WT_ProjectWidget::setFormMode(FormMode mode, const QString& filePath)
{
    m_currentMode = mode;
    m_previewFilePath = filePath;
    m_rightStackedWidget->setCurrentWidget(m_formPage);

    if (mode == Mode_New) {
        m_formTitleLabel->setText("新建项目");
        m_btnAction->setText("✔ 创建并进入项目");
        m_btnBrowse->setEnabled(true);
        m_editProjName->setReadOnly(false);
        clearFormFields();
    }
    else if (mode == Mode_Preview) {
        m_formTitleLabel->setText("项目预览 (未加载)");
        m_btnAction->setText("📂 加载此项目至引擎");
        m_btnBrowse->setEnabled(false);
        m_editProjName->setReadOnly(true);
        loadFormFromJson(filePath);
    }
    else if (mode == Mode_Opened) {
        m_formTitleLabel->setText("项目属性设置 (运行中)");
        m_btnAction->setText("💾 保存属性更改");
        m_btnBrowse->setEnabled(false);
        m_editProjName->setReadOnly(true);
    }
}

void WT_ProjectWidget::clearFormFields()
{
    m_editProjName->clear();
    m_editPath->setText(QCoreApplication::applicationDirPath() + "/Projects");
    m_editOilField->clear(); m_editWell->clear(); m_editEngineer->clear(); m_editComments->clear();
    m_dateEdit->setDate(QDate::currentDate());
    m_comboUnit->setCurrentIndex(0);

    m_editRate->setText("10.0"); m_editThickness->setText("10.0");
    m_editPorosity->setText("0.2"); m_editRw->setText("0.1");
    m_editL->setText("1000.0"); m_editNf->setText("10");
    m_editCt->setText("0.001"); m_editMu->setText("1.0"); m_editB->setText("1.0");
}

void WT_ProjectWidget::loadFormFromJson(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return;
    QJsonObject root = doc.object();

    auto rs = [&root](const QString& k1, const QString& k2) { return root.contains(k1) ? root.value(k1).toString() : root.value(k2).toString(); };
    auto rd = [&root](const QString& k1, const QString& k2) { return root.contains(k1) ? root.value(k1).toDouble() : root.value(k2).toDouble(); };

    m_editProjName->setText(QFileInfo(filePath).baseName());
    m_editPath->setText(QFileInfo(filePath).absolutePath());
    m_editOilField->setText(rs("oilFieldName", "OilFieldName"));
    m_editWell->setText(rs("wellName", "WellName"));
    m_editEngineer->setText(rs("engineer", "Engineer"));
    m_editComments->setText(rs("comments", "Comments"));

    QString dateStr = rs("testDate", "TestDate");
    if(!dateStr.isEmpty()) {
        m_dateEdit->setDate(QDate::fromString(dateStr.left(10), "yyyy-MM-dd"));
    }

    int unitSys = root.contains("currentUnitSystem") ? root.value("currentUnitSystem").toInt() : root.value("CurrentUnitSystem").toInt();
    m_comboUnit->setCurrentIndex(unitSys);

    m_editRate->setText(QString::number(rd("productionRate", "ProductionRate")));
    m_editThickness->setText(QString::number(rd("thickness", "Thickness")));
    m_editPorosity->setText(QString::number(rd("porosity", "Porosity")));
    m_editRw->setText(QString::number(rd("wellRadius", "WellRadius")));
    m_editL->setText(QString::number(rd("horizLength", "HorizLength")));
    m_editNf->setText(QString::number(rd("fracCount", "FracCount")));
    m_editCt->setText(QString::number(rd("compressibility", "Compressibility")));
    m_editMu->setText(QString::number(rd("viscosity", "Viscosity")));
    m_editB->setText(QString::number(rd("volumeFactor", "VolumeFactor")));
}

bool WT_ProjectWidget::saveFormToJson(const QString& filePath)
{
    QJsonObject root;
    root["projectName"] = m_editProjName->text();
    root["oilFieldName"] = m_editOilField->text();
    root["wellName"] = m_editWell->text();
    root["engineer"] = m_editEngineer->text();

    root["testDate"] = m_dateEdit->date().toString("yyyy-MM-dd");
    root["comments"] = m_editComments->text();
    root["currentUnitSystem"] = m_comboUnit->currentIndex();

    root["productionRate"] = m_editRate->text().toDouble();
    root["thickness"] = m_editThickness->text().toDouble();
    root["porosity"] = m_editPorosity->text().toDouble();
    root["wellRadius"] = m_editRw->text().toDouble();
    root["horizLength"] = m_editL->text().toDouble();
    root["fracCount"] = m_editNf->text().toDouble();
    root["compressibility"] = m_editCt->text().toDouble();
    root["viscosity"] = m_editMu->text().toDouble();
    root["volumeFactor"] = m_editB->text().toDouble();
    root["testType"] = 0;

    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", "无法写入工程文件: " + filePath);
        return false;
    }
    file.write(doc.toJson());
    file.close();
    return true;
}

void WT_ProjectWidget::onBrowsePathClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择工程保存目录", m_editPath->text());
    if(!dir.isEmpty()) m_editPath->setText(dir);
}

void WT_ProjectWidget::onActionButtonClicked()
{
    if (m_currentMode == Mode_New) {
        if(m_editProjName->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "必填项", "请输入项目名称。"); return;
        }
        QString fullPath = m_editPath->text() + "/" + m_editProjName->text().trimmed() + ".pwt";

        if (saveFormToJson(fullPath)) {
            ModelParameter::instance()->setParameters(
                m_editPorosity->text().toDouble(), m_editThickness->text().toDouble(),
                m_editMu->text().toDouble(), m_editB->text().toDouble(), m_editCt->text().toDouble(),
                m_editRate->text().toDouble(), m_editRw->text().toDouble(), m_editL->text().toDouble(),
                m_editNf->text().toDouble(), fullPath);

            setProjectState(true, fullPath);
            updateRecentProjectsList(fullPath);
            setFormMode(Mode_Opened, fullPath);
            emit projectOpened(true);
        }
    }
    else if (m_currentMode == Mode_Preview) {
        if (ModelParameter::instance()->loadProject(m_previewFilePath)) {
            setProjectState(true, m_previewFilePath);
            updateRecentProjectsList(m_previewFilePath);
            setFormMode(Mode_Opened, m_previewFilePath);
            emit projectOpened(false);
        } else {
            QMessageBox::critical(this, "错误", "项目文件加载失败。");
        }
    }
    else if (m_currentMode == Mode_Opened) {
        if (saveFormToJson(m_currentProjectFilePath)) {
            ModelParameter::instance()->setParameters(
                m_editPorosity->text().toDouble(), m_editThickness->text().toDouble(),
                m_editMu->text().toDouble(), m_editB->text().toDouble(), m_editCt->text().toDouble(),
                m_editRate->text().toDouble(), m_editRw->text().toDouble(), m_editL->text().toDouble(),
                m_editNf->text().toDouble(), m_currentProjectFilePath);

            QMessageBox::information(this, "保存成功", "项目属性已成功保存至文件。");
        }
    }
}

void WT_ProjectWidget::onNewProjectClicked()
{
    if (m_isProjectOpen) {
        QMessageBox::warning(this, "操作受限", "项目正在运行中。\n请先关闭当前项目再新建。");
        return;
    }
    setFormMode(Mode_New);
}

void WT_ProjectWidget::onRecentProjectClicked(QListWidgetItem *item)
{
    if (!item || m_isProjectOpen) return;
    QString filePath = item->data(Qt::UserRole).toString();
    if(QFileInfo::exists(filePath)) setFormMode(Mode_Preview, filePath);
}

void WT_ProjectWidget::onRecentProjectDoubleClicked(QListWidgetItem *item)
{
    if (m_isProjectOpen) {
        QMessageBox::warning(this, "操作受限", "当前已有项目在运行，请先关闭当前项目。");
        return;
    }
    QString filePath = item->data(Qt::UserRole).toString();
    if (ModelParameter::instance()->loadProject(filePath)) {
        setProjectState(true, filePath);
        updateRecentProjectsList(filePath);
        setFormMode(Mode_Opened, filePath);
        emit projectOpened(false);
    } else {
        QMessageBox::critical(this, "错误", "无法打开项目。");
        m_recentProjects.removeAll(filePath);
        loadRecentProjects();
        m_rightStackedWidget->setCurrentWidget(m_workflowPage);
    }
}

void WT_ProjectWidget::onOpenProjectClicked()
{
    if (m_isProjectOpen) {
        QMessageBox::warning(this, "操作受限", "不能同时打开多个项目。\n请先点击“关闭”按钮。");
        return;
    }
    QString filePath = QFileDialog::getOpenFileName(this, "打开项目", "", "WellTest Project (*.pwt)");
    if (filePath.isEmpty()) return;

    if (ModelParameter::instance()->loadProject(filePath)) {
        setProjectState(true, filePath);
        updateRecentProjectsList(filePath);
        setFormMode(Mode_Opened, filePath);
        emit projectOpened(false);
    } else {
        QMessageBox::critical(this, "错误", "格式不正确，无法打开。");
    }
}

void WT_ProjectWidget::onCloseProjectClicked()
{
    if (!m_isProjectOpen) {
        QMessageBox::information(this, "提示", "当前没有正在运行的项目。");
        return;
    }
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("关闭项目");
    msgBox.setText(QString("是否关闭当前项目 [%1]？").arg(QFileInfo(m_currentProjectFilePath).fileName()));
    msgBox.setInformativeText("关闭前会自动保存数据。");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStyleSheet(getMessageBoxStyle());

    QPushButton *saveCloseBtn = msgBox.addButton("保存并关闭", QMessageBox::AcceptRole);
    QPushButton *directCloseBtn = msgBox.addButton("直接关闭", QMessageBox::DestructiveRole);
    msgBox.addButton("取消", QMessageBox::RejectRole);
    msgBox.setDefaultButton(saveCloseBtn);
    msgBox.exec();

    if (msgBox.clickedButton() == (QAbstractButton*)saveCloseBtn) {
        onActionButtonClicked();
        if(saveCurrentProject()) closeProjectInternal();
    } else if (msgBox.clickedButton() == (QAbstractButton*)directCloseBtn) {
        closeProjectInternal();
    }
}

void WT_ProjectWidget::closeProjectInternal()
{
    setProjectState(false, "");
    ModelParameter::instance()->closeProject();
    m_currentMode = Mode_None;
    m_rightStackedWidget->setCurrentWidget(m_workflowPage);
    emit projectClosed();
}

void WT_ProjectWidget::loadRecentProjects() {
    QSettings settings("PWT_Team", "WellTestApp");
    m_recentProjects = settings.value("RecentProjects").toStringList();
    ui->listWidget_recent->clear();
    for (const QString& path : m_recentProjects) {
        if (QFileInfo::exists(path)) {
            QListWidgetItem* item = new QListWidgetItem(QString("%1\n%2").arg(QFileInfo(path).fileName(), path), ui->listWidget_recent);
            item->setData(Qt::UserRole, path);
        }
    }
}

void WT_ProjectWidget::updateRecentProjectsList(const QString& newProjectPath) {
    m_recentProjects.removeAll(newProjectPath);
    m_recentProjects.prepend(newProjectPath);
    while (m_recentProjects.size() > MAX_RECENT_PROJECTS) m_recentProjects.removeLast();
    QSettings("PWT_Team", "WellTestApp").setValue("RecentProjects", m_recentProjects);
    loadRecentProjects();
}

void WT_ProjectWidget::setProjectState(bool isOpen, const QString& filePath) {
    m_isProjectOpen = isOpen;
    m_currentProjectFilePath = filePath;
}

bool WT_ProjectWidget::saveCurrentProject() {
    ModelParameter::instance()->saveProject();
    return true;
}

void WT_ProjectWidget::onExitClicked() {
    if (!m_isProjectOpen) QApplication::quit();
    else this->window()->close();
}

QString WT_ProjectWidget::getMessageBoxStyle() const {
    return "QMessageBox { background-color: #ffffff; color: #000000; }"
           "QLabel { color: #000000; background-color: transparent; }"
           "QPushButton { color: #000000; background-color: #f0f0f0; border: 1px solid #c0c0c0; border-radius: 3px; padding: 5px 15px; min-width: 60px;}"
           "QPushButton:hover { background-color: #e0e0e0; }"
           "QPushButton:pressed { background-color: #d0d0d0; }";
}
