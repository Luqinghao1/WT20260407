/*
 * 文件名: wt_datawidget.cpp
 * 文件作用: 数据编辑器主窗口实现文件
 * 功能描述:
 * 1. 管理 QTabWidget，支持多标签页显示多份数据文件。
 * 2. 实现了多文件同时打开、解析与展示的功能，并提供数据修改代理。
 * 3. 动态配置9大功能按键（矢量图标+文字），实现与内部表格的解耦。
 * 4. 【核心亮点】悬浮窗(Overlay)技术实现“滤波取样”面板嵌入，不破坏原界面布局，
 * 自动探测底层 QTableView 的宽度实现完美右侧贴边，并支持鼠标按住左侧边缘拉伸宽度。
 * 5. 处理各种数据保存、列属性替换和格式转换，杜绝了科学计数法。
 */

#include "wt_datawidget.h"
#include "ui_wt_datawidget.h"
#include "modelparameter.h"
#include "dataimportdialog.h"
#include "datasamplingdialog.h"
#include "dataunitdialog.h"
#include "dataunitmanager.h"

// Qt GUI 与事件模块
#include <QToolButton>
#include <QPainter>
#include <QPixmap>
#include <QPolygon>
#include <QResizeEvent>
#include <QMouseEvent>

// Qt 视图与模型模块
#include <QTableView>
#include <QHeaderView>
#include <QScrollBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QDateTime>

// 第三方 Excel 模块库
#include "xlsxdocument.h"
#include "xlsxformat.h"

/**
 * @brief 内部辅助函数：统一应用数据对话框的样式，灰底黑字提高可视性
 * @param dialog 需要被设置样式的 QWidget 指针
 */
static void applyDataDialogStyle(QWidget* dialog) {
    if (!dialog) return;
    QString qss = "QWidget { color: black; background-color: white; font-family: 'Microsoft YaHei'; }"
                  "QPushButton { "
                  "   background-color: #f0f0f0; "
                  "   color: black; "
                  "   border: 1px solid #bfbfbf; "
                  "   border-radius: 3px; "
                  "   padding: 5px 15px; "
                  "   min-width: 70px; "
                  "}"
                  "QPushButton:hover { background-color: #e0e0e0; }"
                  "QPushButton:pressed { background-color: #d0d0d0; }";
    dialog->setStyleSheet(qss);
}

/**
 * @brief 内部辅助函数：将双精度浮点数格式化为常规字符串，杜绝科学计数法
 * @param val 传入的浮点数值
 * @return 转换后的常规格式字符串，并自动去除末尾多余的 0 和小数点
 */
static QString formatToNormalNumber(double val) {
    QString res = QString::number(val, 'f', 8);
    if (res.contains('.')) {
        while (res.endsWith('0')) res.chop(1);
        if (res.endsWith('.')) res.chop(1);
    }
    return res;
}

// =========================================================================
// =                           构造与初始化模块                               =
// =========================================================================

WT_DataWidget::WT_DataWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WT_DataWidget),
    m_samplingWidget(nullptr),      // 初始化滤波取样悬浮界面为空指针
    m_isSamplingMaximized(false),   // 初始化不为最大化状态
    m_floatingWidth(-1)             // 初始宽度 -1 代表交给程序自动计算空白区域贴合
{
    ui->setupUi(this);
    initUI();
    setupConnections();
}

WT_DataWidget::~WT_DataWidget()
{
    delete ui;
}

/**
 * @brief 使用 QPainter 在内存中绘制扁平化风格的矢量图标
 * @param iconType 功能标识名
 * @return 绘制好的 QIcon 对象
 * 作用：替代缺失的图片资源，确保界面拥有上图标下文字的美观展现
 */
