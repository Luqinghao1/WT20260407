/*
 * 文件名: fittingparameterchart.cpp
 * 作用与功能:
 * 1. 【架构优化】利用 ModelManager 提取包含第一级和第二级的中枢字典。
 * 2. 【视觉优化】赋予全面的希腊语与 Unicode 角标展示。
 * 3. 实现边界封锁与异常防抖过滤操作。
 */

#include "fittingparameterchart.h"
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QDebug>
#include <QBrush>
#include <QColor>
#include <QRegularExpression>
#include <QWheelEvent>
#include <cmath>

#include "modelsolver01.h"

/**
 * @brief 初始化绑定
 */
FittingParameterChart::FittingParameterChart(
    QTableWidget *parentTable, QObject *parent)
    : QObject(parent), m_table(parentTable), m_modelManager(nullptr)
{
    m_wheelTimer = new QTimer(this);
    m_wheelTimer->setSingleShot(true);
    m_wheelTimer->setInterval(200);
    connect(m_wheelTimer, &QTimer::timeout,
            this, &FittingParameterChart::onWheelDebounceTimeout);

    if(m_table) {
        QStringList headers;
        headers << "序号" << "参数名称" << "数值" << "单位";
        m_table->setColumnCount(headers.size());
        m_table->setHorizontalHeaderLabels(headers);

        m_table->horizontalHeader()->setStyleSheet(
            "QHeaderView::section { background-color: #E0E0E0; "
            "color: black; font-weight: bold; border: 1px solid #A0A0A0; }");

        m_table->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Interactive);
        m_table->horizontalHeader()->setStretchLastSection(true);

        m_table->setColumnWidth(0, 40);
        m_table->setColumnWidth(1, 160);
        m_table->setColumnWidth(2, 80);

        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setAlternatingRowColors(false);
        m_table->verticalHeader()->setVisible(false);

        m_table->viewport()->installEventFilter(this);
        connect(m_table, &QTableWidget::itemChanged,
                this, &FittingParameterChart::onTableItemChanged);
    }
}

/**
 * @brief 限制与安全拦截
 */
bool FittingParameterChart::eventFilter(QObject *w, QEvent *e) {
    if (w == m_table->viewport() && e->type() == QEvent::Wheel) {
        QWheelEvent *we = static_cast<QWheelEvent*>(e);
        QTableWidgetItem *item = m_table->itemAt(we->position().toPoint());
        if (item && item->column() == 2) {
            QTableWidgetItem *kItem = m_table->item(item->row(), 1);
            if (!kItem) return false;
            QString pName = kItem->data(Qt::UserRole).toString();

            if (pName == "LfD") return true;

            for (auto &p : m_params) {
                if (p.name == pName) {
                    QString txt = item->text();
                    if (txt.contains(',') || txt.contains(QChar(0xFF0C)))
                        return false;

                    bool ok; double cur = txt.toDouble(&ok);
                    if (ok) {
                        double nV = cur + (we->angleDelta().y() / 120) * p.step;
                        if (p.name == "nf")
                            nV = qMax(1.0, std::round(nV));
                        if (p.max > p.min)
                            nV = qBound(p.min, nV, p.max);

                        item->setText(
                            p.name == "nf" ?
                                QString::number(nV) :
                                QString::number(nV, 'g', 6));

                        p.value = nV;
                        m_wheelTimer->start();
                        return true;
                    }
                }
            }
        }
    }
    return QObject::eventFilter(w, e);
}

void FittingParameterChart::onWheelDebounceTimeout() {
    emit parameterChangedByWheel();
}

/**
 * @brief 表格改动拦截推导 LfD
 */
void FittingParameterChart::onTableItemChanged(QTableWidgetItem *item) {
    if (!item || item->column() != 2) return;
    QTableWidgetItem *kItem = m_table->item(item->row(), 1);
    if (!kItem) return;
    QString key = kItem->data(Qt::UserRole).toString();

    if (key == "L" || key == "Lf") {
        double valL = 0.0, valLf = 0.0;
        QTableWidgetItem* iLfD = nullptr;
        for(int i = 0; i < m_table->rowCount(); ++i) {
            QTableWidgetItem* k = m_table->item(i, 1);
            QTableWidgetItem* v = m_table->item(i, 2);
            if(k && v) {
                QString cK = k->data(Qt::UserRole).toString();
                if (cK == "L") valL = v->text().toDouble();
                else if (cK == "Lf") valLf = v->text().toDouble();
                else if (cK == "LfD") iLfD = v;
            }
        }
        if (valL > 1e-9 && iLfD) {
            double nLfD = valLf / valL;
            m_table->blockSignals(true);
            iLfD->setText(QString::number(nLfD, 'g', 6));
            m_table->blockSignals(false);
            for(auto& p : m_params) {
                if(p.name == "LfD") { p.value = nLfD; break; }
            }
        }
        if (!m_wheelTimer->isActive()) m_wheelTimer->start();
    }
}

