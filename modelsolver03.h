/*
 * 文件名: modelsolver03.h
 * 作用:
 * 1. 混积型复合模型核心头文件。
 * 2. 覆盖 Model 73-108 的运算。
 * 3. 声明三孔内区与各外区的解析。
 */

#ifndef ModelSolver03_H
#define ModelSolver03_H

#include <QMap>
#include <QVector>
#include <QString>
#include <tuple>
#include <functional>
#include <QtConcurrent>

using ModelCurveData = std::tuple<
    QVector<double>,
    QVector<double>,
    QVector<double>>;

class ModelSolver03
{
public:
    enum ModelType {
        Model_1 = 0, Model_2,
        Model_3, Model_4,
        Model_5, Model_6,
        Model_7, Model_8,
        Model_9, Model_10,
        Model_11, Model_12,
        Model_13, Model_14,
        Model_15, Model_16,
        Model_17, Model_18,
        Model_19, Model_20,
        Model_21, Model_22,
        Model_23, Model_24,
        Model_25, Model_26,
        Model_27, Model_28,
        Model_29, Model_30,
        Model_31, Model_32,
        Model_33, Model_34,
        Model_35, Model_36
    };

    /**
     * @brief 构造函数
     */
    explicit ModelSolver03(
        ModelType type);

    /**
     * @brief 析构函数
     */
    virtual ~ModelSolver03();

    /**
     * @brief 设置精度
     */
    void setHighPrecision(bool h);

    /**
     * @brief 图版计算主函数
     */
    ModelCurveData
    calculateTheoreticalCurve(
        const QMap<QString, double>& p,
        const QVector<double>& t =
        QVector<double>());

    /**
     * @brief 取模型名
     */
    static QString getModelName(
        ModelType type,
        bool v = true);

    /**
     * @brief 时间轴生成
     */
    static QVector<double>
    generateLogTimeSteps(
        int c, double s, double e);

private:
    /**
     * @brief 数值反演与压敏外推
     */
    void calculatePDandDeriv(
        const QVector<double>& tD,
        const QMap<QString, double>& p,
        std::function<double(
            double,
            const QMap<QString,
                       double>&)> f,
        QVector<double>& oPD,
        QVector<double>& oDeriv);

    /**
     * @brief Laplace域耦合解
     */
    double flaplace_composite(
        double z,
        const QMap<QString, double>& p);

    /**
     * @brief 面源积分方程求解
     */
    double PWD_composite(
        double z, double fs1,
        double fs2, double M12,
        double LfD, double rmD,
        double reD, int nF,
        ModelType type);

    // 辅助防溢出方法
    static double safe_bessel_i_sc(
        int v, double x);
    static double safe_bessel_k_sc(
        int v, double x);
    static double safe_bessel_k(
        int v, double x);

    // 页岩传导函数
    double calc_fs_shale(
        double u, double o, double l);

    double getStehfestCoeff(
        int i, int N);
    void precomputeStehfestCoeffs(
        int N);
    double factorial(int n);

private:
    ModelType m_type;
    bool m_highPrecision;
    QVector<double> m_stehfestCoeffs;
    int m_currentN;
};

#endif // ModelSolver03_H
