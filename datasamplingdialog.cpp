/*
 * 文件名: datasamplingdialog.cpp
 * 文件作用: 数据滤波与取样悬浮窗口实现文件
 * 功能描述:
 * 1. 提供用户交互界面，设置数据的多阶段（早期、中期、晚期）滤波与抽样参数。
 * 2. 【悬浮窗特性】开启鼠标事件追踪，识别左侧 6 像素宽度的边缘，支持用户按住左键手动向左拉伸窗口宽度。
 * 3. 使用 QPainter 纯代码绘制顶部的“恢复”、“最大化”、“关闭”控制台图标。
 * 4. 底部和中间的控制按钮去除了复杂的背景色，回归原生扁平风格。
 * 5. 使用 QCustomPlot 实时绘制数据点，并支持用户在图表上直接拖拽垂直虚线来划分阶段边界。
 * 6. 包含了中值滤波、对数抽样、等间隔抽样等核心数学算法。
 */

#include "datasamplingdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

// =========================================================================
// =                           构造与析构模块                                 =
// =========================================================================

DataSamplingWidget::DataSamplingWidget(QWidget *parent)
    : QWidget(parent),
    m_model(nullptr),
    m_dragLineIndex(0),
    m_isResizing(false),
    m_dragStartX(0)
{
    // 【关键】必须开启鼠标追踪，才能在不按下的情况下识别边缘并改变鼠标指针形态为拉伸箭头
    setMouseTracking(true);

    initUI();
    setupConnections();
    applyStyle();
}

DataSamplingWidget::~DataSamplingWidget() {}

// =========================================================================
// =                           UI 初始化与绘制模块                            =
// =========================================================================

/**
 * @brief 纯代码绘制标题栏的三个控制图标
 * @param type 0:恢复默认(双矩形) 1:最大化(单矩形) 2:关闭(交叉X)
 * @return 绘制完成的 QIcon
 */
QIcon DataSamplingWidget::createCtrlIcon(int type) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing); // 开启抗锯齿
    p.setPen(QPen(QColor(80, 80, 80), 1.5)); // 图标线条颜色与粗细

    if (type == 0) {
        // 绘制恢复默认大小图标 (两个重叠的矩形)
        p.drawRect(5, 9, 10, 10);
        p.drawLine(9, 9, 9, 5);
        p.drawLine(9, 5, 19, 5);
        p.drawLine(19, 5, 19, 15);
        p.drawLine(19, 15, 15, 15);
    } else if (type == 1) {
        // 绘制最大化图标 (单矩形)
        p.drawRect(6, 6, 12, 12);
    } else if (type == 2) {
        // 绘制关闭图标 (交叉X)
        p.drawLine(7, 7, 17, 17);
        p.drawLine(17, 7, 7, 17);
    }
    return QIcon(pixmap);
}

/**
 * @brief 界面所有控件的生成与布局规划
 */