void FittingParameterChart::setModelManager(ModelManager *m) {
    m_modelManager = m;
}

/**
 * @brief 【中心生成点】
 */
QList<FitParameter> FittingParameterChart::generateDefaultParams(
    ModelManager::ModelType type)
{
    QList<FitParameter> params;
    QMap<QString, double> defs = ModelManager::getDefaultParameters(type);

    auto addParam = [&](QString name, bool isFitDefault) {
        if (!defs.contains(name)) return;
        FitParameter p;
        p.name = name;
        p.value = defs.value(name);
        p.isFit = isFitDefault;
        p.isVisible = true;
        p.min = p.max = p.step = 0;

        QString dummy;
        getParamDisplayInfo(p.name, p.displayName, dummy, dummy, dummy);
        params.append(p);
    };

    addParam("phi", false); addParam("h", false); addParam("rw", false);
    addParam("mu", false); addParam("B", false); addParam("Ct", false);
    addParam("q", false);

    addParam("kf", true); addParam("M12", true); addParam("L", true);
    addParam("Lf", true); addParam("nf", true); addParam("rm", true);

    if (defs.contains("re")) addParam("re", true);

    int t = static_cast<int>(type);
    if (t >= 72 && t <= 107) {
        addParam("omega_f1", true); addParam("omega_v1", true);
        addParam("lambda_m1", true); addParam("lambda_v1", true);
        if (t - 72 < 24) {
            addParam("omega_f2", true); addParam("lambda_m2", true);
        }
    } else if (!(t >= (int)ModelSolver01::Model_7 &&
                 t <= (int)ModelSolver01::Model_12))
    {
        addParam("omega1", true); addParam("omega2", true);
        addParam("lambda1", true); addParam("lambda2", true);
    }

    int sType = t % 4;
    if(sType != 1) {
        addParam("C", true); addParam("S", true);
    }
    if (sType == 2 || sType == 3) {
        addParam("alpha", false); addParam("C_phi", false);
    }
    addParam("gamaD", false);

    FitParameter lfd;
    lfd.name = "LfD";
    lfd.displayName = "无因次缝长 LfD";
    lfd.value = defs.value("Lf", 50.0)/defs.value("L", 1000.0);
    lfd.isFit = false;
    lfd.isVisible = true;
    lfd.step = 0;
    params.append(lfd);

    return params;
}

/**
 * @brief 自动推演步长和上下限
 */
void FittingParameterChart::adjustLimits(QList<FitParameter>& params) {
    for(auto& p : params) {
        if(p.name == "LfD") continue;
        double val = p.value;
        if (std::abs(val) > 1e-15) {
            p.min = val > 0 ? val*0.1 : val*10.0;
            p.max = val > 0 ? val*10.0 : val*0.1;
        } else {
            p.min = 0.0; p.max = 1.0;
        }

        if (p.name == "phi" || p.name.startsWith("omega")) {
            p.max = qMin(p.max, 1.0);
            p.min = qMax(p.min, 0.0001);
        }

        if (p.name == "kf" || p.name == "M12" || p.name == "L" ||
            p.name == "Lf" || p.name == "rm" || p.name == "re" ||
            p.name.startsWith("lambda") || p.name == "h" ||
            p.name == "rw" || p.name == "mu" || p.name == "B" ||
            p.name == "Ct" || p.name == "C" || p.name == "q" ||
            p.name == "alpha" || p.name == "C_phi")
        {
            p.min = qMax(p.min, qMax(std::abs(val)*0.01, 1e-6));
        }

        if (p.name == "nf") {
            p.min = qMax(std::ceil(p.min), 1.0);
            p.max = std::floor(p.max);
            p.step = 1.0;
        }

        if (p.max - p.min > 1e-20 && p.name != "nf") {
            double rs = (p.max - p.min) / 20.0;
            double mag = std::pow(10.0, std::floor(std::log10(rs)));
            p.step = qMax(std::round(rs / mag * 10.0) / 10.0, 0.1) * mag;
        } else if (p.name != "nf") {
            p.step = 0.1;
        }
    }
}

