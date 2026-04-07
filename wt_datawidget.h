/*
 * 文件名: wt_datawidget.h
 * 文件作用: 数据编辑器主窗口头文件
 * 功能描述:
 * 1. 定义数据编辑器的主界面类 WT_DataWidget，作为数据模块的顶层容器。
 * 2. 负责管理多个 DataSingleSheet (单表实例)，并以多页签(TabWidget)形式展示。
 * 3. 协调顶部工具栏中的9个功能项与当前活动表数据的交互。
 * 4. 【新增】声明代码原生图标绘制函数，彻底摆脱外部图片文件的依赖。
 */

#ifndef WT_DATAWIDGET_H
#define WT_DATAWIDGET_H

#include <QWidget>
#include <QStandardItemModel>
#include <QJsonArray>
#include <QMap>
#include <QVector>
#include <QStringList>
#include <QIcon>
#include "datasinglesheet.h"

namespace Ui {
class WT_DataWidget;
}

class WT_DataWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WT_DataWidget(QWidget *parent = nullptr);
    ~WT_DataWidget();

    // 清空所有表格数据
    void clearAllData();
    // 从持久化项目文件恢复数据面板
    void loadFromProjectData();
    // 提取当前高亮页签的数据模型对象
    QStandardItemModel* getDataModel() const;
    // 获取系统中加载的所有文件的数据模型对象集合
    QMap<QString, QStandardItemModel*> getAllDataModels() const;
    // 外部调用：传入路径以加载新数据进入独立页签
    void loadData(const QString& filePath, const QString& fileType = "auto");
    // 获取当前活动页签对应的文件绝对路径
    QString getCurrentFileName() const;
    // 检查目前编辑器是否含有合法数据
    bool hasData() const;

signals:
    // 当内部表格数据被实质性修改时发出该信号
    void dataChanged();
    // 当有新文件加载或移除时发出该信号
    void fileChanged(const QString& filePath, const QString& fileType);

private slots:
    // 以下为9个数据功能按钮的触发槽函数
    void onOpenFile();          // 1.打开数据
    void onSave();              // 2.同步保存
    void onExportExcel();       // 3.导出Excel
    void onUnitManager();       // 4.单位管理
    void onTimeConvert();       // 5.时间转换
    void onPressureDropCalc();  // 6.计算压降
    void onCalcPwf();           // 7.套压转流压
    void onFilterSample();      // 8.滤波取样
    void onHighlightErrors();   // 9.异常检查

    // 页签发生切换或要求关闭时的回调
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    // 代理子组件发出的数据变动信号
    void onSheetDataChanged();

private:
    Ui::WT_DataWidget *ui;

    // 初始化界面控件逻辑，包含设置图标排版
    void initUI();
    // 绑定信号和槽函数
    void setupConnections();
    // 动态更新9个按钮的可点击状态(无数据时置灰)
    void updateButtonsState();

    // 根据外部设定的属性新建一个带有数据的独立页签
    void createNewTab(const QString& filePath, const DataImportSettings& settings);
    // 安全获取当前正在前台展示的数据表实例
    DataSingleSheet* currentSheet() const;

    // 将完整的全表数据连同表头一并写入底层文件
    void saveAndLoadNewData(const QString& oldFilePath, const QStringList& headers, const QVector<QStringList>& fullTable);

    // 【新增】程序化绘制图标的方法
    QIcon createCustomIcon(const QString& iconType);
};

#endif // WT_DATAWIDGET_H