void DataSamplingWidget::initUI()
{
    // 打破内部组件对尺寸的强制霸占，允许该窗口被主界面的自适应算法强行压窄
    this->setMinimumSize(350, 400);

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ================== 顶部：标题栏与控制按钮 ==================
    QWidget* titleBar = new QWidget(this);
    titleBar->setStyleSheet("background-color: #F0F2F5; border-bottom: 1px solid #D9D9D9;");
    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(15, 6, 10, 6);

    QLabel* lblTitle = new QLabel("数据滤波与多阶段抽样");
    lblTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #333333; border: none;");

    // 初始化右上角图标按钮
    btnRestoreWindow = new QPushButton();
    btnRestoreWindow->setIcon(createCtrlIcon(0));
    btnRestoreWindow->setToolTip("对齐到表格空白区");

    btnMaximizeWindow = new QPushButton();
    btnMaximizeWindow->setIcon(createCtrlIcon(1));
    btnMaximizeWindow->setToolTip("覆盖整个数据区");

    btnCloseWindow = new QPushButton();
    btnCloseWindow->setIcon(createCtrlIcon(2));
    btnCloseWindow->setToolTip("关闭功能面板");

    // 配置顶部按钮的透明无边框样式
    QString ctrlStyle = "QPushButton { border: none; background: transparent; min-width: 28px; height: 28px; border-radius: 4px; }"
                        "QPushButton:hover { background-color: #E0E0E0; }"
                        "QPushButton:pressed { background-color: #D0D0D0; }";
    btnRestoreWindow->setStyleSheet(ctrlStyle);
    btnMaximizeWindow->setStyleSheet(ctrlStyle);
    // 给关闭按钮单独绑定红色 Hover 效果
    btnCloseWindow->setStyleSheet(ctrlStyle + "QPushButton#btnCloseWindow:hover { background-color: #FF4D4F; }");
    btnCloseWindow->setObjectName("btnCloseWindow");

    titleLayout->addWidget(lblTitle);
    titleLayout->addStretch();
    titleLayout->addWidget(btnRestoreWindow);
    titleLayout->addWidget(btnMaximizeWindow);
    titleLayout->addWidget(btnCloseWindow);

    rootLayout->addWidget(titleBar);

    // ================== 主体：内部参数及图表区 ==================
    QWidget* contentWidget = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(contentWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // ---------------- 左侧：参数设置区 ----------------
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(10);

    // 1. 数据源与坐标系组
    QGroupBox* groupData = new QGroupBox("1. 数据源与坐标系");
    QVBoxLayout* vData = new QVBoxLayout(groupData);
    QHBoxLayout* hX = new QHBoxLayout();
    hX->addWidget(new QLabel("X 轴数据:"));
    comboX = new QComboBox();
    chkLogX = new QCheckBox("X轴对数");
    hX->addWidget(comboX, 1);
    hX->addWidget(chkLogX);

    QHBoxLayout* hY = new QHBoxLayout();
    hY->addWidget(new QLabel("Y 轴数据:"));
    comboY = new QComboBox();
    chkLogY = new QCheckBox("Y轴对数");
    hY->addWidget(comboY, 1);
    hY->addWidget(chkLogY);

    vData->addLayout(hX);
    vData->addLayout(hY);
    leftLayout->addWidget(groupData);

    // 2. 全局异常点组 (基于 Y 轴上下限)
    QGroupBox* groupGlobal = new QGroupBox("2. 全局异常点剔除 (Y轴)");
    QVBoxLayout* vGlobal = new QVBoxLayout(groupGlobal);
    QHBoxLayout* hMin = new QHBoxLayout();
    hMin->addWidget(new QLabel("下限:"));
    spinMinY = new QDoubleSpinBox();
    spinMinY->setRange(-1e9, 1e9);
    spinMinY->setValue(0.0);
    hMin->addWidget(spinMinY);

    QHBoxLayout* hMax = new QHBoxLayout();
    hMax->addWidget(new QLabel("上限:"));
    spinMaxY = new QDoubleSpinBox();
    spinMaxY->setRange(-1e9, 1e9);
    spinMaxY->setValue(100.0);
    hMax->addWidget(spinMaxY);

    vGlobal->addLayout(hMin);
    vGlobal->addLayout(hMax);
    leftLayout->addWidget(groupGlobal);

    // 3. 区间划分边界设置
    QGroupBox* groupSplit = new QGroupBox("3. 区间划分 (支持图上拖拽)");
    QVBoxLayout* vSplit = new QVBoxLayout(groupSplit);
    QHBoxLayout* hS1 = new QHBoxLayout();
    hS1->addWidget(new QLabel("早-中边界:"));
    spinSplit1 = new QDoubleSpinBox();
    spinSplit1->setRange(-1e9, 1e9);
    spinSplit1->setDecimals(4);
    hS1->addWidget(spinSplit1);

    QHBoxLayout* hS2 = new QHBoxLayout();
    hS2->addWidget(new QLabel("中-晚边界:"));
    spinSplit2 = new QDoubleSpinBox();
    spinSplit2->setRange(-1e9, 1e9);
    spinSplit2->setDecimals(4);
    hS2->addWidget(spinSplit2);

    vSplit->addLayout(hS1);
    vSplit->addLayout(hS2);
    leftLayout->addWidget(groupSplit);

    // 4. 分阶段处理策略（早中晚三阶段）
    QGroupBox* groupStage = new QGroupBox("4. 分阶段处理策略");
    QVBoxLayout* vStageMain = new QVBoxLayout(groupStage);
    QHBoxLayout* hStage = new QHBoxLayout();
    hStage->setSpacing(5);

    QStringList stageNames = {"早期", "中期", "晚期"};
    for(int i = 0; i < 3; ++i) {
        QWidget* box = new QWidget();
        QVBoxLayout* l = new QVBoxLayout(box);
        l->setContentsMargins(2, 2, 2, 2);
        l->setSpacing(5);

        QLabel* titleLabel = new QLabel(stageNames[i]);
        titleLabel->setStyleSheet("font-weight: bold; color: #555555;");
        l->addWidget(titleLabel, 0, Qt::AlignCenter);

        // 滤波算法下拉框
        m_stages[i].comboFilter = new QComboBox();
        m_stages[i].comboFilter->addItems({"无滤波", "中值滤波"});
        m_stages[i].comboFilter->setCurrentIndex(0);
        l->addWidget(m_stages[i].comboFilter);

        // 滤波窗口参数
        m_stages[i].spinFilterWin = new QSpinBox();
        m_stages[i].spinFilterWin->setRange(3, 101);
        m_stages[i].spinFilterWin->setSingleStep(2);
        m_stages[i].spinFilterWin->setValue(5);
        m_stages[i].spinFilterWin->setEnabled(false);
        l->addWidget(m_stages[i].spinFilterWin);

        // 抽样算法下拉框
        m_stages[i].comboSample = new QComboBox();
        m_stages[i].comboSample->addItems({"对数", "间隔", "总点数"});
        m_stages[i].comboSample->setCurrentIndex(0);
        l->addWidget(m_stages[i].comboSample);

        // 抽样参数
        m_stages[i].spinSampleVal = new QSpinBox();
        m_stages[i].spinSampleVal->setRange(1, 100000);
        m_stages[i].spinSampleVal->setValue(20);
        l->addWidget(m_stages[i].spinSampleVal);

        hStage->addWidget(box);

        // 联动逻辑：仅当选择了中值滤波时，才允许调整参考点数窗口大小
        connect(m_stages[i].comboFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, i](int index){ m_stages[i].spinFilterWin->setEnabled(index == 1); });
    }
    vStageMain->addLayout(hStage);
    leftLayout->addWidget(groupStage);

    leftLayout->addStretch();
    // 预览与重置控制按钮
    QHBoxLayout* opLayout = new QHBoxLayout();
    btnPreview = new QPushButton("应用预览");
    btnReset = new QPushButton("重置数据");
    opLayout->addWidget(btnPreview);
    opLayout->addWidget(btnReset);
    leftLayout->addLayout(opLayout);

    // ---------------- 右侧：双图表展示区 ----------------
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(10);

    customPlotRaw = new QCustomPlot();
    customPlotRaw->plotLayout()->insertRow(0);
    customPlotRaw->plotLayout()->addElement(0, 0, new QCPTextElement(customPlotRaw, "抽样前 (原始数据与阶段划分)", QFont("Microsoft YaHei", 12, QFont::Bold)));

    customPlotProcessed = new QCustomPlot();
    customPlotProcessed->plotLayout()->insertRow(0);
    customPlotProcessed->plotLayout()->addElement(0, 0, new QCPTextElement(customPlotProcessed, "抽样后 (处理后数据)", QFont("Microsoft YaHei", 12, QFont::Bold)));

    rightLayout->addWidget(customPlotRaw, 1);
    rightLayout->addWidget(customPlotProcessed, 1);

    // ---------------- 底部：确认返回按钮 ----------------
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    btnOk = new QPushButton("确定");
    btnOk->setDefault(true);
    btnCancel = new QPushButton("取消");
    bottomLayout->addWidget(btnOk);
    bottomLayout->addWidget(btnCancel);
    rightLayout->addLayout(bottomLayout);

    // 组合左右布局
    mainLayout->addLayout(leftLayout, 4);
    mainLayout->addLayout(rightLayout, 5);
    rootLayout->addWidget(contentWidget, 1);

    // ---------------- 图表阶段分割线初始化 ----------------
    vLine1 = new QCPItemStraightLine(customPlotRaw);
    vLine2 = new QCPItemStraightLine(customPlotRaw);
    vLine1->setPen(QPen(QColor(24, 144, 255), 2, Qt::DashLine));
    vLine2->setPen(QPen(QColor(82, 196, 26), 2, Qt::DashLine));

    vLine1_proc = new QCPItemStraightLine(customPlotProcessed);
    vLine2_proc = new QCPItemStraightLine(customPlotProcessed);
    vLine1_proc->setPen(QPen(QColor(24, 144, 255), 2, Qt::DotLine));
    vLine2_proc->setPen(QPen(QColor(82, 196, 26), 2, Qt::DotLine));
}

