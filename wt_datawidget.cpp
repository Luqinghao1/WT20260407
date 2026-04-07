/*
 * 文件名: wt_datawidget.cpp
 * 文件作用: 数据编辑器主窗口实现文件
 * 功能描述:
 * 1. 管理 QTabWidget，支持多标签页显示多份数据文件。
 * 2. 实现了多文件同时打开、解析与展示的功能。
 * 3. 将9个数据处理按钮配置为 `QToolButton` 并设定为上图标下文字的单排样式。
 * 4. 【核心重构】使用 QPainter 动态生成9款对应功能的矢量图标，彻底解决无图可用的问题。
 * 5. 杜绝了底层计算或单位换算时产生科学计数法的问题，统一输出常规格式。
 */

#include "wt_datawidget.h"
#include "ui_wt_datawidget.h"
#include "modelparameter.h"
#include "dataimportdialog.h"
#include "datasamplingdialog.h"
#include "dataunitdialog.h"
#include "dataunitmanager.h"
#include <QToolButton>
#include <QPainter>
#include <QPixmap>
#include <QPolygon>

// 引入 QXlsx 支持数据直接写入 Excel
#include "xlsxdocument.h"
#include "xlsxformat.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QDateTime>

/**
 * @brief 统一应用数据对话框的样式，灰底黑字提高可视性
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
 * @brief 将双精度浮点数格式化为常规字符串，杜绝科学计数法
 */
static QString formatToNormalNumber(double val) {
    QString res = QString::number(val, 'f', 8);
    if (res.contains('.')) {
        while (res.endsWith('0')) res.chop(1);
        if (res.endsWith('.')) res.chop(1);
    }
    return res;
}

