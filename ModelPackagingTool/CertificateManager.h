#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

class CertificateManager
{
public:
    CertificateManager() = default;
    ~CertificateManager() = default;

    // Sign an MSIX package with a certificate
    bool SignPackage(
        const fs::path& msixPath,
        const fs::path& certPath,
        const std::wstring& password = L"");

private:
    // Find the SignTool.exe in the Windows SDK
    fs::path FindSignToolPath();
};