/**
 * @brief 绑定交互信号
 */
void DataSamplingWidget::setupConnections()
{
    connect(comboX, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataSamplingWidget::onDataColumnChanged);
    connect(comboY, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataSamplingWidget::onDataColumnChanged);

    connect(chkLogX, &QCheckBox::stateChanged, this, &DataSamplingWidget::onAxisScaleChanged);
    connect(chkLogY, &QCheckBox::stateChanged, this, &DataSamplingWidget::onAxisScaleChanged);

    connect(spinSplit1, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &DataSamplingWidget::onSplitBoxValueChanged);
    connect(spinSplit2, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &DataSamplingWidget::onSplitBoxValueChanged);

    connect(btnPreview, &QPushButton::clicked, this, &DataSamplingWidget::onPreviewClicked);
    connect(btnReset, &QPushButton::clicked, this, &DataSamplingWidget::onResetClicked);

    // 将处理好的数据打包抛给主窗口
    connect(btnOk, &QPushButton::clicked, this, [this]() {
        emit processingFinished(getHeaders(), getProcessedTable());
        emit requestClose();
    });
    connect(btnCancel, &QPushButton::clicked, this, &DataSamplingWidget::requestClose);

    // 窗口控制信号外抛
    connect(btnMaximizeWindow, &QPushButton::clicked, this, &DataSamplingWidget::requestMaximize);
    connect(btnRestoreWindow, &QPushButton::clicked, this, &DataSamplingWidget::requestRestore);
    connect(btnCloseWindow, &QPushButton::clicked, this, &DataSamplingWidget::requestClose);

    // 绑定鼠标在图表上的交互，实现边界线的拖拽
    connect(customPlotRaw, &QCustomPlot::mousePress, this, &DataSamplingWidget::onRawPlotMousePress);
    connect(customPlotRaw, &QCustomPlot::mouseMove, this, &DataSamplingWidget::onRawPlotMouseMove);
    connect(customPlotRaw, &QCustomPlot::mouseRelease, this, &DataSamplingWidget::onRawPlotMouseRelease);
}