WT_DataWidget::WT_DataWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WT_DataWidget)
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
    painter.setRenderHint(QPainter::Antialiasing); // 抗锯齿开启

    if (iconType == "open") {
        // 绘制黄色文件夹 (打开数据)
        painter.setBrush(QColor("#FFCA28"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(8, 24, 48, 32);
        painter.drawPolygon(QPolygon({QPoint(8, 24), QPoint(20, 12), QPoint(32, 12), QPoint(44, 24)}));
    }
    else if (iconType == "save") {
        // 绘制绿色保存按钮(软盘概念)
        painter.setBrush(QColor("#4CAF50"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(12, 12, 40, 40);
        painter.setBrush(Qt::white);
        painter.drawRect(20, 12, 24, 16); // 顶部标签
        painter.drawRect(24, 36, 16, 16); // 底部写入区
    }
    else if (iconType == "export") {
        // 绘制绿色表格 (导出Excel)
        painter.setBrush(QColor("#43A047"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(10, 10, 44, 44);
        painter.setPen(QPen(Qt::white, 4));
        painter.drawLine(10, 26, 54, 26);
        painter.drawLine(32, 10, 32, 54);
    }
    else if (iconType == "unit") {
        // 绘制青色标尺 (单位管理)
        painter.setBrush(QColor("#00ACC1"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(8, 26, 48, 14);
        painter.setPen(QPen(Qt::white, 2));
        for (int i = 12; i <= 52; i += 8) {
            painter.drawLine(i, 26, i, (i % 16 == 12) ? 34 : 31);
        }
    }
    else if (iconType == "time") {
        // 绘制橙色时钟 (时间转换)
        painter.setBrush(QColor("#FF9800"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(12, 12, 40, 40);
        painter.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(32, 32, 32, 20); // 时针
        painter.drawLine(32, 32, 42, 32); // 分针
    }
    else if (iconType == "pressuredrop") {
        // 绘制红色下降箭头 (计算压降)
        painter.setBrush(QColor("#E53935"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygon({QPoint(24, 10), QPoint(40, 10), QPoint(40, 34), QPoint(50, 34), QPoint(32, 54), QPoint(14, 34), QPoint(24, 34)}));
    }
    else if (iconType == "pwf") {
        // 绘制深蓝井筒结构 (套压转流压)
        painter.setBrush(QColor("#3949AB"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(24, 8, 16, 48); // 井管
        painter.setBrush(QColor("#29B6F6"));
        painter.drawRect(24, 36, 16, 20); // 内部流体
    }
    else if (iconType == "filter") {
        // 绘制紫色漏斗 (滤波取样)
        painter.setBrush(QColor("#8E24AA"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygon({QPoint(10, 12), QPoint(54, 12), QPoint(36, 32), QPoint(36, 52), QPoint(28, 52), QPoint(28, 32)}));
    }
    else if (iconType == "error") {
        // 绘制深橙警示牌 (异常检查)
        painter.setBrush(QColor("#F4511E"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygon({QPoint(32, 10), QPoint(56, 52), QPoint(8, 52)}));
        painter.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(32, 24, 32, 38); // 惊叹号竖线
        painter.drawPoint(32, 46);        // 惊叹号底点
    }

    return QIcon(pixmap);
}

/**
 * @brief 界面控件初始化，将9个按钮统一配置样式和自主生成的图标
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
            // 设置枚举：图标在上，文字在下
            toolButtons[i]->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            // 调用程序化绘图赋予独立图标
            toolButtons[i]->setIcon(createCustomIcon(iconTypes[i]));
            // 设定统一的图标显示大小
            toolButtons[i]->setIconSize(QSize(36, 36));
            // 设定舒适的宽容尺寸，防止由于文字字数不同导致参差不齐
            toolButtons[i]->setMinimumSize(QSize(80, 75));
            toolButtons[i]->setMaximumSize(QSize(100, 90));
        }
    }

    // 根据初始数据更新是否置灰
    updateButtonsState();
}

/**
 * @brief 绑定界面按键到各个功能槽函数
 */
void WT_DataWidget::setupConnections()
{
    connect(ui->btnOpenFile, &QToolButton::clicked, this, &WT_DataWidget::onOpenFile);
    connect(ui->btnSave, &QToolButton::clicked, this, &WT_DataWidget::onSave);
    connect(ui->btnExport, &QToolButton::clicked, this, &WT_DataWidget::onExportExcel);
    connect(ui->btnUnitManager, &QToolButton::clicked, this, &WT_DataWidget::onUnitManager);
    connect(ui->btnTimeConvert, &QToolButton::clicked, this, &WT_DataWidget::onTimeConvert);
    connect(ui->btnPressureDropCalc, &QToolButton::clicked, this, &WT_DataWidget::onPressureDropCalc);
    connect(ui->btnCalcPwf, &QToolButton::clicked, this, &WT_DataWidget::onCalcPwf);
    connect(ui->btnFilterSample, &QToolButton::clicked, this, &WT_DataWidget::onFilterSample);
    connect(ui->btnErrorCheck, &QToolButton::clicked, this, &WT_DataWidget::onHighlightErrors);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &WT_DataWidget::onTabChanged);
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &WT_DataWidget::onTabCloseRequested);
}

DataSingleSheet* WT_DataWidget::currentSheet() const {
    return qobject_cast<DataSingleSheet*>(ui->tabWidget->currentWidget());
}

QStandardItemModel* WT_DataWidget::getDataModel() const {
    if (auto sheet = currentSheet()) return sheet->getDataModel();
    return nullptr;
}

QMap<QString, QStandardItemModel*> WT_DataWidget::getAllDataModels() const
{
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

void WT_DataWidget::updateButtonsState()
{
    bool hasSheet = (ui->tabWidget->count() > 0);
    ui->btnSave->setEnabled(hasSheet);
    ui->btnExport->setEnabled(hasSheet);
    ui->btnUnitManager->setEnabled(hasSheet);
    ui->btnTimeConvert->setEnabled(hasSheet);
    ui->btnPressureDropCalc->setEnabled(hasSheet);
    ui->btnCalcPwf->setEnabled(hasSheet);
    ui->btnFilterSample->setEnabled(hasSheet);
    ui->btnErrorCheck->setEnabled(hasSheet);

    if (auto sheet = currentSheet()) ui->filePathLabel->setText(sheet->getFilePath());
    else ui->filePathLabel->setText("未加载文件");
}

void WT_DataWidget::onOpenFile()
{
    QString filter = "所有支持文件 (*.csv *.txt *.xlsx *.xls);;Excel (*.xlsx *.xls);;CSV 文件 (*.csv);;文本文件 (*.txt);;所有文件 (*.*)";
    QStringList paths = QFileDialog::getOpenFileNames(this, "打开数据文件", "", filter);
    if (paths.isEmpty()) return;

    for (const QString& path : paths) {
        if (path.endsWith(".json", Qt::CaseInsensitive)) {
            loadData(path, "json");
            return;
        }

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
    if (fileType == "json") return;
    DataImportDialog dlg(filePath, this);
    applyDataDialogStyle(&dlg);
    if (dlg.exec() == QDialog::Accepted) {
        createNewTab(filePath, dlg.getSettings());
    }
}

void WT_DataWidget::onSave() {
    QJsonArray allData;
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

    } else {
        QVector<QStringList> fullTable;
        QStringList headers;

        for (int c = 0; c < colCount; ++c) {
            headers.append(model->horizontalHeaderItem(c) ? model->horizontalHeaderItem(c)->text() : "");
        }

        if (appendMode) {
            for (const auto& task : tasks) headers.append(task.newHeaderName);
        } else {
            for (const auto& task : tasks) headers[task.colIndex] = task.newHeaderName;
        }

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

void WT_DataWidget::onFilterSample()
{
    QStandardItemModel* model = getDataModel();
    if (!model || model->rowCount() == 0) {
        QMessageBox::warning(this, "提示", "当前无数据可处理！");
        return;
    }

    DataSamplingDialog dialog(model, this);
    if (dialog.exec() == QDialog::Accepted) {
        QVector<QStringList> processedTable = dialog.getProcessedTable();
        QStringList headers = dialog.getHeaders();
        if (processedTable.isEmpty()) {
            QMessageBox::warning(this, "错误", "处理后数据为空，请检查参数设置！");
            return;
        }
        QString currentPath = getCurrentFileName();
        saveAndLoadNewData(currentPath, headers, processedTable);
    }
}

void WT_DataWidget::saveAndLoadNewData(const QString& oldFilePath, const QStringList& headers, const QVector<QStringList>& fullTable)
{
    QFileInfo fi(oldFilePath);
    QString baseName = fi.completeBaseName();
    QString dir = fi.absolutePath();

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

void WT_DataWidget::onExportExcel() { if (auto s = currentSheet()) s->onExportExcel(); }
void WT_DataWidget::onTimeConvert() { if (auto s = currentSheet()) s->onTimeConvert(); }
void WT_DataWidget::onPressureDropCalc() { if (auto s = currentSheet()) s->onPressureDropCalc(); }
void WT_DataWidget::onCalcPwf() { if (auto s = currentSheet()) s->onCalcPwf(); }
void WT_DataWidget::onHighlightErrors() { if (auto s = currentSheet()) s->onHighlightErrors(); }

void WT_DataWidget::onTabChanged(int index) {
    Q_UNUSED(index);
    updateButtonsState();
    emit dataChanged();
}
void WT_DataWidget::onTabCloseRequested(int index) {
    QWidget* widget = ui->tabWidget->widget(index);
    if (widget) {
        ui->tabWidget->removeTab(index);
        delete widget;
    }
    updateButtonsState();
    emit dataChanged();
}
void WT_DataWidget::onSheetDataChanged() {
    if (sender() == currentSheet()) emit dataChanged();
}
