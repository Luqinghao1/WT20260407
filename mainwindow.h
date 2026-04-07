/*
 * 文件名: mainwindow.h
 * 文件作用: 主窗口类头文件
 * 功能描述:
 * 1. 声明主窗口框架及各个子功能模块指针。
 * 2. 定义主窗口与各个子模块之间的交互接口。
 * 3. 【新增】重写了 closeEvent 拦截窗口关闭事件，实现了统一的全局退出确认弹窗。
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include <QStandardItemModel>
#include <QCloseEvent>
#include "modelmanager.h"

// 前置声明各个功能页面的类
class NavBtn;
class WT_ProjectWidget;
class WT_DataWidget;
class WT_PlottingWidget;
class FittingPage;
class SettingsWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 初始化主程序逻辑
    void init();

    // 各功能模块界面初始化函数
    void initProjectForm();
    void initDataEditorForm();
    void initModelForm();
    void initPlottingForm();
    void initFittingForm();
    void initPredictionForm();

protected:
    /**
     * @brief 拦截主界面的全局关闭事件，弹出保存并退出/直接退出/取消确认框
     * @param event 关闭事件句柄
     */
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onProjectOpened(bool isNew);
    void onProjectClosed();
    void onFileLoaded(const QString& filePath, const QString& fileType);
    void onPlotAnalysisCompleted(const QString &analysisType, const QMap<QString, double> &results);
    void onDataReadyForPlotting();
    void onTransferDataToPlotting();
    void onDataEditorDataChanged();
    void onViewExportedFile(const QString& filePath);
    void onSystemSettingsChanged();
    void onPerformanceSettingsChanged();
    void onModelCalculationCompleted(const QString &analysisType, const QMap<QString, double> &results);
    void onFittingProgressChanged(int progress);

private:
    Ui::MainWindow *ui;

    WT_ProjectWidget* m_ProjectWidget;
    WT_DataWidget* m_DataEditorWidget;
    ModelManager* m_ModelManager;
    WT_PlottingWidget* m_PlottingWidget;
    FittingPage* m_FittingPage;
    SettingsWidget* m_SettingsWidget;

    QMap<QString, NavBtn*> m_NavBtnMap;
    QTimer m_timer;
    bool m_hasValidData = false;
    bool m_isProjectLoaded = false;

    void transferDataFromEditorToPlotting();
    void updateNavigationState();
    void transferDataToFitting();
    QStandardItemModel* getDataEditorModel() const;
    QString getCurrentFileName() const;
    bool hasDataLoaded();
    QString getMessageBoxStyle() const;
};

#endif // MAINWINDOW_H
