#pragma once
#include <string>
#include "json/json.h"

class CConfig {
public:
    CConfig();
    void LoadConfig(std::string sConfigPath);
    bool SaveConfig();

    enum Mode {
        DOWNLOAD_CIA, 
        INSTALL_CIA, 
        INSTALL_TICKET
    };

    Mode GetMode();
    void SetMode(Mode mode);
    std::string GetRegionFilter();
    void SetRegionFilter(std::string region);

private:
    std::string m_sConfigPath;

    // Config options
    Mode m_eMode;
    std::string m_sRegionFilter;

    Json::Value m_Json;
};
