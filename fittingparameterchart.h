/*
 * 文件名: fittingparameterchart.h
 * 作用与功能:
 * 1. 拟合界面的参数管理控制类。
 * 2. 依据 ModelType 生成 108 种不同的反演属性表。
 * 3. 使用事件过滤器控制参数滚轮修饰功能。
 * 4. 彻底脱离硬编码，专职处理展现逻辑。
 */

#ifndef FITTINGPARAMETERCHART_H
#define FITTINGPARAMETERCHART_H

#include <QObject>
#include <QTableWidget>
#include <QList>
#include <QMap>
#include <QTimer>
#include "modelmanager.h"

/**
 * @brief 拟合参数数据结构体
 */
struct FitParameter {
    QString name;         // 参数英文键值
    QString displayName;  // 带希腊符号展示用名
    double value;         // 参数当前数值
    double min;           // 限制下限
    double max;           // 限制上限
    double step;          // 步长
    bool isFit;           // 决定高亮
    bool isVisible;       // 是否显示
};

class FittingParameterChart : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     */
    explicit FittingParameterChart(
        QTableWidget* parentTable,
        QObject *parent = nullptr);

    /**
     * @brief 关联后台大管家
     */
    void setModelManager(ModelManager* m);

    /**
     * @brief 提取覆盖当前表格
     */
    void resetParams(
        ModelManager::ModelType type,
        bool preserveStates = false);

    /**
     * @brief 继承切面
     */
    void switchModel(ModelManager::ModelType newType);

    /**
     * @brief 抽取列表
     */
    QList<FitParameter> getParameters() const;

    /**
     * @brief 回写列表
     */
    void setParameters(const QList<FitParameter>& params);

    /**
     * @brief 提取修改同步缓存
     */
    void updateParamsFromTable();

    /**
     * @brief 自动规整滚轮限幅
     */
    void autoAdjustLimits();

    /**
     * @brief 获取纯文本键值字典防截断
     */
    QMap<QString, QString> getRawParamTexts() const;

    /**
     * @brief 工具：提取带下角标名字
     */
    static void getParamDisplayInfo(
        const QString& name,
        QString& chName,
        QString& symbol,
        QString& uniSym,
        QString& unit);

    /**
     * @brief 工具：生成目标池
     */
    static QList<FitParameter> generateDefaultParams(
        ModelManager::ModelType type);

    /**
     * @brief 工具：修正界限
     */
    static void adjustLimits(
        QList<FitParameter>& params);

signals:
    /**
     * @brief 触动绘图重算信号
     */
    void parameterChangedByWheel();

protected:
    /**
     * @brief 抓取鼠标混轴操作
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QTableWidget* m_table;
    ModelManager* m_modelManager;
    QList<FitParameter> m_params;
    QTimer* m_wheelTimer;

    /**
     * @brief 重组覆盖表格
     */
    void refreshParamTable();

    /**
     * @brief 写行
     */
    void addRowToTable(
        const FitParameter& p,
        int& serialNo,
        bool highlight);

private slots:
    void onTableItemChanged(QTableWidgetItem* item);
    void onWheelDebounceTimeout();
};

#endif // FITTINGPARAMETERCHART_H