/**
 * @brief 应用扁平化的控件样式（去除了原本强烈的红绿背景色）
 */
void DataSamplingWidget::applyStyle()
{
    QString qss = "QWidget { color: #333333; font-family: 'Microsoft YaHei'; font-size: 12px; }"
                  "QGroupBox { border: 1px solid #E5E5E5; border-radius: 4px; margin-top: 10px; padding-top: 15px; }"
                  "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 3px; font-weight: bold; color: #1890FF; }"
                  "QPushButton { background-color: #FAFAFA; color: #333333; border: 1px solid #D9D9D9; border-radius: 4px; padding: 6px 15px; min-width: 70px; }"
                  "QPushButton:hover { background-color: #E6F7FF; color: #1890FF; border-color: #1890FF; }"
                  "QPushButton:pressed { background-color: #BAE7FF; }"
                  "QComboBox, QSpinBox, QDoubleSpinBox { border: 1px solid #D9D9D9; border-radius: 3px; padding: 2px; min-height: 22px; }"
                  "QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: #1890FF; }"
                  "QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { background-color: #F5F5F5; color: #BFBFBF; }";
    this->setStyleSheet(qss);
}

// =========================================================================
// =                           悬浮窗边缘拖拽伸缩模块                          =
// =========================================================================

void DataSamplingWidget::mousePressEvent(QMouseEvent *event) {
    // 鼠标在左侧 6 像素边缘按下时，启动拉伸模式并记录起始坐标
    if (event->button() == Qt::LeftButton && event->pos().x() <= 6) {
        m_isResizing = true;
        m_dragStartX = event->globalPos().x();
    }
    QWidget::mousePressEvent(event);
}

void DataSamplingWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_isResizing) {
        // 计算鼠标水平移动了多少像素
        int dx = event->globalPos().x() - m_dragStartX;
        m_dragStartX = event->globalPos().x();
        // 将位移增量发送给主窗口，由主窗口调用 setGeometry 进行安全限制和绝对坐标刷新
        emit requestResize(dx);
    } else {
        // 只有开启了 setMouseTracking(true) 才能走到这里。悬停在左侧边缘时改变鼠标形状
        if (event->pos().x() <= 6) setCursor(Qt::SizeHorCursor);
        else unsetCursor();
    }
    QWidget::mouseMoveEvent(event);
}

void DataSamplingWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isResizing = false; // 结束拖拽
    }
    QWidget::mouseReleaseEvent(event);
}

// =========================================================================
// =                             核心数据读取模块                             =
// =========================================================================

/**
 * @brief 将主窗口传入的 QStandardItemModel 绑定到当前组件
 */
void DataSamplingWidget::setModel(QStandardItemModel* model)
{
    m_model = model;
    comboX->clear();
    comboY->clear();
    m_headers.clear();

    if (m_model && m_model->columnCount() > 0) {
        // 加载表头，供用户选择绘制列
        for(int i = 0; i < m_model->columnCount(); ++i) {
            QString name = m_model->headerData(i, Qt::Horizontal).toString();
            if(name.isEmpty()) name = QString("列 %1").arg(i+1);
            m_headers.append(name);
            comboX->addItem(name, i);
            comboY->addItem(name, i);
        }
        // 默认将第一列设为 X 轴，第二列设为 Y 轴
        if (m_model->columnCount() >= 2) {
            comboX->setCurrentIndex(0);
            comboY->setCurrentIndex(1);
        }
        loadRawData();
        updateSplitLines();
    }
}

/**
 * @brief 从模型中提取二维表和坐标数组
 */
void DataSamplingWidget::loadRawData()
{
    if (!m_model) return;

    m_rawTable.clear(); m_rawX.clear(); m_rawY.clear();
    int colX = comboX->currentData().toInt();
    int colY = comboY->currentData().toInt();
    m_xName = comboX->currentText();
    m_yName = comboY->currentText();

    double maxY = -1e9;

    for(int r = 0; r < m_model->rowCount(); ++r) {
        QStringList rowData;
        for(int c = 0; c < m_model->columnCount(); ++c) {
            rowData.append(m_model->index(r, c).data().toString());
        }

        bool okX, okY;
        double x = m_model->index(r, colX).data().toDouble(&okX);
        double y = m_model->index(r, colY).data().toDouble(&okY);

        if(okX && okY) {
            m_rawTable.append(rowData);
            m_rawX.append(x);
            m_rawY.append(y);
            if (y > maxY) maxY = y;
        }
    }

    // 动态更新异常点剔除的上限默认值
    if (maxY > -1e8) {
        spinMaxY->blockSignals(true);
        spinMaxY->setValue(maxY);
        spinMaxY->blockSignals(false);
    }

    // 初始状态下处理后数据与原始数据一致
    m_processedTable = m_rawTable;
    m_processedX = m_rawX;
    m_processedY = m_rawY;

    drawRawPlot();
    drawProcessedPlot();
}

void DataSamplingWidget::onDataColumnChanged() {
    loadRawData();
    updateSplitLines();
}

/**
 * @brief 内部辅助函数：计算对数坐标系下的完美范围
 */
static void adjustLogAxisRange(QCPAxis* axis, const QVector<double>& data) {
    double minPos = 1e-3, maxPos = 1e-3;
    bool found = false;
    for (double v : data) {
        if (v > 0) {
            if (!found) { minPos = v; maxPos = v; found = true; }
            else { if (v < minPos) minPos = v; if (v > maxPos) maxPos = v; }
        }
    }

    if (found && maxPos > minPos) {
        double minLog = std::log10(minPos);
        double maxLog = std::log10(maxPos);
        double lowerLog = std::floor(minLog);
        double upperLog = std::ceil(maxLog);

        if (minLog - lowerLog < 0.15) lowerLog -= 1.0;
        if (upperLog - maxLog < 0.15) upperLog += 1.0;

        if (upperLog <= lowerLog + 1.0) {
            lowerLog -= 1.0;
            upperLog += 1.0;
        }
        axis->setRange(std::pow(10, lowerLog), std::pow(10, upperLog));
    } else if (found && maxPos == minPos) {
        double logV = std::floor(std::log10(minPos));
        axis->setRange(std::pow(10, logV - 1), std::pow(10, logV + 1));
    } else {
        axis->setRange(1e-3, 1e3);
    }
}

