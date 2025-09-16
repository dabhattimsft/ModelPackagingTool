#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

class MsixPackager
{
public:
    MsixPackager();
    ~MsixPackager() = default;

    // Create an MSIX package from a folder
    bool CreateMsixPackage(
        const fs::path& sourceFolder,
        const fs::path& outputMsixPath);

private:
    // Add MSIX-specific implementation details here
};