/**
 * @brief 加载重绘
 */
void FittingParameterChart::resetParams(
    ModelManager::ModelType type, bool preserveStates)
{
    QMap<QString, QPair<bool, bool>> bkp;
    if (preserveStates) {
        for(const auto& p : m_params)
            bkp[p.name] = {p.isFit, p.isVisible};
    }

    m_params = generateDefaultParams(type);

    if (preserveStates) {
        for(auto& p : m_params) {
            if(bkp.contains(p.name)) {
                p.isFit = p.name=="LfD" ? false : bkp[p.name].first;
                p.isVisible = bkp[p.name].second;
            }
        }
    }
    autoAdjustLimits();
    refreshParamTable();
}

void FittingParameterChart::autoAdjustLimits() { adjustLimits(m_params); }
QList<FitParameter> FittingParameterChart::getParameters() const { return m_params; }
void FittingParameterChart::setParameters(const QList<FitParameter> &p) { m_params = p; refreshParamTable(); }

/**
 * @brief 接收新模型
 */
void FittingParameterChart::switchModel(ModelManager::ModelType newType) {
    QMap<QString, double> old;
    for(const auto& p : m_params) old.insert(p.name, p.value);

    resetParams(newType, false);

    // 同名属性不自动还原，维护反演工作流顺畅
    for(auto& p : m_params) {
        if(old.contains(p.name)) p.value = old[p.name];
    }
    autoAdjustLimits();

    double cL = 1000.0;
    for(const auto& p : m_params) if(p.name == "L") cL = p.value;
    for(auto& p : m_params) {
        if(p.name == "rm") {
            p.min = qMax(p.min, cL);
            p.value = qMax(p.value, p.min);
        }
        if(p.name == "LfD") {
            double cLf = 20.0;
            for(const auto& pp : m_params)
                if(pp.name == "Lf") cLf = pp.value;
            if(cL > 1e-9) p.value = cLf / cL;
        }
    }
    refreshParamTable();
}

/**
 * @brief 表格改动上传内存
 */
void FittingParameterChart::updateParamsFromTable() {
    if(!m_table) return;
    for(int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* iK = m_table->item(i, 1);
        if(!iK) continue;
        QTableWidgetItem* iV = m_table->item(i, 2);
        QString k = iK->data(Qt::UserRole).toString();
        QString txt = iV->text();

        double v = txt.contains(QRegularExpression("[,，]")) ?
                       txt.split(QRegularExpression("[,，]"), Qt::SkipEmptyParts).first().toDouble() :
                       txt.toDouble();

        if (k == "nf") v = qMax(1.0, std::round(v));
        for(auto& p : m_params) {
            if(p.name == k) { p.value = v; break; }
        }
    }
}

/**
 * @brief 文本原样提取防截断
 */
QMap<QString, QString> FittingParameterChart::getRawParamTexts() const {
    QMap<QString, QString> res;
    if(!m_table) return res;
    for(int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* k = m_table->item(i, 1);
        QTableWidgetItem* v = m_table->item(i, 2);
        if (k && v) res.insert(k->data(Qt::UserRole).toString(), v->text());
    }
    return res;
}

/**
 * @brief 开始重绘
 */
void FittingParameterChart::refreshParamTable() {
    if(!m_table) return;
    m_table->blockSignals(true);
    m_table->setRowCount(0);
    int no = 1;

    for(const auto& p : m_params)
        if(p.isVisible && p.isFit) addRowToTable(p, no, true);
    for(const auto& p : m_params)
        if(p.isVisible && !p.isFit) addRowToTable(p, no, false);

    m_table->blockSignals(false);
}

/**
 * @brief 加入行列展示数据
 */
