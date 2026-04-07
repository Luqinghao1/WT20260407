/*
 * 文件名: modelmanager.h
 * 作用:
 * 1. 试井模型管理中枢头文件。
 * 2. 管理 108 个模型实例创建与显隐。
 * 3. 全局默认参数统一下发。
 */

#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QStackedWidget>
#include "wt_modelwidget.h"

class ModelSolver01;
class ModelSolver02;
class ModelSolver03;

using ModelCurveData = std::tuple<
    QVector<double>,
    QVector<double>,
    QVector<double>>;

class ModelManager : public QObject
{
    Q_OBJECT

public:
    enum ModelType {
        Model_1=0, Model_2, Model_3,
        Model_4, Model_5, Model_6,
        Model_7, Model_8, Model_9,
        Model_10, Model_11, Model_12,
        Model_13, Model_14, Model_15,
        Model_16, Model_17, Model_18,
        Model_19, Model_20, Model_21,
        Model_22, Model_23, Model_24,
        Model_25, Model_26, Model_27,
        Model_28, Model_29, Model_30,
        Model_31, Model_32, Model_33,
        Model_34, Model_35, Model_36,
        Model_37, Model_38, Model_39,
        Model_40, Model_41, Model_42,
        Model_43, Model_44, Model_45,
        Model_46, Model_47, Model_48,
        Model_49, Model_50, Model_51,
        Model_52, Model_53, Model_54,
        Model_55, Model_56, Model_57,
        Model_58, Model_59, Model_60,
        Model_61, Model_62, Model_63,
        Model_64, Model_65, Model_66,
        Model_67, Model_68, Model_69,
        Model_70, Model_71, Model_72,
        Model_73, Model_74, Model_75,
        Model_76, Model_77, Model_78,
        Model_79, Model_80, Model_81,
        Model_82, Model_83, Model_84,
        Model_85, Model_86, Model_87,
        Model_88, Model_89, Model_90,
        Model_91, Model_92, Model_93,
        Model_94, Model_95, Model_96,
        Model_97, Model_98, Model_99,
        Model_100, Model_101, Model_102,
        Model_103, Model_104, Model_105,
        Model_106, Model_107, Model_108
    };

    /**
     * @brief 构造函数
     */
    explicit ModelManager(
        QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ModelManager();

    /**
     * @brief 挂载至UI
     */
    void initializeModels(
        QWidget* parentWidget);

    /**
     * @brief 切换显示模型
     */
    void switchToModel(
        ModelType modelType);

    /**
     * @brief 取当前模型标号
     */
    ModelType getCurrentModelType()
        const {
        return m_currentModelType;
    }

    /**
     * @brief 获取理论曲线计算结果
     */
    ModelCurveData
    calculateTheoreticalCurve(
        ModelType type,
        const QMap<QString, double>& p,
        const QVector<double>& t =
        QVector<double>());

    /**
     * @brief 中枢参数合并分发
     */
    static QMap<QString, double>
    getDefaultParameters(
        ModelType type);

    /**
     * @brief 文本描述抓取
     */
    static QString getModelTypeName(
        ModelType type);

    /**
     * @brief 统一下发计算精度
     */
    void setHighPrecision(bool high);

    /**
     * @brief 同步界面数据展示
     */
    void
    updateAllModelsBasicParameters();

    // 观测数据管理
    void setObservedData(
        const QVector<double>& t,
        const QVector<double>& p,
        const QVector<double>& d);
    void getObservedData(
        QVector<double>& t,
        QVector<double>& p,
        QVector<double>& d) const;
    void clearCache();
    bool hasObservedData() const;

    // 工具支持
    static QVector<double>
    generateLogTimeSteps(
        int c, double s, double e);

signals:
    /**
     * @brief 界面切换完成通报
     */
    void modelSwitched(
        ModelType newM, ModelType oldM);

    /**
     * @brief 计算完报数据穿透
     */
    void calculationCompleted(
        const QString& t,
        const QMap<QString, double>& r);

public slots:
    void onSelectModelClicked();
    void onWidgetCalculationCompleted(
        const QString& t,
        const QMap<QString, double>& r);

private:
    void createMainWidget();
    WT_ModelWidget* ensureWidget(
        ModelType type);
    ModelSolver01* ensureSolverGroup1(
        int index);
    ModelSolver02* ensureSolverGroup2(
        int index);
    ModelSolver03* ensureSolverGroup3(
        int index);

private:
    QWidget* m_mainWidget;
    QStackedWidget* m_modelStack;

    QVector<WT_ModelWidget*>
        m_modelWidgets;
    QVector<ModelSolver01*>
        m_solversGroup1;
    QVector<ModelSolver02*>
        m_solversGroup2;
    QVector<ModelSolver03*>
        m_solversGroup3;

    ModelType m_currentModelType;
    QVector<double> m_cachedObsTime;
    QVector<double> m_cachedObsPressure;
    QVector<double> m_cachedObsDeriv;
};

#endif // MODELMANAGER_H
