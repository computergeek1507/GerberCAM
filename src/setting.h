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

#ifndef SETTING_H
#define SETTING_H

#include <QString>
#include <QDataStream>
#include <QFile>
#include <QMessageBox>

//enum UnitType{unitInch,unitMM};
//enum SpeedUnit{MMperSec,MMperMin,InchperSec,InchperMin};
//enum ToolType{Conical,Cylindrical,Drill};



struct Tool
{
    QString name;
    QString unitType;
    QString speedUnit;
    QString toolType;
    double diameter;
    double angle;
    double width;
    double overlap;
    double maxStepDepth;
    double maxPlungeSpeed;
    double spindleSpeed;
    double feedrate;
};

struct HoleCondition
{
    Tool drill;
    QString condition;
    double value;
    double value1;
    QString text;
};

struct HoleRule
{
    QString name;
    QList<HoleCondition> ruleList;
};

QDataStream &operator <<(QDataStream &out,const Tool &toolBit);
QDataStream &operator >>(QDataStream &in, Tool &toolBit);

QDataStream &operator <<(QDataStream &out,const HoleCondition &holeCondition);
QDataStream &operator >>(QDataStream &in, HoleCondition &holeCondition);

QDataStream &operator <<(QDataStream &out,const HoleRule &holeRule);
QDataStream &operator >>(QDataStream &in, HoleRule &holeRule);


class Setting
{
public:
    Setting();
    ~Setting();
    Tool engravingTool;
    Tool hatchingTool;
    Tool drillTool;
    Tool centeringTool;
    QList<Tool> toolList;
    QList<Tool> drillList;
    QList<HoleRule> holeRuleList;
    int selectedRule=0;

    void appendTool(Tool t);
    void saveLibrary();
    void saveHoleRule();
    void replaceTool(int index, Tool t);
protected:

    bool readHoleRule();
    bool readTool();
};


#endif // SETTING_H