/**
 * @brief 坐标系切换（线型/对数）时的图表更新逻辑
 */
void DataSamplingWidget::onAxisScaleChanged() {
    if (chkLogX->isChecked()) {
        customPlotRaw->xAxis->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTickerX1(new QCPAxisTickerLog);
        customPlotRaw->xAxis->setTicker(logTickerX1);

        customPlotProcessed->xAxis->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTickerX2(new QCPAxisTickerLog);
        customPlotProcessed->xAxis->setTicker(logTickerX2);
    } else {
        customPlotRaw->xAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTickerX1(new QCPAxisTicker);
        customPlotRaw->xAxis->setTicker(linTickerX1);

        customPlotProcessed->xAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTickerX2(new QCPAxisTicker);
        customPlotProcessed->xAxis->setTicker(linTickerX2);
    }

    if (chkLogY->isChecked()) {
        customPlotRaw->yAxis->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTickerY1(new QCPAxisTickerLog);
        customPlotRaw->yAxis->setTicker(logTickerY1);

        customPlotProcessed->yAxis->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTickerY2(new QCPAxisTickerLog);
        customPlotProcessed->yAxis->setTicker(logTickerY2);
    } else {
        customPlotRaw->yAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTickerY1(new QCPAxisTicker);
        customPlotRaw->yAxis->setTicker(linTickerY1);

        customPlotProcessed->yAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTickerY2(new QCPAxisTicker);
        customPlotProcessed->yAxis->setTicker(linTickerY2);
    }

    drawRawPlot();
    drawProcessedPlot();
}

// =========================================================================
// =                             边界线交互与绘制模块                          =
// =========================================================================

/**
 * @brief 自动计算边界线初始位置，将其设置在数据的 10% 和 50% 处
 */
void DataSamplingWidget::updateSplitLines() {
    if(m_rawX.isEmpty()) return;
    double minX = *std::min_element(m_rawX.begin(), m_rawX.end());
    double maxX = *std::max_element(m_rawX.begin(), m_rawX.end());
    if (minX == maxX) maxX += 1.0;

    double s1 = minX + (maxX - minX) * 0.1;
    double s2 = minX + (maxX - minX) * 0.5;

    spinSplit1->blockSignals(true); spinSplit2->blockSignals(true);
    spinSplit1->setValue(s1);       spinSplit2->setValue(s2);
    spinSplit1->blockSignals(false);spinSplit2->blockSignals(false);

    onSplitBoxValueChanged();
}

/**
 * @brief 将数值框中的值同步更新到图表上的直线位置
 */
void DataSamplingWidget::onSplitBoxValueChanged() {
    double s1 = spinSplit1->value();
    double s2 = spinSplit2->value();
    // 防错控制：中晚期边界永远大于早期边界
    if (s1 > s2) { s1 = s2 - 1e-4; spinSplit1->setValue(s1); }

    vLine1->point1->setCoords(s1, 0); vLine1->point2->setCoords(s1, 1);
    vLine2->point1->setCoords(s2, 0); vLine2->point2->setCoords(s2, 1);

    vLine1_proc->point1->setCoords(s1, 0); vLine1_proc->point2->setCoords(s1, 1);
    vLine2_proc->point1->setCoords(s2, 0); vLine2_proc->point2->setCoords(s2, 1);

    customPlotRaw->replot();
    customPlotProcessed->replot();
}

/** @brief 捕获鼠标点击了图表上哪根边界线 */
void DataSamplingWidget::onRawPlotMousePress(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        double x = customPlotRaw->xAxis->pixelToCoord(event->pos().x());
        double s1 = vLine1->point1->key();
        double s2 = vLine2->point1->key();
        double range = customPlotRaw->xAxis->range().size();
        double tol = range * 0.05; // 点击容差

        if (std::abs(x - s1) < tol && std::abs(x - s1) <= std::abs(x - s2)) m_dragLineIndex = 1;
        else if (std::abs(x - s2) < tol) m_dragLineIndex = 2;
        else m_dragLineIndex = 0;
    }
}

