#pragma once

#include <string>
#include <filesystem>
#include <wrl.h>

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
    // Create the AppxManifest.xml file if it doesn't exist
    bool CreateAppxManifest(
        const fs::path& sourceFolder,
        const std::wstring& packageName,
        const std::wstring& publisherName);
    
    // Build the MSIX package using MakeAppx.exe
    bool BuildMsixPackage(
        const fs::path& sourceFolder,
        const fs::path& outputMsixPath);
    
    // Find the Windows SDK path
    fs::path FindWindowsSDKPath();
    
    // Create a fallback package using structured storage API
    bool CreateZipBasedPackage(
        const fs::path& sourceFolder,
        const fs::path& outputMsixPath);
        
    // Helper method to add a file to structured storage
    bool AddFileToStorage(
        void* storage,
        const fs::path& sourceFolder,
        const fs::path& relativePath);
        
    // Create MSIX package programmatically using COM interfaces
    bool CreateMsixPackageProgrammatically(
        const fs::path& sourceFolder,
        const fs::path& outputMsixPath);
        
    // Add a file to an MSIX package programmatically
    bool AddFileToMsixPackage(
        void* packageWriter,
        const fs::path& sourceFolder,
        const fs::path& relativePath);
        
    // Create images and other assets needed for the package
    bool CreateDefaultAssets(
        const fs::path& sourceFolder);
};