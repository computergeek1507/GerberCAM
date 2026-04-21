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

#include "setting.h"

#include "config.h"

#include <fstream>
#include <nlohmann/json.hpp>

#define BINARY_TOOL_LIBRARY_FILENAME "tool_library.con"
#define JSON_TOOL_LIBRARY_FILENAME "tool_library.json"

#define BINARY_HOLE_RULE_FILENAME "hole_rule.con"
#define JSON_HOLE_RULE_FILENAME "hole_rule.json"

#define JSON_SETTINGS_FILENAME "settings.json"

inline QDataStream &operator <<(QDataStream &out,const Tool &toolBit)
{
    out<<toolBit.name<<toolBit.unitType<<toolBit.toolType<<toolBit.diameter
            <<toolBit.angle<<toolBit.width<<toolBit.overlap<<toolBit.maxPlungeSpeed
                <<toolBit.maxStepDepth<<toolBit.spindleSpeed<<toolBit.feedrate;
    return out;
}

inline QDataStream &operator >>(QDataStream &in,Tool &toolBit)
{
    in>>toolBit.name>>toolBit.unitType>>toolBit.toolType>>toolBit.diameter
            >>toolBit.angle>>toolBit.width>>toolBit.overlap>>toolBit.maxPlungeSpeed
                >>toolBit.maxStepDepth>>toolBit.spindleSpeed>>toolBit.feedrate;
    return in;
}

inline QDataStream &operator <<(QDataStream &out,const HoleCondition &holeCondition)
{
    out<<holeCondition.condition<<holeCondition.drill<<holeCondition.value<<holeCondition.value1<<holeCondition.text;
    return out;
}

inline QDataStream &operator >>(QDataStream &in,HoleCondition &holeCondition)
{
    in>>holeCondition.condition>>holeCondition.drill>>holeCondition.value>>holeCondition.value1>>holeCondition.text;
    return in;
}

inline QDataStream &operator <<(QDataStream &out,const HoleRule &holeRule)
{
    out<<holeRule.name<<holeRule.ruleList.size();
    for(int i=0;i<holeRule.ruleList.size();i++)
    {
        out<<holeRule.ruleList.at(i);
    }
    return out;
}

inline QDataStream &operator >>(QDataStream &in,HoleRule &holeRule)
{
    int num;
    in>>holeRule.name>>num;
    for(int i=0;i<num;i++)
    {
        HoleCondition temp;
        in>>temp;
        holeRule.ruleList.append(temp);
    }
    return in;
}

bool Setting::readTool(QString const& appData)
{
    auto path = appData + "/" + JSON_TOOL_LIBRARY_FILENAME;

    if (!QFile::exists(path))
    {
        path = JSON_TOOL_LIBRARY_FILENAME;
    }

    if (QFile::exists(path))
    {
        try 
        {
			std::ifstream in(path.toStdString());
			nlohmann::json j;
			in >> j;
			toolList.clear();
            if (j.contains("toolList") && j["toolList"].is_array()) {
                for (const auto& item : j["toolList"]) {
                    Tool t(item);
                    toolList.append(t);
                    if (t.toolType == "Drill")
                    {
                        drillList.append(t);
                    }
                }
            }
            return true;
        }
        catch (std::exception &ex) 
        {
			m_logger->error("Error reading tool library: {}", ex.what());
            return false;
        }
    }
    else if(QFile::exists(BINARY_TOOL_LIBRARY_FILENAME))
    {
        QFile file(BINARY_TOOL_LIBRARY_FILENAME);
        QDataStream in(&file);
        in.setVersion(QDataStream::Qt_5_4);
        if (!file.open(QIODevice::ReadOnly))
        {
			m_logger->error("Error reading tool library, cannot open file");
            QMessageBox msgBox;
            msgBox.setText("Error!Cannot open tool library!");
            msgBox.exec();
            return false;
        }

        int num;

        in >> num;
        toolList.clear();
        drillList.clear();
        for (int i = 0; i < num; i++)
        {
            Tool t;
            in >> t;
            toolList.append(t);
            if (t.toolType == "Drill")
                drillList.append(t);
        }
        file.close();
        return true;
    }
    else
    {
        m_logger->error("Error reading tool library, File not found");
		QMessageBox msgBox;
		msgBox.setText("Error!Tool library not found!");
		msgBox.exec();
		
    }
    return false;
}

bool Setting::readHoleRule(QString const& appData)
{
    auto path = appData + "/" + JSON_HOLE_RULE_FILENAME;

    if (!QFile::exists(path))
    {
        path = JSON_HOLE_RULE_FILENAME;
    }
    if (QFile::exists(path))
    {
        try
        {
            std::ifstream in(path.toStdString());
            nlohmann::json j;
            in >> j;

			holeRuleList.clear();

			if (j.contains("holeRuleList") && j["holeRuleList"].is_array()) {
				for (const auto& item : j["holeRuleList"]) {
					HoleRule h(item);
					holeRuleList.append(h);
				}
			}
			return true;
		}
        catch (std::exception& ex)
        {
			m_logger->error("Error reading hole identification rule: {}", ex.what());
        }
    }
    else if (QFile::exists(BINARY_HOLE_RULE_FILENAME))
    {
        holeRuleList.clear();
        int num;
        QFile file1(BINARY_HOLE_RULE_FILENAME);
        QDataStream in1(&file1);
        in1.setVersion(QDataStream::Qt_5_4);
        if (!file1.open(QIODevice::ReadOnly))
        {
            QMessageBox msgBox;
            msgBox.setText("Error!Cannot open hole identification rule!");
            msgBox.exec();
            return false;
        }

        in1 >> num;
        for (int i = 0; i < num; i++)
        {
            HoleRule t;
            in1 >> t;
            holeRuleList.append(t);
        }
        file1.close();
        return true;
    }
    else
    {
		m_logger->error("Error reading hole identification rule, File not found");
		QMessageBox msgBox;
		msgBox.setText("Error!Hole identification rule not found!");
		msgBox.exec();
    }
    return false;
}

