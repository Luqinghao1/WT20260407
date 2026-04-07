/*
 * 文件名: modelselect.h
 * 作用:
 * 1. 声明模型选择对话框 UI 类。
 * 2. 提供模型代码、名称的获取。
 * 3. 支持高达 108 种模型的切换。
 */

#ifndef MODELSELECT_H
#define MODELSELECT_H

#include <QDialog>

namespace Ui {
class ModelSelect;
}

class ModelSelect : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * 初始化界面及选项
     */
    explicit ModelSelect(
        QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     * 释放界面内存
     */
    ~ModelSelect();

    /**
     * @brief 获取选中模型代码
     * 返回内部标识符(如 modelwidget1)
     */
    QString getSelectedModelCode()
        const;

    /**
     * @brief 获取选中模型名称
     * 返回中文描述
     */
    QString getSelectedModelName()
        const;

    /**
     * @brief 设置当前模型
     * 供界面回显当前激活模型
     */
    void setCurrentModelCode(
        const QString& code);

private slots:
    /**
     * @brief 选项变动槽函数
     * 拼接计算最终的模型ID
     */
    void onSelectionChanged();

    /**
     * @brief 确认按钮槽函数
     */
    void onAccepted();

    /**
     * @brief 更新内外区选项
     * 依据储层大类更新子项
     */
    void updateInnerOuterOptions();

private:
    /**
     * @brief 初始选项加载
     * 挂载所有的文字及代码映射
     */
    void initOptions();

private:
    Ui::ModelSelect *ui;
    QString m_selectedModelCode;
    QString m_selectedModelName;
    bool m_isInitializing;
};

#endif // MODELSELECT_H