/** @brief 实现拖动更新 */
void DataSamplingWidget::onRawPlotMouseMove(QMouseEvent *event) {
    if (m_dragLineIndex != 0) {
        double x = customPlotRaw->xAxis->pixelToCoord(event->pos().x());
        if (m_dragLineIndex == 1) {
            double s2 = spinSplit2->value();
            if (x >= s2) x = s2 - 1e-4;
            spinSplit1->setValue(x);
        } else if (m_dragLineIndex == 2) {
            double s1 = spinSplit1->value();
            if (x <= s1) x = s1 + 1e-4;
            spinSplit2->setValue(x);
        }
    }
}

/** @brief 释放拖拽状态 */
void DataSamplingWidget::onRawPlotMouseRelease(QMouseEvent *) {
    m_dragLineIndex = 0;
}

void DataSamplingWidget::drawRawPlot() {
    customPlotRaw->clearGraphs();
    customPlotRaw->addGraph();
    customPlotRaw->graph(0)->setData(m_rawX, m_rawY);

    QPen redPen(Qt::red);
    redPen.setWidthF(1.0);
    customPlotRaw->graph(0)->setPen(redPen);
    customPlotRaw->graph(0)->setLineStyle(QCPGraph::lsLine);
    customPlotRaw->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 3));
    customPlotRaw->graph(0)->setName(QString("原始数据 (共 %1 点)").arg(m_rawX.size()));

    customPlotRaw->xAxis->setLabel(m_xName);
    customPlotRaw->yAxis->setLabel(m_yName);

    if (chkLogX->isChecked()) adjustLogAxisRange(customPlotRaw->xAxis, m_rawX);
    else customPlotRaw->xAxis->rescale();

    if (chkLogY->isChecked()) adjustLogAxisRange(customPlotRaw->yAxis, m_rawY);
    else customPlotRaw->yAxis->rescale();

    customPlotRaw->legend->setVisible(true);
    customPlotRaw->replot();
}

void DataSamplingWidget::drawProcessedPlot() {
    customPlotProcessed->clearGraphs();
    customPlotProcessed->addGraph();
    customPlotProcessed->graph(0)->setData(m_processedX, m_processedY);

    QPen darkBluePen(Qt::darkBlue);
    darkBluePen.setWidthF(1.5);
    customPlotProcessed->graph(0)->setPen(darkBluePen);
    customPlotProcessed->graph(0)->setLineStyle(QCPGraph::lsLine);
    customPlotProcessed->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 4));
    customPlotProcessed->graph(0)->setName(QString("处理后数据 (共 %1 点)").arg(m_processedX.size()));

    customPlotProcessed->xAxis->setLabel(m_xName);
    customPlotProcessed->yAxis->setLabel(m_yName);

    if (chkLogX->isChecked()) adjustLogAxisRange(customPlotProcessed->xAxis, m_processedX);
    else customPlotProcessed->xAxis->rescale();

    if (chkLogY->isChecked()) adjustLogAxisRange(customPlotProcessed->yAxis, m_processedY);
    else customPlotProcessed->yAxis->rescale();

    customPlotProcessed->legend->setVisible(true);
    customPlotProcessed->replot();
}

// =========================================================================
// =                             核心算法处理模块                              =
// =========================================================================

void DataSamplingWidget::onPreviewClicked() {
    processData();
    drawProcessedPlot();
}

void DataSamplingWidget::onResetClicked() {
    m_processedTable = m_rawTable;
    m_processedX = m_rawX;
    m_processedY = m_rawY;
    drawProcessedPlot();
}

/**
 * @brief 执行异常点剔除、阶段划分、阶段滤波、阶段抽样主流程
 */
