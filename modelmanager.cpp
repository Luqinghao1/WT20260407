/*
 * 文件名: modelmanager.cpp
 * 作用与功能:
 * 1. 拦截并管理软件内全部 108 个试井模型界面的懒加载与生命周期堆栈。
 * 2. 负责分配和路由底层的数学求解任务，将特定模型的正演计算指令正确分发至对应的 Group1/2/3 求解器组件。
 * 3. 作为静态默认参数字典池的生成中枢，负责将全局基础物理参数与各特定模型专有的理论参数合并，统一下发并在此处管理专有参数的默认值。
 * 4. 统一下向所有挂载的底层数学求解模型广播和下发全局计算精度与算法设置（如 Stehfest 精度阶数等）。
 * 5. 处理观测历史数据的读写缓存管理，并在各个计算组件与 UI 界面间建立跨层级的计算完毕回调与信号槽透传。
 */

#include "modelmanager.h"
#include "modelselect.h"
#include "modelparameter.h"
#include "wt_modelwidget.h"
#include "modelsolver01.h"
#include "modelsolver02.h"
#include "modelsolver03.h"
#include <QVBoxLayout>

/**
 * @brief 构造函数：初始化管理器内部的基础组件指针状态
 * @param parent 遵循 Qt 对象树机制的父对象
 */
ModelManager::ModelManager(
    QWidget* parent) :
    QObject(parent),
    m_mainWidget(nullptr),
    m_modelStack(nullptr),
    m_currentModelType(Model_1) {}

/**
 * @brief 析构清理内存：遍历释放所有内部托管的 3 组底层数学求解器实体
 */
ModelManager::~ModelManager() {
    // 释放第 1 组数学求解器（处理 1-36号 模型）
    for(auto* s : m_solversGroup1)
        if(s) delete s;
    // 释放第 2 组数学求解器（处理 37-71号 模型）
    for(auto* s : m_solversGroup2)
        if(s) delete s;
    // 释放第 3 组数学求解器（处理 72-108号 模型）
    for(auto* s : m_solversGroup3)
        if(s) delete s;
}

/**
 * @brief 界面搭建：在父级窗口内布置 108 个模型的堆栈容器和初始化路由
 * @param parentWidget 宿主容器 UI 控件
 */
void ModelManager::initializeModels(
    QWidget* parentWidget)
{
    // 如果没有宿主窗口则直接返回
    if (!parentWidget) return;

    // 建立内部主布局容器
    createMainWidget();
    // 实例化一个堆栈窗口组件用于存放所有懒加载模型子页面
    m_modelStack = new QStackedWidget(m_mainWidget);

    // 为 108 个模型分别预留界面和数学求解器的指针空间数组
    m_modelWidgets.resize(108);
    m_modelWidgets.fill(nullptr);
    m_solversGroup1.resize(36);
    m_solversGroup1.fill(nullptr);
    m_solversGroup2.resize(36);
    m_solversGroup2.fill(nullptr);
    m_solversGroup3.resize(36);
    m_solversGroup3.fill(nullptr);

    // 将堆栈容器推入主布局
    m_mainWidget->layout()->addWidget(m_modelStack);

    // 默认切换并显示第一号模型（Model_1）
    switchToModel(Model_1);

    // 将组装好的主控件挂载到外部父窗口中
    if (parentWidget->layout()) {
        parentWidget->layout()->addWidget(m_mainWidget);
    } else {
        QVBoxLayout* lay = new QVBoxLayout(parentWidget);
        lay->addWidget(m_mainWidget);
        parentWidget->setLayout(lay);
    }
}

/**
 * @brief 构建内部的无边距主宿主小部件
 */
void ModelManager::createMainWidget() {
    m_mainWidget = new QWidget();
    QVBoxLayout* ml = new QVBoxLayout(m_mainWidget);
    // 取消所有布局留白，让子界面填满
    ml->setContentsMargins(0,0,0,0);
    ml->setSpacing(0);
    m_mainWidget->setLayout(ml);
}

/**
 * @brief 懒加载界面策略：当某模型首次被切换时才申请创建界面的内存开销
 * @param type 需要获取的模型枚举编号
 * @return 对应的界面控件指针
 */
