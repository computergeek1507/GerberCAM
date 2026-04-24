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

#pragma once

#include <QString>
#include <QDataStream>
//#include <QFile>
#include <QMessageBox>

#include <QtMath>

#include <optional>

#include "nlohmann/json.hpp"

#include <magic_enum/magic_enum.hpp>

#include "spdlog/spdlog.h"
#include "spdlog/common.h"

enum class UnitType : int { Inch, Milimeter = 1};
//enum class SpeedUnit : int { MMperSec, MMperMin, InchperSec, InchperMin};
enum class ToolType : int { Conical, Cylindrical, Drill};

struct CuttingParm
{
    QString toolName;
	double depth{ 1.7 };
    int isolationRings{ 1 };
	double overlap{ 0.5 };
	//double stepDown{ 0.1 };
    bool clearEmptyArea{ false };
	CuttingParm() = default;
	CuttingParm(nlohmann::json const& j)
	{
		toolName = QString::fromStdString(j.value("toolName", ""));
		depth = j.value("depth", depth);
        isolationRings = j.value("isolationRings", isolationRings);
        overlap = j.value("overlap", overlap);
        clearEmptyArea = j.value("clearEmptyArea", clearEmptyArea);
	}
	nlohmann::json toJson() const
	{
		nlohmann::json j;
		j["toolName"] = toolName.toStdString();
		j["depth"] = depth;
        j["isolationRings"] = isolationRings;
		j["overlap"] = overlap;
        j["clearEmptyArea"] = clearEmptyArea;
		return j;
	}
};

struct Tool
{
    QString name;
    //QString unitType = "MM";
    //QString speedUnit = "InchperMin";
    //QString toolType = "Conical";
    UnitType unitType = UnitType::Milimeter;
    //SpeedUnit speedUnit = SpeedUnit::InchperMin;
    ToolType toolType = ToolType::Conical;

    double diameter{0.0};
    double angle{0.0};
    double width{0.0};
    double overlap{0.0};
    double maxStepDepth{0.0};
    double maxPlungeSpeed{0.0};
    double spindleSpeed{12000.0};//rpm
    double feedrate{0.0};

    double calculateWidth(double depth) const
    {
        if (toolType  == ToolType::Conical)
        {
			return width + 2 * depth * std::tan(qDegreesToRadians(angle / 2.0));
        }
		return width;
    }

	Tool() = default;

    Tool(nlohmann::json const& j)
    {
        name = QString::fromStdString(j.value("name", ""));
        unitType = magic_enum::enum_cast<UnitType>(j.value("unitType", magic_enum::enum_name(unitType)), magic_enum::case_insensitive).value_or(unitType);
        //speedUnit = magic_enum::enum_cast<SpeedUnit>(j.value("speedUnit", magic_enum::enum_name(speedUnit)), magic_enum::case_insensitive).value_or(speedUnit);
        toolType = magic_enum::enum_cast<ToolType>(j.value("toolType", magic_enum::enum_name(toolType)), magic_enum::case_insensitive).value_or(toolType);
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
		j["unitType"] = magic_enum::enum_name(unitType);
		//j["speedUnit"] = magic_enum::enum_name(speedUnit);
		j["toolType"] = magic_enum::enum_name(toolType);
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

class Setting
{
public:
    Setting(QString const& appData);
    ~Setting();
    CuttingParm engravingParm;
    CuttingParm drillParm;
    CuttingParm cutParm;
    bool openGcodeInNotepad{false};

    //Tool engravingTool;
    //Tool drillTool;
    //Tool cutTool;

    QList<Tool> toolList;
    //QList<Tool> drillList;
    QList<HoleRule> holeRuleList;
    int selectedRule = 0;

    void appendTool(Tool t);
    void saveLibrary();
    void saveHoleRule();
    void saveSettings();
    void replaceTool(int index, Tool t);

    std::optional<Tool> getEngravingTool() const;
    std::optional<Tool> getDrillTool() const;
    std::optional<Tool> getCutTool() const;

    std::vector<Tool> getDrillList() const;

    QString lastDir() const;
    void setLastDir(const QString& filePath);

	bool isDrillTool(Tool const& t) const;
	bool hasTool(QString const& toolName) const;
    
    std::optional<Tool> getTool(QString const& toolName) const;

protected:

    bool readHoleRule(QString const& appData);
    bool readTool(QString const& appData);
    bool readSettings(QString const& appData);

    std::shared_ptr<spdlog::logger> m_logger{ nullptr };

    QString m_appData;
    QString m_lastDir;
};

