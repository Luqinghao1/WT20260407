/*
 * 文件名: modelselect.cpp
 * 作用:
 * 1. 实现模型选择界面交互。
 * 2. 修正“页岩型+夹层型”名称。
 * 3. 增加“混积型径向复合模型”
 * 及其三类内外区子选项。
 * 4. 映射组合生成 1-108 编号。
 */

#include "modelselect.h"
#include "ui_modelselect.h"
#include <QDialogButtonBox>
#include <QPushButton>

ModelSelect::ModelSelect(
    QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ModelSelect),
    m_isInitializing(false)
{
    ui->setupUi(this);
    this->setStyleSheet(
        "QWidget { color: black; "
        "font-family: Arial; }");

    // 加载全部模型组合选项
    initOptions();

    // 绑定信号槽
    connect(
        ui->comboReservoirModel,
        SIGNAL(currentIndexChanged(int)),
        this,
        SLOT(updateInnerOuterOptions()));

    connect(
        ui->comboWellModel,
        SIGNAL(currentIndexChanged(int)),
        this,
        SLOT(onSelectionChanged()));

    connect(
        ui->comboReservoirModel,
        SIGNAL(currentIndexChanged(int)),
        this,
        SLOT(onSelectionChanged()));

    connect(
        ui->comboBoundary,
        SIGNAL(currentIndexChanged(int)),
        this,
        SLOT(onSelectionChanged()));

    connect(
        ui->comboStorage,
        SIGNAL(currentIndexChanged(int)),
        this,
        SLOT(onSelectionChanged()));

    connect(
        ui->comboInnerOuter,
        SIGNAL(currentIndexChanged(int)),
        this,
        SLOT(onSelectionChanged()));

    disconnect(
        ui->buttonBox,
        &QDialogButtonBox::accepted,
        this,
        &QDialog::accept);

    connect(
        ui->buttonBox,
        &QDialogButtonBox::accepted,
        this,
        &ModelSelect::onAccepted);

    connect(
        ui->buttonBox,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject);

    onSelectionChanged();
}

ModelSelect::~ModelSelect() {
    delete ui;
}

void ModelSelect::initOptions()
{
    m_isInitializing = true;
    ui->comboWellModel->clear();
    ui->comboReservoirModel->clear();
    ui->comboBoundary->clear();
    ui->comboStorage->clear();
    ui->comboInnerOuter->clear();

    ui->comboWellModel->addItem(
        "压裂水平井",
        "FracHorizontal");

    ui->comboReservoirModel->addItem(
        "径向复合模型",
        "RadialComposite");

    ui->comboReservoirModel->addItem(
        "夹层型径向复合模型",
        "InterlayerComposite");

    ui->comboReservoirModel->addItem(
        "页岩型径向复合模型",
        "ShaleComposite");

    // 修改为混积型
    ui->comboReservoirModel->addItem(
        "混积型径向复合模型",
        "MixedComposite");

    ui->comboBoundary->addItem(
        "无限大外边界", "Infinite");
    ui->comboBoundary->addItem(
        "封闭边界", "Closed");
    ui->comboBoundary->addItem(
        "定压边界",
        "ConstantPressure");

    ui->comboStorage->addItem(
        "定井储", "Constant");
    ui->comboStorage->addItem(
        "线源解", "LineSource");
    ui->comboStorage->addItem(
        "Fair模型", "Fair");
    ui->comboStorage->addItem(
        "Hegeman模型", "Hegeman");

    ui->comboWellModel->
        setCurrentIndex(0);
    ui->comboReservoirModel->
        setCurrentIndex(0);
    ui->comboBoundary->
        setCurrentIndex(0);
    ui->comboStorage->
        setCurrentIndex(0);

    m_isInitializing = false;
    updateInnerOuterOptions();
}

