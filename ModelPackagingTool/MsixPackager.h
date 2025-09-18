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
        const fs::path& outputMsixPath,
        const std::wstring& packageName = L"",
        const std::wstring& publisherName = L"" );

    // Sign an MSIX package
    bool SignMsixPackage(
        const fs::path& msixPath,
        const fs::path& certPath,
        const std::wstring& certPassword = L"");
        
    // Clean a name for use in the package manifest
    std::wstring CleanNameForPackage(const std::wstring& name);

private:
    // Create the AppxManifest.xml file
    bool CreateAppxManifest(
        const fs::path& sourceFolder,
        const std::wstring& packageName,
        const std::wstring& publisherName);
    
    // Create default image assets
    bool CreateDefaultAssets(const fs::path& sourceFolder);
    
    // Build the MSIX package using MakeAppx.exe
    bool BuildMsixPackage(
        const fs::path& sourceFolder,
        const fs::path& outputMsixPath);
    
    // Find the Windows SDK path
    fs::path FindWindowsSDKPath();
    
    // Create a zip-based package (fallback method)
    bool CreateZipBasedPackage(
        const fs::path& sourceFolder,
        const fs::path& outputMsixPath);
};