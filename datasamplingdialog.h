/*
 * 文件名: datasamplingdialog.h
 * 作用: 数据滤波与取样悬浮窗口头文件
 * 更新: 新增了对左侧边缘拖拽缩放的鼠标事件支持，以及绘制标题栏图标的支持。
 */

#ifndef DATASAMPLINGDIALOG_H
#define DATASAMPLINGDIALOG_H

#include <QWidget>
#include <QVector>
#include <QStringList>
#include <QStandardItemModel>
#include <QMouseEvent>
#include <QIcon>
#include "qcustomplot.h"

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;

class DataSamplingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DataSamplingWidget(QWidget *parent = nullptr);
    ~DataSamplingWidget();

    void setModel(QStandardItemModel* model);

    QVector<QStringList> getProcessedTable() const;
    QStringList getHeaders() const;

signals:
    void requestMaximize(); // 请求最大化
    void requestRestore();  // 请求恢复默认对齐大小
    void requestClose();    // 请求关闭
    void requestResize(int dx); // 【新增】请求主界面配合进行水平宽度拉伸
    void processingFinished(const QStringList& headers, const QVector<QStringList>& processedTable);

protected:
    // 【新增】重载鼠标事件以支持悬浮窗边缘的手动拖拽拉伸
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onPreviewClicked();
    void onResetClicked();
    void onDataColumnChanged();
    void onAxisScaleChanged();
    void onSplitBoxValueChanged();

    void onRawPlotMousePress(QMouseEvent *event);
    void onRawPlotMouseMove(QMouseEvent *event);
    void onRawPlotMouseRelease(QMouseEvent *event);

private:
    void initUI();
    void setupConnections();
    void applyStyle();

    // 【新增】纯代码绘制控制台矢量图标
    QIcon createCtrlIcon(int type);

    void loadRawData();
    void updateSplitLines();
    void drawRawPlot();
    void drawProcessedPlot();
    void processData();

    void applyMedianFilter(QVector<double>& y, int windowSize);
    QVector<int> getLogSamplingIndices(const QVector<double>& x, int pointsPerDecade);
    QVector<int> getIntervalSamplingIndices(int totalCount, int interval);
    QVector<int> getTotalPointsSamplingIndices(int totalCount, int targetPoints);

    QStandardItemModel* m_model;
    QStringList m_headers;
    QVector<QStringList> m_rawTable;
    QVector<QStringList> m_processedTable;

    QVector<double> m_rawX, m_rawY;
    QVector<double> m_processedX, m_processedY;
    QString m_xName, m_yName;

    QComboBox* comboX;
    QComboBox* comboY;
    QCheckBox* chkLogX;
    QCheckBox* chkLogY;
    QDoubleSpinBox* spinMinY;
    QDoubleSpinBox* spinMaxY;
    QDoubleSpinBox* spinSplit1;
    QDoubleSpinBox* spinSplit2;

    struct StageUI {
        QComboBox* comboFilter;
        QSpinBox* spinFilterWin;
        QComboBox* comboSample;
        QSpinBox* spinSampleVal;
    };
    StageUI m_stages[3];

    QPushButton* btnPreview;
    QPushButton* btnReset;
    QPushButton* btnOk;
    QPushButton* btnCancel;

    QPushButton* btnMaximizeWindow;
    QPushButton* btnRestoreWindow;
    QPushButton* btnCloseWindow;

    QCustomPlot* customPlotRaw;
    QCustomPlot* customPlotProcessed;
    QCPItemStraightLine *vLine1, *vLine2;
    QCPItemStraightLine *vLine1_proc, *vLine2_proc;

    int m_dragLineIndex;

    // 窗口拉伸的状态标记
    bool m_isResizing;
    int m_dragStartX;
};

#endif // DATASAMPLINGDIALOG_H