Setting::Setting(QString const& appData) : m_logger(spdlog::get(PROJECT_NAME))
{
	m_appData = appData;
    readTool(appData);
    readHoleRule(appData);
    readSettings(appData);
}
void Setting::appendTool(Tool t)
{
    toolList.append(t);
    if (t.toolType == "Drill")
    {
        drillList.append(t);
    }
}

void Setting::replaceTool(int index,Tool t)
{
    toolList.replace(index,t);
    if(t.toolType=="Drill")
    {
        drillList.clear();
        for(int i=0;i<toolList.size();i++)
            if(toolList.at(i).toolType=="Drill")
                drillList.append(toolList.at(i));
    }
}

void Setting::saveLibrary()
{
    try
    {
        nlohmann::json j;
		j["toolList"] = nlohmann::json::array();
		for (const auto& tool : toolList) {
			j["toolList"].push_back(tool.toJson());
		}
        std::ofstream out((m_appData + "/" + JSON_TOOL_LIBRARY_FILENAME).toStdString());
        out << j.dump(4); // Pretty print with 4 spaces indentation
    }
    catch (std::exception& ex)
    {
        m_logger->error("Error saving hole identification rule: {}", ex.what());
    }
    /*
    QFile file(DEFAULT_TOOL_LIBRARY_FILENAME);
    if(!file.open(QIODevice::WriteOnly))
    {
        QMessageBox msgBox;
        msgBox.setText("Error!Cannot save library!");
        msgBox.exec();
        return;
    }
    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_5_4);
    out<<toolList.size();
    for(int i=0;i<toolList.size();i++)
    {
        Tool t=toolList.at(i);
        if(t.toolType=="Conical")
        out<<t;
    }

    for(int i=0;i<toolList.size();i++)
    {
        Tool t=toolList.at(i);
        if(t.toolType=="Cylindrical")
        out<<t;
    }

    for(int i=0;i<toolList.size();i++)
    {
        Tool t=toolList.at(i);
        if(t.toolType=="Drill")
        out<<t;
    }
    file.flush();
    file.close();
    */
    toolList.clear();
    readTool(m_appData);
}

void Setting::saveHoleRule()
{
    try
    {
		nlohmann::json j;
		j["holeRuleList"] = nlohmann::json::array();
		for (const auto& rule : holeRuleList) {
			j["holeRuleList"].push_back(rule.toJson());
		}
        std::ofstream out((m_appData + "/" + JSON_HOLE_RULE_FILENAME).toStdString());
		out << j.dump(4); // Pretty print with 4 spaces indentation
    }
    catch(std::exception &ex)
    {
        m_logger->error("Error saving hole identification rule: {}", ex.what());
    }
    //QFile file(DEFAULT_HOLE_RULE_FILENAME);
    //if(!file.open(QIODevice::WriteOnly))
    //{
    //    QMessageBox msgBox;
    //    msgBox.setText("Error!Cannot save hole identification rule!");
    //    msgBox.exec();
    //    return;
    //}
    //QDataStream out(&file);
    //out.setVersion(QDataStream::Qt_5_4);
    //out<<holeRuleList.size();
    //for(int i=0;i<holeRuleList.size();i++)
    //{
    //    HoleRule t=holeRuleList.at(i);
    //    out<<t;
    //}
    //
    //file.flush();
    //file.close();
    //
}

bool Setting::readSettings(QString const& appData)
{
    auto path = appData + "/" + JSON_SETTINGS_FILENAME;

    if (QFile::exists(path))
    {
        try
        {
            std::ifstream in(path.toStdString());
            nlohmann::json j;
            in >> j;

            if (j.contains("engravingParm"))
                engravingParm = CuttingParm(j["engravingParm"]);
            if (j.contains("drillParm"))
                drillParm = CuttingParm(j["drillParm"]);
            if (j.contains("cutParm"))
                cutParm = CuttingParm(j["cutParm"]);

            return true;
        }
        catch (std::exception &ex)
        {
            m_logger->error("Error reading toolpath settings: {}", ex.what());
        }
    }
    return false;
}

void Setting::saveSettings()
{
    try
    {
        nlohmann::json j;
        j["engravingParm"] = engravingParm.toJson();
        j["drillParm"] = drillParm.toJson();
        j["cutParm"] = cutParm.toJson();

        std::ofstream out((m_appData + "/" + JSON_SETTINGS_FILENAME).toStdString());
        out << j.dump(4);
    }
    catch (std::exception& ex)
    {
        m_logger->error("Error saving toolpath settings: {}", ex.what());
    }
}

std::optional<Tool> Setting::getEngravingTool() const 
{
    for (const auto& tool : toolList) {
        if (tool.name == engravingParm.toolName) {
            return tool;
        }
    }
    return std::nullopt;
}

std::optional<Tool> Setting::getDrillTool() const
{
    for (const auto& tool : toolList) {
        if (tool.name == drillParm.toolName) {
            return tool;
        }
    }
    return std::nullopt;
}

std::optional<Tool> Setting::getCutTool() const
{
    for (const auto& tool : toolList) {
        if (tool.name == cutParm.toolName) {
            return tool;
        }
    }
    return std::nullopt;
}

Setting::~Setting()
{
	saveSettings();
}

