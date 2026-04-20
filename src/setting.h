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

#include "nlohmann/json.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/common.h"

//enum UnitType{unitInch,unitMM};
//enum SpeedUnit{MMperSec,MMperMin,InchperSec,InchperMin};
//enum ToolType{Conical,Cylindrical,Drill};

struct Tool
{
    QString name;
    QString unitType = "Inch";
    QString speedUnit = "InchperMin";;
    QString toolType = "Conical";;
    double diameter{0.0};
    double angle{0.0};
    double width{0.0};
    double overlap{0.0};
    double maxStepDepth{0.0};
    double maxPlungeSpeed{0.0};
    double spindleSpeed{0.0};
    double feedrate{0.0};

	Tool() = default;

    Tool(nlohmann::json const& j)
    {
        name = QString::fromStdString(j.value("name", ""));
        unitType = QString::fromStdString(j.value("unitType", unitType.toStdString()));
        speedUnit = QString::fromStdString(j.value("speedUnit", speedUnit.toStdString()));
        toolType = QString::fromStdString(j.value("toolType", toolType.toStdString()));
        diameter = j.value("diameter", diameter);
        angle = j.value("angle", angle);
        width = j.value("width", width);
        overlap = j.value("overlap", overlap);
        maxStepDepth = j.value("maxStepDepth", maxStepDepth);
        maxPlungeSpeed = j.value("maxPlungeSpeed", maxPlungeSpeed);
        spindleSpeed = j.value("spindleSpeed", spindleSpeed);
        feedrate = j.value("feedrate", feedrate);
    }

	nlohmann::json toJson() const
	{
		nlohmann::json j;
		j["name"] = name.toStdString();
		j["unitType"] = unitType.toStdString();
		j["speedUnit"] = speedUnit.toStdString();
		j["toolType"] = toolType.toStdString();
		j["diameter"] = diameter;
		j["angle"] = angle;
		j["width"] = width;
		j["overlap"] = overlap;
		j["maxStepDepth"] = maxStepDepth;
		j["maxPlungeSpeed"] = maxPlungeSpeed;
		j["spindleSpeed"] = spindleSpeed;
		j["feedrate"] = feedrate;
		return j;
	}
};

struct HoleCondition
{
    Tool drill;
    QString condition;
    double value;
    double value1;
    QString text;

    HoleCondition() = default;

    HoleCondition(nlohmann::json const& j)
    {
        drill = Tool(j.value("drill", nlohmann::json::object()));
        condition = QString::fromStdString(j.value("condition", ""));
        value = j.value("value", 0.0);
        value1 = j.value("value1", 0.0);
        text = QString::fromStdString(j.value("text", ""));
    }

    nlohmann::json toJson() const
    {
		nlohmann::json j;
		j["drill"] = drill.toJson();
		j["condition"] = condition.toStdString();
		j["value"] = value;
		j["value1"] = value1;
		j["text"] = text.toStdString();
		return j;
    }
};

struct HoleRule
{
    QString name;
    QList<HoleCondition> ruleList;

	HoleRule() = default;

	HoleRule(nlohmann::json const& j)
	{
		name = QString::fromStdString(j.value("name", ""));
		ruleList.clear();
		if (j.contains("ruleList") && j["ruleList"].is_array()) {
			for (const auto& item : j["ruleList"]) {
				ruleList.append(HoleCondition(item));
			}
		}
	}

    nlohmann::json toJson() const
    {
        nlohmann::json j;
        j["name"] = name.toStdString();
        j["ruleList"] = nlohmann::json::array();
        for (const auto& condition : ruleList) {
            j["ruleList"].push_back(condition.toJson());
        }
        return j;
    }
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

    std::shared_ptr<spdlog::logger> m_logger{ nullptr };
};


#endif // SETTING_H