void ModelSelect::
    updateInnerOuterOptions()
{
    bool oS = ui->comboInnerOuter->
              blockSignals(true);
    ui->comboInnerOuter->clear();

    QString cR =
        ui->comboReservoirModel->
        currentData().toString();

    if (cR == "RadialComposite") {
        ui->comboInnerOuter->addItem(
            "均质+均质", "Homo_Homo");
    }
    else if (cR ==
             "InterlayerComposite") {
        ui->comboInnerOuter->addItem(
            "夹层型+夹层型",
            "Interlayer_Interlayer");
        ui->comboInnerOuter->addItem(
            "夹层型+均质",
            "Interlayer_Homo");
    }
    else if (cR == "ShaleComposite") {
        ui->comboInnerOuter->addItem(
            "页岩型+页岩型",
            "Shale_Shale");
        ui->comboInnerOuter->addItem(
            "页岩型+均质",
            "Shale_Homo");
        ui->comboInnerOuter->addItem(
            "页岩型+夹层型",
            "Shale_Interlayer");
    }
    else if (cR == "MixedComposite") {
        ui->comboInnerOuter->addItem(
            "三重孔隙介质+夹层型",
            "Triple_Interlayer");
        ui->comboInnerOuter->addItem(
            "三重孔隙介质+页岩型",
            "Triple_Shale");
        ui->comboInnerOuter->addItem(
            "三重孔隙介质+均质",
            "Triple_Homo");
    }

    if (ui->comboInnerOuter->
        count() > 0) {
        ui->comboInnerOuter->
            setCurrentIndex(0);
    }

    ui->comboInnerOuter->
        blockSignals(oS);
    ui->label_InnerOuter->
        setVisible(true);
    ui->comboInnerOuter->
        setVisible(true);
}

void ModelSelect::
    setCurrentModelCode(const QString& c)
{
    m_isInitializing = true;
    QString nS = c;
    nS.remove("modelwidget");
    int id = nS.toInt();

    if (id >= 1) {
        int iW = ui->comboWellModel->
                 findData("FracHorizontal");
        if (iW >= 0) {
            ui->comboWellModel->
                setCurrentIndex(iW);
        }

        QString rD, iD;
        if (id <= 12) {
            rD = "InterlayerComposite";
            iD = "Interlayer_Interlayer";
        }
        else if (id <= 24) {
            rD = "InterlayerComposite";
            iD = "Interlayer_Homo";
        }
        else if (id <= 36) {
            rD = "RadialComposite";
            iD = "Homo_Homo";
        }
        else if (id <= 48) {
            rD = "ShaleComposite";
            iD = "Shale_Shale";
        }
        else if (id <= 60) {
            rD = "ShaleComposite";
            iD = "Shale_Homo";
        }
        else if (id <= 72) {
            rD = "ShaleComposite";
            iD = "Shale_Interlayer";
        }
        else if (id <= 84) {
            rD = "MixedComposite";
            iD = "Triple_Interlayer";
        }
        else if (id <= 96) {
            rD = "MixedComposite";
            iD = "Triple_Shale";
        }
        else if (id <= 108){
            rD = "MixedComposite";
            iD = "Triple_Homo";
        }

        int iR = ui->
                 comboReservoirModel->
                 findData(rD);

        if (iR >= 0) {
            ui->comboReservoirModel->
                setCurrentIndex(iR);
            updateInnerOuterOptions();
        }

        int gO = (id - 1) % 12;
        QString bD;
        if (gO < 4) bD = "Infinite";
        else if (gO < 8) bD = "Closed";
        else bD = "ConstantPressure";

        int iB = ui->comboBoundary->
                 findData(bD);
        if (iB >= 0) {
            ui->comboBoundary->
                setCurrentIndex(iB);
        }

        int sO = (id - 1) % 4;
        QString sD;
        if (sO == 0) sD = "Constant";
        else if (sO == 1)
            sD = "LineSource";
        else if (sO == 2)
            sD = "Fair";
        else sD = "Hegeman";

        int iS = ui->comboStorage->
                 findData(sD);
        if (iS >= 0) {
            ui->comboStorage->
                setCurrentIndex(iS);
        }

        int iIo = ui->comboInnerOuter->
                  findData(iD);
        if (iIo >= 0) {
            ui->comboInnerOuter->
                setCurrentIndex(iIo);
        }
    }
    m_isInitializing = false;
    onSelectionChanged();
}