WT_ModelWidget* ModelManager::
    ensureWidget(ModelType type)
{
    int i = (int)type;
    // 安全校验：超出模型总数则返回空
    if (i < 0 || i >= 108)
        return nullptr;

    // 判断该位置的界面指针是否为空，空则代表首次进入
    if (!m_modelWidgets[i]) {
        // 动态实例化该模型的专属图版控件
        WT_ModelWidget* w = new WT_ModelWidget(type, m_modelStack);
        m_modelWidgets[i] = w;
        // 添加至堆栈控件系统
        m_modelStack->addWidget(w);

        // 绑定该模型界面弹出的“选择模型”对话框请求回调
        connect(w,
                &WT_ModelWidget::requestModelSelection,
                this,
                &ModelManager::onSelectModelClicked);

        // 绑定该模型界面正演计算完成后的信号透传
        connect(w,
                &WT_ModelWidget::calculationCompleted,
                this,
                &ModelManager::onWidgetCalculationCompleted);
    }
    return m_modelWidgets[i];
}

/**
 * @brief 懒加载生成并返回第 1 组（1-36）模型的底层数学求解引擎
 */
ModelSolver01* ModelManager::
    ensureSolverGroup1(int idx) {
    if (!m_solversGroup1[idx])
        m_solversGroup1[idx] = new ModelSolver01((ModelSolver01::ModelType)idx);
    return m_solversGroup1[idx];
}

/**
 * @brief 懒加载生成并返回第 2 组（37-71）模型的底层数学求解引擎
 */
ModelSolver02* ModelManager::
    ensureSolverGroup2(int idx) {
    if (!m_solversGroup2[idx])
        m_solversGroup2[idx] = new ModelSolver02((ModelSolver02::ModelType)idx);
    return m_solversGroup2[idx];
}

/**
 * @brief 懒加载生成并返回第 3 组（72-108）模型的底层数学求解引擎
 */
ModelSolver03* ModelManager::
    ensureSolverGroup3(int idx) {
    if (!m_solversGroup3[idx])
        m_solversGroup3[idx] = new ModelSolver03((ModelSolver03::ModelType)idx);
    return m_solversGroup3[idx];
}

/**
 * @brief 界面切换并实现参数记忆接力（实现平滑过渡）
 * @param modelType 要切换的目标模型编号
 */
void ModelManager::switchToModel(
    ModelType modelType)
{
    if (!m_modelStack) return;
    ModelType old = m_currentModelType;

    // 抓取并缓存当前旧界面上用户已填写的参数数值
    QMap<QString, QString> curT;
    if (m_modelWidgets[old])
        curT = m_modelWidgets[old]->getUiTexts();

    // 更新当前的内部路由状态
    m_currentModelType = modelType;

    // 获取新目标模型的界面实例（触发懒加载机制）
    WT_ModelWidget* w = ensureWidget(modelType);

    if (w) {
        // 如果旧界面有数据残留，则覆盖进新界面，实现记忆接力
        if (!curT.isEmpty())
            w->setUiTexts(curT);
        // 让 UI 堆栈显示新目标界面
        m_modelStack->setCurrentWidget(w);
    }
    // 广播模型切换成功信号
    emit modelSwitched(modelType, old);
}

/**
 * @brief 核心计算路由分发：根据模型类型分发到对应的 3 类解析器组并获取结果
 */
ModelCurveData ModelManager::
    calculateTheoreticalCurve(
        ModelType type,
        const QMap<QString, double>& p,
        const QVector<double>& t)
{
    int id = (int)type;
    // 路由分拣：前 36 个打入 Group 1
    if (id <= 35)
        return ensureSolverGroup1(id)->calculateTheoreticalCurve(p, t);
    // 路由分拣：中 36 个打入 Group 2，并将编号降级归一
    else if (id <= 71)
        return ensureSolverGroup2(id-36)->calculateTheoreticalCurve(p, t);
    // 路由分拣：后 36 个打入 Group 3，并将编号降级归一
    else
        return ensureSolverGroup3(id-72)->calculateTheoreticalCurve(p, t);
}

/**
 * @brief 获取特定模型标号所对应的描述标题
 */
