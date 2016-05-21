#include <string>
#include <fstream>
#include "config.h"
#include "json/json.h"

using namespace std;

CConfig::CConfig()
{
    // Empty
}

void CConfig::LoadConfig(string sConfigPath)
{
    // Store the config path so we can save it later
    m_sConfigPath = sConfigPath;

    std::ifstream ifs(m_sConfigPath);
    if (ifs.is_open()) {
        ifs >> m_Json;
    }

    // Mode defaults to Install CIA
    m_eMode = (Mode)m_Json.get("Mode", (int)Mode::INSTALL_CIA).asInt();
    m_sRegionFilter = m_Json.get("RegionFilter", "off").asString();
}

bool CConfig::SaveConfig()
{
    // Update the config
    m_Json["Mode"] = (int)m_eMode;
    m_Json["RegionFilter"] = m_sRegionFilter;

    // Write to file
    std::ofstream ofs(m_sConfigPath, std::ofstream::trunc);
    if (ofs.is_open())
    {
        ofs << m_Json;
        return true;
    }

    return false;
}

CConfig::Mode CConfig::GetMode()
{
    return m_eMode;
}

void CConfig::SetMode(CConfig::Mode mode)
{
    m_eMode = mode;
    SaveConfig();
}

std::string CConfig::GetRegionFilter()
{
    return m_sRegionFilter;
}

void CConfig::SetRegionFilter(std::string region)
{
    m_sRegionFilter = region;
    SaveConfig();
}