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
#include <QFileInfo>

#define JSON_TOOL_LIBRARY_FILENAME "tool_library.json"

#define JSON_HOLE_RULE_FILENAME "hole_rule.json"

#define JSON_SETTINGS_FILENAME "settings.json"

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
                    //if (t.toolType == "Drill")
                    //{
                    //    drillList.append(t);
                    //}
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
					holeRuleList.append(HoleRule(item));
				}
			}
			return true;
		}
        catch (std::exception& ex)
        {
			m_logger->error("Error reading hole identification rule: {}", ex.what());
        }
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
}

void Setting::replaceTool(int index,Tool t)
{
    //toolList[index] = t;

    //toolList[index] = t;
    toolList.replace(index, t);
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

            if (j.contains("engravingParm")) {
                engravingParm = CuttingParm(j["engravingParm"]);
            }
            if (j.contains("drillParm")) {
                drillParm = CuttingParm(j["drillParm"]);
            }
            if (j.contains("cutParm")) {
                cutParm = CuttingParm(j["cutParm"]);
            }
            if (j.contains("lastDir")) {
                m_lastDir = QString::fromStdString(j.value("lastDir", ""));
            }

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
        j["lastDir"] = m_lastDir.toStdString();

        std::ofstream out((m_appData + "/" + JSON_SETTINGS_FILENAME).toStdString());
        out << j.dump(4);
    }
    catch (std::exception& ex)
    {
        m_logger->error("Error saving toolpath settings: {}", ex.what());
    }
}

QString Setting::lastDir() const
{
    return m_lastDir;
}

void Setting::setLastDir(const QString& filePath)
{
    m_lastDir = QFileInfo(filePath).absolutePath();
}

bool Setting::isDrillTool(Tool const& t) const
{
    return t.toolType == ToolType::Drill;
}

bool Setting::hasTool(QString const& toolName) const
{
	auto idx = std::find_if(toolList.begin(), toolList.end(), [&](const Tool& tool) {
		return tool.name == toolName;
		});
    
    if (idx != toolList.end()) {
        return true;
    }
    return false;
}

std::optional<Tool> Setting::getTool(QString const& toolName) const
{
    auto idx = std::find_if(toolList.begin(), toolList.end(), [&](const Tool& tool) {
        return tool.name == toolName;
        });

    if (idx != toolList.end()) {
		return *idx;
    }

    return std::nullopt;
}

std::optional<Tool> Setting::getEngravingTool() const
{
    return getTool(engravingParm.toolName);
}

std::optional<Tool> Setting::getDrillTool() const
{
    return getTool(drillParm.toolName);
}

std::optional<Tool> Setting::getCutTool() const
{
    return getTool(cutParm.toolName);
}

std::vector<Tool> Setting::getDrillList() const 
{
    std::vector<Tool> drillList;
    for (auto t : toolList) {
        if (isDrillTool(t))
        {
            drillList.push_back(t);
        }
    }
    return drillList;
}

Setting::~Setting()
{
	saveSettings();
}