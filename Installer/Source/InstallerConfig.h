#pragma once

// The names and the version: CMakeLists.txt is the one place that states them, and the build
// writes them into this header (see Installer/InstallerBuildConfig.h.in).
#include <InstallerBuildConfig.h>

namespace daaura::config
{
    inline constexpr const char* productName = DAAURA_PRODUCT_NAME;       // "D.A AURA"
    inline constexpr const char* companyName = DAAURA_COMPANY_NAME;       // "D.A Production"
    inline constexpr const char* version     = DAAURA_VERSION_STRING;     // "1.2.0"
}