void FittingParameterChart::addRowToTable(
    const FitParameter& p, int& serialNo, bool highlight)
{
    int r = m_table->rowCount();
    m_table->insertRow(r);
    QColor bg = p.name == "LfD" ?
                    QColor(245, 245, 245) :
                    (highlight ? QColor(255, 255, 224) : Qt::white);

    QTableWidgetItem* i0 = new QTableWidgetItem(QString::number(serialNo++));
    i0->setFlags(i0->flags() & ~Qt::ItemIsEditable);
    i0->setTextAlignment(Qt::AlignCenter);
    i0->setBackground(bg);
    m_table->setItem(r, 0, i0);

    QTableWidgetItem* i1 = new QTableWidgetItem(p.displayName);
    i1->setFlags(i1->flags() & ~Qt::ItemIsEditable);
    i1->setData(Qt::UserRole, p.name);
    i1->setBackground(bg);
    if(highlight) { QFont f = i1->font(); f.setBold(true); i1->setFont(f); }
    m_table->setItem(r, 1, i1);

    QTableWidgetItem* i2 = new QTableWidgetItem(
        p.name == "nf" ?
            QString::number(qMax(1.0, std::round(p.value))) :
            QString::number(p.value, 'g', 6));
    i2->setBackground(bg);
    if(highlight) { QFont f = i2->font(); f.setBold(true); i2->setFont(f); }
    if (p.name == "LfD") {
        i2->setFlags(i2->flags() & ~Qt::ItemIsEditable);
        i2->setForeground(QBrush(Qt::darkGray));
    }
    m_table->setItem(r, 2, i2);

    QString d, u;
    getParamDisplayInfo(p.name, d, d, d, u);
    QTableWidgetItem* i3 = new QTableWidgetItem(
        u == "无因次" || u == "小数" ? "-" : u);
    i3->setFlags(i3->flags() & ~Qt::ItemIsEditable);
    i3->setBackground(bg);
    m_table->setItem(r, 3, i3);
}

/**
 * @brief 【界面显示核心】配置带 Unicode 角标与希腊字格式的排版系统
 */
void FittingParameterChart::getParamDisplayInfo(
    const QString &n, QString &cN, QString &s, QString &uS, QString &u)
{
    if(n == "kf") { cN = "内区渗透率 kf"; u = "mD"; }
    else if(n == "M12") { cN = "流度比 M₁₂"; u = "无因次"; }
    else if(n == "L") { cN = "水平井长 L"; u = "m"; }
    else if(n == "Lf") { cN = "裂缝半长 Lf"; u = "m"; }
    else if(n == "rm") { cN = "复合半径 rm"; u = "m"; }
    else if(n == "omega1") { cN = "内区储容比 ω₁"; u = "无因次"; }
    else if(n == "omega2") { cN = "外区储容比 ω₂"; u = "无因次"; }
    else if(n == "lambda1") { cN = "内区窜流系数 λ₁"; u = "无因次"; }
    else if(n == "lambda2") { cN = "外区窜流系数 λ₂"; u = "无因次"; }

    else if(n == "omega_f1") { cN = "内区裂缝储容比 ωf₁"; u = "无因次"; }
    else if(n == "omega_v1") { cN = "内区溶洞储容比 ωv₁"; u = "无因次"; }
    else if(n == "lambda_m1") { cN = "内区基质窜流系数 λm₁"; u = "无因次"; }
    else if(n == "lambda_v1") { cN = "内区溶洞窜流系数 λv₁"; u = "无因次"; }
    else if(n == "omega_f2") { cN = "外区裂缝储容比 ωf₂"; u = "无因次"; }
    else if(n == "lambda_m2") { cN = "外区基质窜流系数 λm₂"; u = "无因次"; }

    else if(n == "re") { cN = "外区半径 re"; u = "m"; }
    else if(n == "nf") { cN = "裂缝条数 nf"; u = "条"; }
    else if(n == "h") { cN = "有效厚度 h"; u = "m"; }
    else if(n == "rw") { cN = "井筒半径 rw"; u = "m"; }
    else if(n == "phi") { cN = "孔隙度 φ"; u = "小数"; }
    else if(n == "mu") { cN = "流体粘度 μ"; u = "mPa·s"; }
    else if(n == "B") { cN = "体积系数 B"; u = "无因次"; }
    else if(n == "Ct") { cN = "综合压缩系数 Ct"; u = "MPa⁻¹"; }
    else if(n == "q") { cN = "测试产量 q"; u = "m³/d"; }
    else if(n == "C") { cN = "井筒储集 C"; u = "m³/MPa"; }
    else if(n == "cD") { cN = "无因次井储 cD"; u = "无因次"; }
    else if(n == "S") { cN = "表皮系数 S"; u = "无因次"; }
    else if(n == "gamaD") { cN = "压敏系数 γD"; u = "无因次"; }
    else if(n == "LfD") { cN = "无因次缝长 LfD"; u = "无因次"; }
    else if(n == "alpha") { cN = "变井储时间参数 α"; u = "h"; }
    else if(n == "C_phi") { cN = "变井储压力参数 Cφ"; u = "MPa"; }
    else { cN = n; u = ""; }

    s = uS = n;
}