QString ModelManager::
    getModelTypeName(ModelType type) {
    int id = (int)type;
    if (id <= 35)
        return ModelSolver01::getModelName((ModelSolver01::ModelType)id);
    else if (id <= 71)
        return ModelSolver02::getModelName((ModelSolver02::ModelType)(id - 36));
    else
        return ModelSolver03::getModelName((ModelSolver03::ModelType)(id - 72));
}

/**
 * @brief 响应特定界面弹出的“选择模型”交互对话框行为
 */
void ModelManager::
    onSelectModelClicked() {
    // 实例化模型选择 UI 弹窗
    ModelSelect dlg(m_mainWidget);

    // 设置进入弹窗时，左侧树状目录默认展开并选定当前对应的模型
    dlg.setCurrentModelCode(QString("modelwidget%1").arg((int)m_currentModelType + 1));

    // 如果用户点击了弹窗的确认按钮
    if (dlg.exec() == QDialog::Accepted) {
        // 获取用户选择的编码结果
        QString numS = dlg.getSelectedModelCode();
        // 剔除前后缀，剥离纯数字ID
        numS.remove("modelwidget");
        int mId = numS.toInt();
        // 发起跨界面路由切换请求
        if (mId >= 1 && mId <= 108)
            switchToModel((ModelType)(mId - 1));
    }
}

/**
 * @brief 中枢配置生成池：将全局物理参数与模型专有理论参数进行统一装配并发放
 * @param type 当前所处的模型枚举（用于判断所需的特定理论结构）
 * @return 组装完成的全量参数字典包
 */
QMap<QString, double> ModelManager::
    getDefaultParameters(ModelType type)
{
    // 声明最终下发使用的键值对映射字典
    QMap<QString, double> p;
    // 获取系统的第一级参数单例指针
    ModelParameter* mp = ModelParameter::instance();

    // 第1步：从单例中提取共享的基础客观物理属性并插入字典
    p.insert("phi", mp->getPhi());       // 插入孔隙度
    p.insert("h", mp->getH());           // 插入有效厚度
    p.insert("rw", mp->getRw());         // 插入井筒半径
    p.insert("mu", mp->getMu());         // 插入粘度
    p.insert("B", mp->getB());           // 插入体积系数
    p.insert("Ct", mp->getCt());         // 插入综合压缩系数
    p.insert("q", mp->getQ());           // 插入产量
    p.insert("L", mp->getL());           // 插入水平井长
    p.insert("nf", mp->getNf());         // 插入裂缝条数
    p.insert("alpha", mp->getAlpha());   // 插入变井储时间参数
    p.insert("C_phi", mp->getCPhi());    // 插入变井储压力参数

    // ========================================================================
    // 【修改默认值指引】: 若需修改试井模型的各分支专有理论参数，请在此处修改对应的默认值
    // ========================================================================

    // 第2步：插入各模型公用的通用理论计算参数默认值
    p.insert("kf", 10.0);       // 修改裂缝渗透率默认值
    p.insert("M12", 5.0);       // 修改内外区流度比默认值
    p.insert("Lf", 50.0);       // 修改裂缝半长默认值
    p.insert("rm", 50000.0);     // 修改内区复合半径默认值
    p.insert("gamaD", 0.006);   // 修改压敏系数默认值

    int t = (int)type;
    // 判断是否属于后半部分带有三孔混合特征的模型
    bool isMix = (t >= 72 && t <= 107);
    // 解析具体边界类型分类（每 12 个一循环）
    int gI = t % 12;

    // 如果属于非无穷大边界（封闭或定压），额外赋予外部边界半径预设值
    if (gI >= 4) {
        p.insert("re", isMix ? 500000.0 : 20000.0); // 混合模型给予 50万，否则 2万
    }

    // 第3步：针对复杂介质（双重介质 / 三重介质）赋予特定默认参数
    if (isMix) {
        // 三孔模型专属特征参数
        int sub3 = t - 72;
        p.insert("omega_f1", 0.02);    // 第一缝网区储容比
        p.insert("omega_v1", 0.01);    // 溶洞区储容比
        p.insert("lambda_m1", 4e-4);   // 介质1向裂缝窜流系数
        p.insert("lambda_v1", 1e-4);   // 溶洞向裂缝窜流系数

        // 若为内三孔+外双孔等复杂搭配模型
        if (sub3 < 24) {
            p.insert("omega_f2", 0.008);   // 外区裂缝储容比
            p.insert("lambda_m2", 1e-7);   // 外区窜流系数
        }
    } else {
        // 双孔模型专属特征参数
        p.insert("omega1", 0.1);       // 内部区储容比默认值
        p.insert("omega2", 0.001);      // 外部区储容比默认值
        p.insert("lambda1", 2e-3);     // 内部区窜流系数默认值
        p.insert("lambda2", 1e-3);     // 外部区窜流系数默认值
    }

    // 第4步：针对不同的井筒储集效应模型进行参数赋值分配
    int sT = t % 4;
    // 排除纯线源解模型（无井储效应），为其他模型分配常规井筒系数
    if (sT != 1) {
        p.insert("C", 10.0);          // 默认无因次井筒储集系数
        p.insert("S", 1.0);           // 默认表皮系数
    }

    // 第5步：插入绘图辅助生成信息参数
    p.insert("t", 1e10);       // 修改默认计算终点时长
    p.insert("points", 100.0); // 修改默认散点曲线插值点密度

    return p; // 输出装配完整的字典池
}