void ModelSelect::onSelectionChanged()
{
    if (m_isInitializing) return;

    QString well =
        ui->comboWellModel->
        currentData().toString();
    QString res =
        ui->comboReservoirModel->
        currentData().toString();
    QString bnd =
        ui->comboBoundary->
        currentData().toString();
    QString store =
        ui->comboStorage->
        currentData().toString();
    QString io =
        ui->comboInnerOuter->
        currentData().toString();

    m_selectedModelCode = "";
    m_selectedModelName = "";

    auto calcID = [&](
                      int sId,
                      QString bT,
                      QString sT) -> int {

        int oB = 0;
        if (bT == "Closed") oB = 4;
        else if (bT ==
                 "ConstantPressure") oB = 8;

        int oS = 0;
        if (sT == "LineSource") oS = 1;
        else if (sT == "Fair") oS = 2;
        else if (sT == "Hegeman") oS=3;

        return sId + oB + oS;
    };

    int bSId = 0;
    QString bNCn = "";

    if (well == "FracHorizontal") {
        if (res ==
            "InterlayerComposite") {
            if (io ==
                "Interlayer_Interlayer")
            {
                bSId = 1;
                bNCn = "夹层型"
                       "试井模型";
            }
            else if (io ==
                     "Interlayer_Homo")
            {
                bSId = 13;
                bNCn = "夹层型"
                       "试井模型";
            }
        }
        else if (res ==
                 "RadialComposite") {
            if (io == "Homo_Homo") {
                bSId = 25;
                bNCn = "径向复合模型";
            }
        }
        else if (res ==
                 "ShaleComposite") {
            if (io == "Shale_Shale") {
                bSId = 37;
                bNCn = "页岩型"
                       "试井模型";
            }
            else if (io ==
                     "Shale_Homo") {
                bSId = 49;
                bNCn = "页岩型"
                       "试井模型";
            }
            else if (io ==
                     "Shale_Interlayer")
            {
                bSId = 61;
                bNCn = "页岩型"
                       "试井模型";
            }
        }
        else if (res ==
                 "MixedComposite") {
            if (io ==
                "Triple_Interlayer") {
                bSId = 73;
                bNCn = "混积型"
                       "试井模型";
            }
            else if (io ==
                     "Triple_Shale") {
                bSId = 85;
                bNCn = "混积型"
                       "试井模型";
            }
            else if (io ==
                     "Triple_Homo") {
                bSId = 97;
                bNCn = "混积型"
                       "试井模型";
            }
        }
    }

    if (bSId > 0) {
        int fId = calcID(
            bSId, bnd, store);
        m_selectedModelCode =
            QString("modelwidget%1").
            arg(fId);

        int dId = 0;
        if (bSId == 25)
            dId = fId - 24;
        else if (bSId >= 37 &&
                 bSId <= 72)
            dId = fId - 36;
        else if (bSId >= 73)
            dId = fId - 72;
        else
            dId = fId;

        m_selectedModelName =
            QString("%1%2").
            arg(bNCn).arg(dId);
    }

    bool isV = !m_selectedModelCode.
                isEmpty();

    if (isV) {
        ui->label_ModelName->
            setText(m_selectedModelName);
        ui->label_ModelName->
            setStyleSheet(
                "color: black; "
                "font-weight: bold; "
                "font-size: 14px;");
    } else {
        ui->label_ModelName->
            setText(
                "该组合暂无已实现模型");
        ui->label_ModelName->
            setStyleSheet(
                "color: red; "
                "font-weight: normal;");
    }

    if(QPushButton* okB =
        ui->buttonBox->button(
            QDialogButtonBox::Ok))
    {
        okB->setEnabled(isV);
    }
}

void ModelSelect::onAccepted() {
    if (!m_selectedModelCode.isEmpty())
        accept();
}

QString ModelSelect::
    getSelectedModelCode() const {
    return m_selectedModelCode;
}

QString ModelSelect::
    getSelectedModelName() const {
    return m_selectedModelName;
}
