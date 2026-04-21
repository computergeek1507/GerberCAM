/*
Copyright (c) <2014> <Lichao Ma>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef SETTINGWINDOW_H
#define SETTINGWINDOW_H

#include <QDialog>

#include "setting.h"
#include "treemodel.h"
#include <QMessageBox>
namespace Ui {
class settingwindow;
}

class Settingwindow : public QDialog
{
    Q_OBJECT

public:
    explicit Settingwindow(QString const& appData, QWidget *parent = 0);
    ~Settingwindow();

    Setting* settings{nullptr};

    std::optional<Tool> getCurrentTool();

protected:
    void updateWindow(Tool t);
    void updateWindow();
    bool checkValue(Tool &t, bool newTool);
    bool checkHoleRuleValue(HoleCondition &c);
    void updateMTreeView(HoleRule r);
    void holeDrillCheck();
private slots:
    void on_tlAddButton_clicked();

    void on_tlUnitInchRadio_clicked();

    void on_tlUnitMMRadio_clicked();

    void on_tlTypeComboBox_currentIndexChanged(int index);

    void on_tlTypeComboBox_activated(int index);

    void on_treeView_clicked(const QModelIndex &index);

    void on_treeView_doubleClicked(const QModelIndex &index);

    void on_tlDeleteButton_clicked();

    void on_tlEditButton_clicked();

    void on_tlSaveButton_clicked();

    void on_tlCancelButton_clicked();

    void on_buttonYes_clicked();

    void on_buttonNo_clicked();

    void on_mSelectRuleComboBox_activated(const QString &arg1);

    void on_mNewButton_clicked();

    void on_mEditButton_clicked();

    void on_mSaveButton_clicked();

    void on_mCancelButton_clicked();

    void on_mREUpButton_clicked();

    void on_mREDownButton_clicked();

    void on_mREAddButton_clicked();

    void on_mREDeleteButton_clicked();

    void on_mREEditButton_clicked();

    void on_mRESaveButton_clicked();

    void on_mRECancelButton_clicked();

    void on_tabWidget_tabBarClicked(int index);

    void on_mSelectRuleComboBox_activated(int index);

    void on_mDeleteButton_clicked();

    void on_mREDrillComboBox_activated(int index);
    //void on_comboBoxEngravingTool_currentIndexChanged(int index);

    void on_pushButtonSaveBit_clicked();

    void refreshToolCombobox();

private:
    Ui::settingwindow *ui;

    TreeModel *tlModel,*hrModel;
    HoleRule hr;
    QString unitType="Milimeter";
    bool editFlag=false;
    int editIndex=0;

    int mEditIndex=0;
    bool mEditFlag=false;

    int mREEditIndex=0;
    bool mREEditFlag=false;

};

#endif // SETTINGWINDOW_H