QIcon WT_DataWidget::createCustomIcon(const QString& iconType)
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent); // 透明背景
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing); // 开启抗锯齿

    if (iconType == "open") {
        painter.setBrush(QColor("#FFCA28"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(8, 24, 48, 32);
        painter.drawPolygon(QPolygon({QPoint(8, 24), QPoint(20, 12), QPoint(32, 12), QPoint(44, 24)}));
    }
    else if (iconType == "save") {
        painter.setBrush(QColor("#4CAF50"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(12, 12, 40, 40);
        painter.setBrush(Qt::white);
        painter.drawRect(20, 12, 24, 16);
        painter.drawRect(24, 36, 16, 16);
    }
    else if (iconType == "export") {
        painter.setBrush(QColor("#43A047"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(10, 10, 44, 44);
        painter.setPen(QPen(Qt::white, 4));
        painter.drawLine(10, 26, 54, 26);
        painter.drawLine(32, 10, 32, 54);
    }
    else if (iconType == "unit") {
        painter.setBrush(QColor("#00ACC1"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(8, 26, 48, 14);
        painter.setPen(QPen(Qt::white, 2));
        for (int i = 12; i <= 52; i += 8) {
            painter.drawLine(i, 26, i, (i % 16 == 12) ? 34 : 31);
        }
    }
    else if (iconType == "time") {
        painter.setBrush(QColor("#FF9800"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(12, 12, 40, 40);
        painter.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(32, 32, 32, 20);
        painter.drawLine(32, 32, 42, 32);
    }
    else if (iconType == "pressuredrop") {
        painter.setBrush(QColor("#E53935"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygon({QPoint(24, 10), QPoint(40, 10), QPoint(40, 34), QPoint(50, 34), QPoint(32, 54), QPoint(14, 34), QPoint(24, 34)}));
    }
    else if (iconType == "pwf") {
        painter.setBrush(QColor("#3949AB"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(24, 8, 16, 48);
        painter.setBrush(QColor("#29B6F6"));
        painter.drawRect(24, 36, 16, 20);
    }
    else if (iconType == "filter") {
        painter.setBrush(QColor("#8E24AA"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygon({QPoint(10, 12), QPoint(54, 12), QPoint(36, 32), QPoint(36, 52), QPoint(28, 52), QPoint(28, 32)}));
    }
    else if (iconType == "error") {
        painter.setBrush(QColor("#F4511E"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygon({QPoint(32, 10), QPoint(56, 52), QPoint(8, 52)}));
        painter.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(32, 24, 32, 38);
        painter.drawPoint(32, 46);
    }
    return QIcon(pixmap);
}

/**
 * @brief 界面控件初始化，将9个按钮统一配置样式
 */
void WT_DataWidget::initUI()
{
    // 获取UI中已排好一排的QToolButton
    QList<QToolButton*> toolButtons = {
        ui->btnOpenFile, ui->btnSave, ui->btnExport,
        ui->btnUnitManager, ui->btnTimeConvert, ui->btnPressureDropCalc,
        ui->btnCalcPwf, ui->btnFilterSample, ui->btnErrorCheck
    };

    // 对应的图标代号数组
    QStringList iconTypes = {
        "open", "save", "export",
        "unit", "time", "pressuredrop",
        "pwf", "filter", "error"
    };

    // 循环遍历设置按钮样式
    for (int i = 0; i < toolButtons.size(); ++i) {
        if (toolButtons[i]) {
            toolButtons[i]->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            toolButtons[i]->setIcon(createCustomIcon(iconTypes[i]));
            toolButtons[i]->setIconSize(QSize(36, 36));
            toolButtons[i]->setMinimumSize(QSize(80, 75));
            toolButtons[i]->setMaximumSize(QSize(100, 90));
        }
    }
    updateButtonsState();
}

/**
 * @brief 绑定主界面所有按键到各个功能槽函数
 */
void WT_DataWidget::setupConnections()
{
    // 顶部工具栏 9 大按钮
    connect(ui->btnOpenFile, &QToolButton::clicked, this, &WT_DataWidget::onOpenFile);
    connect(ui->btnSave, &QToolButton::clicked, this, &WT_DataWidget::onSave);
    connect(ui->btnExport, &QToolButton::clicked, this, &WT_DataWidget::onExportExcel);
    connect(ui->btnUnitManager, &QToolButton::clicked, this, &WT_DataWidget::onUnitManager);
    connect(ui->btnTimeConvert, &QToolButton::clicked, this, &WT_DataWidget::onTimeConvert);
    connect(ui->btnPressureDropCalc, &QToolButton::clicked, this, &WT_DataWidget::onPressureDropCalc);
    connect(ui->btnCalcPwf, &QToolButton::clicked, this, &WT_DataWidget::onCalcPwf);
    connect(ui->btnFilterSample, &QToolButton::clicked, this, &WT_DataWidget::onFilterSample);
    connect(ui->btnErrorCheck, &QToolButton::clicked, this, &WT_DataWidget::onHighlightErrors);

    // 标签页切换与关闭
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &WT_DataWidget::onTabChanged);
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &WT_DataWidget::onTabCloseRequested);
}

/**
 * @brief 动态更新9个按钮的可点击状态(无数据时置灰)
 */
void WT_DataWidget::updateButtonsState()
{
    bool hasSheet = (ui->tabWidget->count() > 0);
    // 打开按钮永远可用，其它依赖数据的按钮视情况而定
    ui->btnSave->setEnabled(hasSheet);
    ui->btnExport->setEnabled(hasSheet);
    ui->btnUnitManager->setEnabled(hasSheet);
    ui->btnTimeConvert->setEnabled(hasSheet);
    ui->btnPressureDropCalc->setEnabled(hasSheet);
    ui->btnCalcPwf->setEnabled(hasSheet);
    ui->btnFilterSample->setEnabled(hasSheet);
    ui->btnErrorCheck->setEnabled(hasSheet);

    // 更新底部状态栏的文件名显示
    if (auto sheet = currentSheet()) {
        ui->filePathLabel->setText(sheet->getFilePath());
    } else {
        ui->filePathLabel->setText("未加载文件");
    }
}


// =========================================================================
// =                         数据获取与状态代理模块                           =
// =========================================================================

DataSingleSheet* WT_DataWidget::currentSheet() const {
    return qobject_cast<DataSingleSheet*>(ui->tabWidget->currentWidget());
}

QStandardItemModel* WT_DataWidget::getDataModel() const {
    if (auto sheet = currentSheet()) return sheet->getDataModel();
    return nullptr;
}

QMap<QString, QStandardItemModel*> WT_DataWidget::getAllDataModels() const {
    QMap<QString, QStandardItemModel*> map;
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        DataSingleSheet* sheet = qobject_cast<DataSingleSheet*>(ui->tabWidget->widget(i));
        if (sheet) {
            QString key = sheet->getFilePath();
            if (key.isEmpty()) key = ui->tabWidget->tabText(i);
            map.insert(key, sheet->getDataModel());
        }
    }
    return map;
}

QString WT_DataWidget::getCurrentFileName() const {
    if (auto sheet = currentSheet()) return sheet->getFilePath();
    return QString();
}

bool WT_DataWidget::hasData() const {
    return ui->tabWidget->count() > 0;
}


// =========================================================================
// =                            文件 I/O 模块                                =
// =========================================================================

void WT_DataWidget::onOpenFile()
{
    QString filter = "所有支持文件 (*.csv *.txt *.xlsx *.xls);;Excel (*.xlsx *.xls);;CSV 文件 (*.csv);;文本文件 (*.txt);;所有文件 (*.*)";
    QStringList paths = QFileDialog::getOpenFileNames(this, "打开数据文件", "", filter);
    if (paths.isEmpty()) return;

    for (const QString& path : paths) {
        // 如果是项目 json 配置文件则走单独解析
        if (path.endsWith(".json", Qt::CaseInsensitive)) {
            loadData(path, "json");
            return;
        }

        // 调用数据导入格式向导窗口
        DataImportDialog dlg(path, this);
        applyDataDialogStyle(&dlg);

        if (dlg.exec() == QDialog::Accepted) {
            DataImportSettings settings = dlg.getSettings();
            createNewTab(path, settings);
        }
    }
}

void WT_DataWidget::createNewTab(const QString& filePath, const DataImportSettings& settings) {
    DataSingleSheet* sheet = new DataSingleSheet(this);
    if (sheet->loadData(filePath, settings)) {
        QFileInfo fi(filePath);
        ui->tabWidget->addTab(sheet, fi.fileName());
        ui->tabWidget->setCurrentWidget(sheet);
        // 绑定单表的数据变更信号，向上传递
        connect(sheet, &DataSingleSheet::dataChanged, this, &WT_DataWidget::onSheetDataChanged);

        updateButtonsState();
        emit fileChanged(filePath, "text");
        emit dataChanged();
    } else {
        delete sheet;
        ui->statusLabel->setText("加载文件失败: " + filePath);
    }
}

void WT_DataWidget::loadData(const QString& filePath, const QString& fileType)
{
    if (fileType == "json") return; // 暂由项目管理器处理 json
    DataImportDialog dlg(filePath, this);
    applyDataDialogStyle(&dlg);
    if (dlg.exec() == QDialog::Accepted) {
        createNewTab(filePath, dlg.getSettings());
    }
}

void WT_DataWidget::onSave() {
    QJsonArray allData;
    // 遍历所有 Tab 获取子表格的序列化 JSON 数据
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        DataSingleSheet* sheet = qobject_cast<DataSingleSheet*>(ui->tabWidget->widget(i));
        if (sheet) allData.append(sheet->saveToJson());
    }
    ModelParameter::instance()->saveTableData(allData);
    ModelParameter::instance()->saveProject();

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("保存");
    msgBox.setText("所有标签页数据已同步保存到项目文件。");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.addButton(QMessageBox::Ok);
    applyDataDialogStyle(&msgBox);
    msgBox.exec();
}

void WT_DataWidget::loadFromProjectData() {
    clearAllData();
    QJsonArray dataArray = ModelParameter::instance()->getTableData();
    if (dataArray.isEmpty()) {
        ui->statusLabel->setText("无数据");
        return;
    }

    // 兼容新老保存格式判断
    bool isNewFormat = false;
    if (!dataArray.isEmpty()) {
        QJsonValue first = dataArray.first();
        if (first.isObject() && first.toObject().contains("filePath") && first.toObject().contains("data")) {
            isNewFormat = true;
        }
    }

    if (isNewFormat) {
        for (auto val : dataArray) {
            QJsonObject sheetObj = val.toObject();
            DataSingleSheet* sheet = new DataSingleSheet(this);
            sheet->loadFromJson(sheetObj);
            QString path = sheet->getFilePath();
            QFileInfo fi(path);
            ui->tabWidget->addTab(sheet, fi.fileName().isEmpty() ? "恢复数据" : fi.fileName());
            connect(sheet, &DataSingleSheet::dataChanged, this, &WT_DataWidget::onSheetDataChanged);
        }
    } else {
        DataSingleSheet* sheet = new DataSingleSheet(this);
        QJsonObject sheetObj;
        sheetObj["filePath"] = "Restored Data";
        if (!dataArray.isEmpty() && dataArray.first().toObject().contains("headers")) {
            sheetObj["headers"] = dataArray.first().toObject()["headers"];
        }
        QJsonArray rows;
        for (int i = 1; i < dataArray.size(); ++i) {
            QJsonObject rowObj = dataArray[i].toObject();
            if (rowObj.contains("row_data")) rows.append(rowObj["row_data"]);
        }
        sheetObj["data"] = rows;
        sheet->loadFromJson(sheetObj);
        ui->tabWidget->addTab(sheet, "恢复数据");
        connect(sheet, &DataSingleSheet::dataChanged, this, &WT_DataWidget::onSheetDataChanged);
    }

    updateButtonsState();
    ui->statusLabel->setText("数据已恢复");
}

void WT_DataWidget::clearAllData() {
    ui->tabWidget->clear();
    ui->filePathLabel->setText("未加载文件");
    ui->statusLabel->setText("无数据");
    updateButtonsState();
    emit dataChanged();
}


// =========================================================================
// =                            功能计算与代理模块                             =
// =========================================================================

void WT_DataWidget::onExportExcel() { if (auto s = currentSheet()) s->onExportExcel(); }
void WT_DataWidget::onTimeConvert() { if (auto s = currentSheet()) s->onTimeConvert(); }
void WT_DataWidget::onPressureDropCalc() { if (auto s = currentSheet()) s->onPressureDropCalc(); }
void WT_DataWidget::onCalcPwf() { if (auto s = currentSheet()) s->onCalcPwf(); }
void WT_DataWidget::onHighlightErrors() { if (auto s = currentSheet()) s->onHighlightErrors(); }

/**
 * @brief 响应单位管理按钮，处理复杂的列数据提取与计算换算
 */
void WT_DataWidget::onUnitManager()
{
    QStandardItemModel* model = getDataModel();
    if (!model || model->rowCount() == 0) {
        QMessageBox::warning(this, "提示", "当前无数据可操作！");
        return;
    }

    DataUnitDialog dialog(model, this);
    if (dialog.exec() != QDialog::Accepted) return;

    auto tasks = dialog.getConversionTasks();
    if (tasks.isEmpty()) return;

    bool appendMode = dialog.isAppendMode();
    bool saveToFile = dialog.isSaveToFile();
    int rowCount = model->rowCount();
    int colCount = model->columnCount();

    // 模式 A：在原表格上直接修改或者追加
    if (!saveToFile) {
        if (appendMode) {
            int insertPos = colCount;
            for (const auto& task : tasks) {
                model->insertColumn(insertPos);
                model->setHeaderData(insertPos, Qt::Horizontal, task.newHeaderName);
                for (int r = 0; r < rowCount; ++r) {
                    QString origText = model->item(r, task.colIndex)->text();
                    if (task.needsMathConversion) {
                        bool ok; double val = origText.toDouble(&ok);
                        if (ok) {
                            double newVal = DataUnitManager::instance()->convert(val, task.quantityType, task.fromUnit, task.toUnit);
                            model->setItem(r, insertPos, new QStandardItem(formatToNormalNumber(newVal)));
                        } else {
                            model->setItem(r, insertPos, new QStandardItem(""));
                        }
                    } else {
                        model->setItem(r, insertPos, new QStandardItem(origText));
                    }
                }
                insertPos++;
            }
        } else {
            for (const auto& task : tasks) {
                model->setHeaderData(task.colIndex, Qt::Horizontal, task.newHeaderName);
                if (task.needsMathConversion) {
                    for (int r = 0; r < rowCount; ++r) {
                        QString origText = model->item(r, task.colIndex)->text();
                        bool ok; double val = origText.toDouble(&ok);
                        if (ok) {
                            double newVal = DataUnitManager::instance()->convert(val, task.quantityType, task.fromUnit, task.toUnit);
                            model->item(r, task.colIndex)->setText(formatToNormalNumber(newVal));
                        }
                    }
                }
            }
        }
        emit dataChanged();
        ui->statusLabel->setText("列属性与单位已在当前表格中更新完毕。");

        // 模式 B：处理完毕后直接另存为一个独立的新文件页签
    } else {
        QVector<QStringList> fullTable;
        QStringList headers;

        // 构建新表头
        for (int c = 0; c < colCount; ++c) {
            headers.append(model->horizontalHeaderItem(c) ? model->horizontalHeaderItem(c)->text() : "");
        }
        if (appendMode) {
            for (const auto& task : tasks) headers.append(task.newHeaderName);
        } else {
            for (const auto& task : tasks) headers[task.colIndex] = task.newHeaderName;
        }

        // 遍历所有行进行数据推演计算
        for (int r = 0; r < rowCount; ++r) {
            QStringList rowData;
            for (int c = 0; c < colCount; ++c) rowData.append(model->item(r, c)->text());

            if (appendMode) {
                for (const auto& task : tasks) {
                    if (task.needsMathConversion) {
                        bool ok; double val = rowData[task.colIndex].toDouble(&ok);
                        if (ok) {
                            double nV = DataUnitManager::instance()->convert(val, task.quantityType, task.fromUnit, task.toUnit);
                            rowData.append(formatToNormalNumber(nV));
                        } else {
                            rowData.append("");
                        }
                    } else {
                        rowData.append(rowData[task.colIndex]);
                    }
                }
            } else {
                for (const auto& task : tasks) {
                    if (task.needsMathConversion) {
                        bool ok; double val = rowData[task.colIndex].toDouble(&ok);
                        if (ok) {
                            double nV = DataUnitManager::instance()->convert(val, task.quantityType, task.fromUnit, task.toUnit);
                            rowData[task.colIndex] = formatToNormalNumber(nV);
                        }
                    }
                }
            }
            fullTable.append(rowData);
        }

        QString currentPath = getCurrentFileName();
        saveAndLoadNewData(currentPath, headers, fullTable);
        ui->statusLabel->setText("列属性与单位已应用，并生成新数据文件。");
    }
}


// =========================================================================
// =                   悬浮窗模式的滤波与取样（完全剥离 QSplitter）             =
// =========================================================================

/**
 * @brief 响应滤波取样按钮，创建绝对定位的子窗口悬浮在表格上方
 */
void WT_DataWidget::onFilterSample()
{
    QStandardItemModel* model = getDataModel();
    if (!model || model->rowCount() == 0) {
        QMessageBox::warning(this, "提示", "当前无数据可处理！");
        return;
    }

    // 单例化模式避免重复创建
    if (!m_samplingWidget) {
        m_samplingWidget = new DataSamplingWidget(this);
        m_samplingWidget->setObjectName("DataSamplingWidget");
        // 使用明显的左边框和上边框与底层表格区分开
        m_samplingWidget->setStyleSheet("#DataSamplingWidget { border-left: 2px solid #1890FF; border-top: 1px solid #D9D9D9; background-color: #FFFFFF; }");

        // 绑定来自组件的控制指令
        connect(m_samplingWidget, &DataSamplingWidget::requestMaximize, this, &WT_DataWidget::onSamplingMaximize);
        connect(m_samplingWidget, &DataSamplingWidget::requestRestore, this, &WT_DataWidget::onSamplingRestore);
        connect(m_samplingWidget, &DataSamplingWidget::requestClose, this, &WT_DataWidget::onSamplingClose);
        connect(m_samplingWidget, &DataSamplingWidget::requestResize, this, &WT_DataWidget::onSamplingResized); // 监听用户拖拽左侧边缘
        connect(m_samplingWidget, &DataSamplingWidget::processingFinished, this, &WT_DataWidget::onSamplingFinished);
    }

    // 把当前焦点页签的数据注入
    m_samplingWidget->setModel(model);
    m_samplingWidget->show();
    m_samplingWidget->raise(); // 将其拉伸到 Z 轴最上层，避免被表格盖住

    m_isSamplingMaximized = false;
    m_floatingWidth = -1; // -1 告诉排版函数：请自动识别表格的空白区域去填满它
    updateSamplingWidgetGeometry();
}

/** @brief 接收组件内发出的最大化请求（完全盖住数据表）*/
void WT_DataWidget::onSamplingMaximize()
{
    m_isSamplingMaximized = true;
    updateSamplingWidgetGeometry();
}

/** @brief 接收组件内发出的恢复大小请求（退回并吸附在数据表右边）*/
void WT_DataWidget::onSamplingRestore()
{
    m_isSamplingMaximized = false;
    m_floatingWidth = -1; // 恢复时重置掉用户的自定宽度，重新自动吸附空白区域
    updateSamplingWidgetGeometry();
}

/** @brief 接收组件内发出的关闭请求 */
void WT_DataWidget::onSamplingClose()
{
    if (m_samplingWidget) {
        m_samplingWidget->hide();
    }
}

/**
 * @brief 接收组件被手动拖拽改变宽度的反馈并计算新坐标
 * @param dx 鼠标水平移动的差值
 */
void WT_DataWidget::onSamplingResized(int dx)
{
    if (m_isSamplingMaximized) return; // 逻辑保护：全屏状态不允许拖拽

    QRect geom = m_samplingWidget->geometry();
    int newWidth = geom.width() - dx;
    int newX = geom.x() + dx;

    // 约束1：最低保留 400 像素以呈现里面的图表元素
    if (newWidth < 400) {
        newX -= (400 - newWidth);
        newWidth = 400;
    }

    // 约束2：不允许拖得太宽越过了底层表格的可视左边界
    QWidget* currentTab = ui->tabWidget->currentWidget();
    QPoint topLeft = currentTab->mapTo(this, QPoint(0, 0));
    if (newX < topLeft.x()) {
        newWidth -= (topLeft.x() - newX);
        newX = topLeft.x();
    }

    m_floatingWidth = newWidth; // 记录用户手动设定的偏好宽度
    m_samplingWidget->setGeometry(newX, geom.y(), newWidth, geom.height());
}

/**
 * @brief 【核心亮点】动态坐标映射计算悬浮窗的具体位置
 * 作用：
 * 1. mapTo 确保能避开 QTabWidget 顶部的标签栏，完美与内部画布贴合。
 * 2. 结合底层 QTableView 的特性，计算出实际数据显示的真实像素宽度，让功能窗口严丝合缝地贴在数据表旁边。
 */
void WT_DataWidget::updateSamplingWidgetGeometry()
{
    if (!m_samplingWidget || !m_samplingWidget->isVisible()) return;

    QWidget* currentTab = ui->tabWidget->currentWidget();
    if (!currentTab) {
        m_samplingWidget->hide();
        return;
    }

    // 将内部 Pane 的相对坐标(0,0)转换为在当前主界面容器上的绝对坐标
    QPoint topLeft = currentTab->mapTo(this, QPoint(0, 0));
    QSize tabSize = currentTab->size();

    if (m_isSamplingMaximized) {
        // 全覆盖模式：起点在 Pane 的左上，长宽完全一致
        m_samplingWidget->setGeometry(topLeft.x(), topLeft.y(), tabSize.width(), tabSize.height());
    } else {
        int startX = topLeft.x();

        // 情形 A：用户已经通过拖拽边缘自定义过宽度
        if (m_floatingWidth > 0) {
            startX = topLeft.x() + tabSize.width() - m_floatingWidth;
        }
        // 情形 B：程序首次加载，智能计算底层数据表格真实占用的宽度
        else {
            QTableView* view = currentTab->findChild<QTableView*>();
            if (view) {
                // 表格实际占用的总宽 = 数据列宽之和 + 左侧行号宽 + 垂直滚动条宽 + 外边框冗余(2px)
                int columnsWidth = view->horizontalHeader()->length();
                int vHeaderWidth = view->verticalHeader()->isVisible() ? view->verticalHeader()->width() : 0;
                int scrollBarWidth = view->verticalScrollBar()->isVisible() ? view->verticalScrollBar()->width() : 0;

                int totalTableWidth = columnsWidth + vHeaderWidth + scrollBarWidth + 2;
                startX += totalTableWidth;
            } else {
                startX += tabSize.width() / 2; // 如果意外拿不到表格指针，给个保底的中分策略
            }

            // 安全限制：如果数据极其庞大铺满屏幕，防止悬浮窗被压缩消失，强行占其 450px 的右侧宽度
            if (startX > topLeft.x() + tabSize.width() - 450) {
                startX = topLeft.x() + tabSize.width() - 450;
            }
            // 实时把系统算出来的这个宽度赋值给 m_floatingWidth
            m_floatingWidth = topLeft.x() + tabSize.width() - startX;
        }

        int finalWidth = topLeft.x() + tabSize.width() - startX;
        m_samplingWidget->setGeometry(startX, topLeft.y(), finalWidth, tabSize.height());
    }

    // 更新完坐标后确保其没有被 Qt 的重绘机制盖在底下
    m_samplingWidget->raise();
}

/**
 * @brief 重载主界面缩放事件
 * 作用：当用户拉伸改变软件大小时，内部的悬浮面板也能像被磁铁吸附一样实时更新坐标。
 */
void WT_DataWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateSamplingWidgetGeometry();
}


// =========================================================================
// =                           事件回调与通信模块                             =
// =========================================================================

/**
 * @brief 接收组件内部处理后的最终数据
 */
void WT_DataWidget::onSamplingFinished(const QStringList& headers, const QVector<QStringList>& processedTable)
{
    if (processedTable.isEmpty()) {
        QMessageBox::warning(this, "错误", "处理后数据为空，请检查参数设置！");
        return;
    }
    QString currentPath = getCurrentFileName();
    saveAndLoadNewData(currentPath, headers, processedTable);
}

/**
 * @brief 基础数据保存逻辑：将完整二维表落盘并形成新的工作页签加载
 */
void WT_DataWidget::saveAndLoadNewData(const QString& oldFilePath, const QStringList& headers, const QVector<QStringList>& fullTable)
{
    QFileInfo fi(oldFilePath);
    QString baseName = fi.completeBaseName();
    QString dir = fi.absolutePath();

    // 构建默认带时间戳的文件名防止覆盖原始数据
    QString defaultName = QString("%1_处理后_%2.xlsx").arg(baseName).arg(QDateTime::currentDateTime().toString("HHmmss"));
    QString defaultPath = dir + "/" + defaultName;

    QString savePath = QFileDialog::getSaveFileName(this, "保存处理后的全表数据", defaultPath, "Excel文件 (*.xlsx *.xls);;CSV文件 (*.csv);;文本文件 (*.txt)");
    if (savePath.isEmpty()) return;

    bool isExcel = savePath.endsWith(".xlsx", Qt::CaseInsensitive) || savePath.endsWith(".xls", Qt::CaseInsensitive);

    if (isExcel) {
        QXlsx::Document xlsx;
        QXlsx::Format infoFormat;
        infoFormat.setFontColor(Qt::darkGray);
        xlsx.write(1, 1, "// PWT System: Data Processed/Standardized Data", infoFormat);
        xlsx.write(2, 1, "// Original File: " + fi.fileName(), infoFormat);

        QXlsx::Format headerFormat;
        headerFormat.setFontBold(true);
        headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
        headerFormat.setPatternBackgroundColor(QColor(240, 240, 240));

        for (int c = 0; c < headers.size(); ++c) {
            xlsx.write(3, c + 1, headers[c], headerFormat);
        }

        // 强行用 try-toDouble 来避免直接存字符串，以便在 Excel 内部仍然是纯数字单元格
        for (int r = 0; r < fullTable.size(); ++r) {
            for (int c = 0; c < fullTable[r].size(); ++c) {
                bool ok;
                double val = fullTable[r][c].toDouble(&ok);
                if (ok) xlsx.write(r + 4, c + 1, val);
                else xlsx.write(r + 4, c + 1, fullTable[r][c]);
            }
        }

        if (!xlsx.saveAs(savePath)) {
            QMessageBox::critical(this, "错误", "无法保存 Excel 文件，请确保文件未被其它程序占用。");
            return;
        }
    } else {
        QFile file(savePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, "错误", "无法创建或写入文件！");
            return;
        }
        QTextStream out(&file);
        out << "// PWT System: Data Processed/Standardized Data\n";
        out << "// Original File: " << fi.fileName() << "\n";
        out << headers.join(",") << "\n";
        for (const QStringList& row : fullTable) {
            out << row.join(",") << "\n";
        }
        file.close();
    }

    // 使用向导设定的标准进行自动挂载加载
    DataImportSettings settings;
    settings.filePath = savePath;
    settings.encoding = "UTF-8";
    settings.separator = ",";
    settings.useHeader = true;
    settings.headerRow = 3;
    settings.startRow = 4;
    settings.isExcel = isExcel;

    createNewTab(savePath, settings);
    ui->statusLabel->setText("处理完成，保留有效点数: " + QString::number(fullTable.size()));
}

/**
 * @brief 页签切换触发回调
 */
void WT_DataWidget::onTabChanged(int index) {
    Q_UNUSED(index);
    updateButtonsState();

    // 若悬浮面板仍然显示，随着页签的切换，面板内部展示的数据也要一起切换为新表
    if(m_samplingWidget && m_samplingWidget->isVisible()){
        m_samplingWidget->setModel(getDataModel());

        // 切页会导致底层表格的长宽、滚动条发生变化，需要同步校准悬浮窗的自动吸附坐标
        updateSamplingWidgetGeometry();
    }

    emit dataChanged();
}

/**
 * @brief 处理标签页删除
 */
void WT_DataWidget::onTabCloseRequested(int index) {
    QWidget* widget = ui->tabWidget->widget(index);
    if (widget) {
        ui->tabWidget->removeTab(index);
        delete widget;
    }
    updateButtonsState();

    // 如果没有数据表了，悬浮面板也应该随之强行消失
    if (!hasData() && m_samplingWidget) {
        m_samplingWidget->hide();
    } else if (hasData() && m_samplingWidget && m_samplingWidget->isVisible()){
        m_samplingWidget->setModel(getDataModel());
        updateSamplingWidgetGeometry();
    }

    emit dataChanged();
}

/**
 * @brief 代理向下子组件发出的数据变动信号
 */
void WT_DataWidget::onSheetDataChanged() {
    if (sender() == currentSheet()) emit dataChanged();
}