void DataSamplingWidget::processData()
{
    if (m_rawTable.isEmpty()) return;

    double s1 = spinSplit1->value();
    double s2 = spinSplit2->value();
    double minY = spinMinY->value();
    double maxY = spinMaxY->value();

    int colY = comboY->currentData().toInt();

    QVector<QStringList> tb[3];
    QVector<double> tx[3], ty[3];

    // 第一步：全局数据遍历，执行上下限剔除，并基于X轴分配到不同阶段数组
    for (int i = 0; i < m_rawTable.size(); ++i) {
        double x = m_rawX[i];
        double y = m_rawY[i];
        if (y < minY || y > maxY) continue;

        int st = (x < s1) ? 0 : ((x < s2) ? 1 : 2);
        tb[st].append(m_rawTable[i]);
        tx[st].append(x);
        ty[st].append(y);
    }

    m_processedTable.clear(); m_processedX.clear(); m_processedY.clear();

    // 第二步：分阶段处理
    for (int i = 0; i < 3; ++i) {
        if (tx[i].isEmpty()) continue;

        // 滤波处理
        if (m_stages[i].comboFilter->currentIndex() == 1) {
            applyMedianFilter(ty[i], m_stages[i].spinFilterWin->value());
            // 将滤波改变后的 Y 值重新写入到二维表的字面字符串中
            for(int j = 0; j < ty[i].size(); ++j) {
                tb[i][j][colY] = QString::number(ty[i][j], 'f', 6);
            }
        }

        // 抽样处理
        int sType = m_stages[i].comboSample->currentIndex();
        int sVal = m_stages[i].spinSampleVal->value();
        QVector<int> indices;

        if (sType == 0) indices = getLogSamplingIndices(tx[i], sVal);
        else if (sType == 1) indices = getIntervalSamplingIndices(tx[i].size(), sVal);
        else if (sType == 2) indices = getTotalPointsSamplingIndices(tx[i].size(), sVal);

        // 合并生成该阶段最终保留的数据点
        for (int idx : indices) {
            m_processedTable.append(tb[i][idx]);
            m_processedX.append(tx[i][idx]);
            m_processedY.append(ty[i][idx]);
        }
    }
}

/**
 * @brief 一维序列中值滤波算法
 * @param y 待滤波序列 (引用传递，原位修改)
 * @param windowSize 滑动窗口大小
 */
void DataSamplingWidget::applyMedianFilter(QVector<double>& y, int windowSize) {
    if (y.size() < windowSize) return;
    int halfWin = windowSize / 2;
    QVector<double> y_filtered = y;
    for (int i = halfWin; i < y.size() - halfWin; ++i) {
        QVector<double> winData;
        for (int j = -halfWin; j <= halfWin; ++j) winData.append(y[i + j]);
        std::sort(winData.begin(), winData.end());
        y_filtered[i] = winData[winData.size() / 2];
    }
    y = y_filtered;
}

/**
 * @brief 对数阶梯抽样算法（常用于试井等存在对数周期规律的数据）
 * @param x 依据的 X 坐标数组
 * @param pointsPerDecade 每十个数量级采集点数
 */
QVector<int> DataSamplingWidget::getLogSamplingIndices(const QVector<double>& x, int pointsPerDecade) {
    QVector<int> indices;
    if (x.isEmpty() || pointsPerDecade <= 0) return indices;
    indices.append(0);

    double xMin = x.first() > 0 ? x.first() : 1e-4;
    double logStep = 1.0 / pointsPerDecade;
    double nextTargetLog = std::log10(xMin) + logStep;

    for (int i = 1; i < x.size() - 1; ++i) {
        if (x[i] > 0 && std::log10(x[i]) >= nextTargetLog) {
            indices.append(i);
            nextTargetLog += logStep;
        }
    }
    if (indices.last() != x.size() - 1) indices.append(x.size() - 1); // 强保末尾点
    return indices;
}

/**
 * @brief 等间隔抽样算法
 */
QVector<int> DataSamplingWidget::getIntervalSamplingIndices(int totalCount, int interval) {
    QVector<int> indices;
    if (interval <= 1) {
        for(int i=0; i<totalCount; ++i) indices.append(i);
        return indices;
    }
    for (int i = 0; i < totalCount; i += interval) indices.append(i);
    if (indices.last() != totalCount - 1) indices.append(totalCount - 1);
    return indices;
}

/**
 * @brief 总点数强制均分抽样算法
 */
QVector<int> DataSamplingWidget::getTotalPointsSamplingIndices(int totalCount, int targetPoints) {
    QVector<int> indices;
    if (totalCount <= targetPoints || targetPoints <= 2) {
        for(int i=0; i<totalCount; ++i) indices.append(i);
        return indices;
    }
    double step = (double)(totalCount - 1) / (targetPoints - 1);
    for (int i = 0; i < targetPoints; ++i) {
        int idx = qRound(i * step);
        if (idx >= totalCount) idx = totalCount - 1;
        indices.append(idx);
    }
    return indices;
}

QVector<QStringList> DataSamplingWidget::getProcessedTable() const {
    return m_processedTable;
}

QStringList DataSamplingWidget::getHeaders() const {
    return m_headers;
}