/**
 * @brief 精度穿透广播：将顶层的计算精度需求设置遍历同步下发至全部托管实体
 */
void ModelManager::
    setHighPrecision(bool h) {
    // 通知已创建的各种界面
    for(WT_ModelWidget* w : m_modelWidgets)
        if(w) w->setHighPrecision(h);

    // 强制通知所有 3 组的数学求解器采用高精度模式
    for(ModelSolver01* s : m_solversGroup1)
        if(s) s->setHighPrecision(h);

    for(ModelSolver02* s : m_solversGroup2)
        if(s) s->setHighPrecision(h);

    for(ModelSolver03* s : m_solversGroup3)
        if(s) s->setHighPrecision(h);
}

/**
 * @brief 刷新全部界面的基础参数展示（触发 UI 执行 onResetParameters 的重置信号）
 */
void ModelManager::
    updateAllModelsBasicParameters() {
    for(WT_ModelWidget* w : m_modelWidgets)
        // 使用元系统安全调用底层刷新响应，避免指针失效引发崩溃
        if(w) QMetaObject::invokeMethod(w, "onResetParameters");
}

/**
 * @brief 存储外部导入观测数据至此内存缓存（时间、压力、导数向量）
 */
void ModelManager::setObservedData(
    const QVector<double>& t,
    const QVector<double>& p,
    const QVector<double>& d)
{
    m_cachedObsTime = t;
    m_cachedObsPressure = p;
    m_cachedObsDeriv = d;
}

/**
 * @brief 提供外部接口获取已缓存的历史观测数据
 */
void ModelManager::getObservedData(
    QVector<double>& t,
    QVector<double>& p,
    QVector<double>& d) const
{
    t = m_cachedObsTime;
    p = m_cachedObsPressure;
    d = m_cachedObsDeriv;
}

/**
 * @brief 清空内存中的所有观测历史数据缓存
 */
void ModelManager::clearCache() {
    m_cachedObsTime.clear();
    m_cachedObsPressure.clear();
    m_cachedObsDeriv.clear();
}

/**
 * @brief 校验并返回内存中当前是否存在有效的观测点集
 */
bool ModelManager::hasObservedData()
    const {
    return !m_cachedObsTime.isEmpty();
}

/**
 * @brief 底层模型界面计算完成回调向上的透传响应槽位
 */
void ModelManager::
    onWidgetCalculationCompleted(
        const QString &t,
        const QMap<QString, double> &r)
{
    // 将完成事件进一步向上级容器广播
    emit calculationCompleted(t, r);
}

/**
 * @brief 提供生成对数级均匀分布时间离散序列的跨类静态支持工具
 */
QVector<double> ModelManager::
    generateLogTimeSteps(
        int c, double s, double e)
{
    // 借用底层的算法模块逻辑进行序列构建
    return ModelSolver01::generateLogTimeSteps(c, s, e);
}
