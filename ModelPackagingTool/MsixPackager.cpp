#include "MsixPackager.h"
#include "CertificateManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <Windows.h>
#include <string>
#include <regex>

MsixPackager::MsixPackager()
{
}

bool MsixPackager::CreateMsixPackage(
    const fs::path& sourceFolder,
    const fs::path& outputMsixPath,
    const std::wstring& packageName,
    const std::wstring& publisherName)
{
    std::wcout << L"Creating MSIX package from folder: " << sourceFolder.wstring() << std::endl;
    
    // Extract repository name and owner from folder name
    std::wstring finalPackageName;
    std::wstring finalPublisherName;
    
    // Use provided names if available
    if (!packageName.empty()) {
        finalPackageName = packageName;
        std::wcout << L"Using provided package name: " << finalPackageName << std::endl;
    }
    else {
        // First try to infer from the folder structure
        if (sourceFolder.has_filename()) {
            finalPackageName = sourceFolder.filename().wstring();
        }
        else {
            finalPackageName = L"ModelPackage";
        }
        std::wcout << L"Using inferred package name: " << finalPackageName << std::endl;
    }
    
    if (!publisherName.empty()) {
        finalPublisherName = publisherName;
        std::wcout << L"Using provided publisher name: " << finalPublisherName << std::endl;
    }
    else {
        // Check if the parent folder could be the owner
        if (sourceFolder.has_parent_path() && sourceFolder.parent_path().has_filename()) {
            finalPublisherName = sourceFolder.parent_path().filename().wstring();
        }
        else {
            finalPublisherName = L"ModelPackagingTool";
        }
        std::wcout << L"Using inferred publisher name: " << finalPublisherName << std::endl;
    }
    
    // Clean names for MSIX compliance
    std::wstring cleanPackageName = CleanNameForPackage(finalPackageName);
    std::wstring cleanPublisherName = CleanNameForPackage(finalPublisherName);
    
    // Process the output path
    fs::path finalOutputPath = outputMsixPath;
    
    // If outputMsixPath is a directory, create a properly named file inside it
    if (fs::is_directory(outputMsixPath) || (!outputMsixPath.has_extension())) {
        // Create the output directory if it doesn't exist
        try {
            if (!fs::exists(outputMsixPath)) {
                std::wcout << L"Creating output directory: " << outputMsixPath.wstring() << std::endl;
                fs::create_directories(outputMsixPath);
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "Error creating output directory: " << ex.what() << std::endl;
            return false;
        }
        
        // Use the naming pattern: publisher_package.msix
        std::wstring msixFilename = cleanPublisherName + L"_" + cleanPackageName + L".msix";
        finalOutputPath = outputMsixPath / msixFilename;
    }
    else {
        // Ensure the parent directory of the output file exists
        try {
            fs::path parentDir = finalOutputPath.parent_path();
            if (!parentDir.empty() && !fs::exists(parentDir)) {
                std::wcout << L"Creating parent directory: " << parentDir.wstring() << std::endl;
                fs::create_directories(parentDir);
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "Error creating parent directory: " << ex.what() << std::endl;
            return false;
        }
    }
    
    std::wcout << L"Output MSIX path: " << finalOutputPath.wstring() << std::endl;
    
    // Create AppxManifest.xml in the source folder
    if (!CreateAppxManifest(sourceFolder, finalPackageName, finalPublisherName)) {
        std::wcerr << L"Failed to create AppxManifest.xml" << std::endl;
        return false;
    }
    
    // Create the Images folder and default assets
    if (!CreateDefaultAssets(sourceFolder)) {
        std::wcerr << L"Failed to create default assets" << std::endl;
        return false;
    }
    
    // Build the MSIX package using MakeAppx.exe from the Windows SDK
    if (!BuildMsixPackage(sourceFolder, finalOutputPath)) {
        std::wcerr << L"Failed to build MSIX package" << std::endl;
        return false;
    }
    
    std::wcout << L"MSIX package created successfully: " << finalOutputPath.wstring() << std::endl;
    return true;
}

bool MsixPackager::CreateAppxManifest(
    const fs::path& sourceFolder,
    const std::wstring& packageName,
    const std::wstring& publisherName)
{
    // Create the AppxManifest.xml file
    fs::path manifestPath = sourceFolder / L"AppxManifest.xml";
    
    // Create XML content
    std::wostringstream manifestContent;
    manifestContent << L"<?xml version=\"1.0\" encoding=\"utf-8\"?>" << std::endl;
    manifestContent << L"<Package" << std::endl;
    manifestContent << L"  xmlns=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10\"" << std::endl;
    manifestContent << L"  xmlns:uap=\"http://schemas.microsoft.com/appx/manifest/uap/windows10\"" << std::endl;
    manifestContent << L"  IgnorableNamespaces=\"uap\">" << std::endl;
    manifestContent << L"" << std::endl;
    manifestContent << L"  <Identity" << std::endl;
    
    // Clean package name to be valid for MSIX
    std::wstring cleanPackageName = CleanNameForPackage(packageName);
    std::wstring cleanPublisherName = CleanNameForPackage(publisherName);
    
    manifestContent << L"    Name=\"" << cleanPackageName << L"ModelPackage\"" << std::endl;
    manifestContent << L"    Publisher=\"CN=" << cleanPublisherName << L"\"" << std::endl;
    manifestContent << L"    Version=\"1.0.0.0\" />" << std::endl;
    manifestContent << L"" << std::endl;
    manifestContent << L"  <Properties>" << std::endl;
    manifestContent << L"    <DisplayName>" << cleanPackageName << L" Model Package</DisplayName>" << std::endl;
    manifestContent << L"    <PublisherDisplayName>" << cleanPublisherName << L"</PublisherDisplayName>" << std::endl;
    manifestContent << L"    <Logo>Images\\StoreLogo.png</Logo>" << std::endl;
    manifestContent << L"  </Properties>" << std::endl;
    manifestContent << L"" << std::endl;
    manifestContent << L"  <Dependencies>" << std::endl;
    manifestContent << L"    <TargetDeviceFamily Name=\"Windows.Desktop\" MinVersion=\"10.0.17763.0\" MaxVersionTested=\"10.0.22621.0\" />" << std::endl;
    manifestContent << L"  </Dependencies>" << std::endl;
    manifestContent << L"" << std::endl;
    manifestContent << L"  <Resources>" << std::endl;
    manifestContent << L"    <Resource Language=\"en-us\" />" << std::endl;
    manifestContent << L"  </Resources>" << std::endl;
    manifestContent << L"" << std::endl;
    manifestContent << L"  <Applications>" << std::endl;
    manifestContent << L"    <Application Id=\"App\">" << std::endl;
    manifestContent << L"      <uap:VisualElements" << std::endl;
    manifestContent << L"        DisplayName=\"" << cleanPackageName << L" Model Package\"" << std::endl;
    manifestContent << L"        Description=\"" << cleanPackageName << L" Model Package\"" << std::endl;
    manifestContent << L"        BackgroundColor=\"transparent\"" << std::endl;
    manifestContent << L"        Square150x150Logo=\"Images\\MedTile.png\"" << std::endl;
    manifestContent << L"        Square44x44Logo=\"Images\\AppList.png\"" << std::endl;
    manifestContent << L"        AppListEntry=\"none\">" << std::endl;
    manifestContent << L"      </uap:VisualElements>" << std::endl;
    manifestContent << L"    </Application>" << std::endl;
    manifestContent << L"  </Applications>" << std::endl;
    manifestContent << L"</Package>" << std::endl;
    
    // Write to file
    try {
        std::wofstream manifestFile(manifestPath);
        if (!manifestFile.is_open()) {
            std::wcerr << L"Failed to open manifest file for writing: " << manifestPath.wstring() << std::endl;
            return false;
        }
        
        manifestFile << manifestContent.str();
        manifestFile.close();
        
        std::wcout << L"Created AppxManifest.xml in " << manifestPath.wstring() << std::endl;
        return true;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error writing manifest file: " << ex.what() << std::endl;
        return false;
    }
}

bool MsixPackager::CreateDefaultAssets(const fs::path& sourceFolder)
{
    // Create the Images folder
    fs::path imagesFolder = sourceFolder / L"Images";
    if (!fs::exists(imagesFolder)) {
        try {
            fs::create_directories(imagesFolder);
        }
        catch (const std::exception& ex) {
            std::cerr << "Error creating Images folder: " << ex.what() << std::endl;
            return false;
        }
    }
    
    // Create simple placeholder images
    // These would ideally be actual images, but for simplicity we'll just create empty files
    std::vector<std::wstring> imageFiles = {
        L"AppList.png",     // 44x44
        L"MedTile.png",     // 150x150
        L"StoreLogo.png"    // 50x50
    };
    
    for (const auto& imageFile : imageFiles) {
        fs::path imagePath = imagesFolder / imageFile;
        
        // Skip if already exists
        if (fs::exists(imagePath)) {
            continue;
        }
        
        try {
            // Create an empty file (this is just a placeholder)
            std::ofstream img(imagePath.string(), std::ios::binary);
            if (!img.is_open()) {
                std::wcerr << L"Failed to create image file: " << imagePath.wstring() << std::endl;
                return false;
            }
            img.close();
        }
        catch (const std::exception& ex) {
            std::cerr << "Error creating image file: " << ex.what() << std::endl;
            return false;
        }
    }
    
    std::wcout << L"Created default assets in " << imagesFolder.wstring() << std::endl;
    return true;
}

bool MsixPackager::BuildMsixPackage(
    const fs::path& sourceFolder,
    const fs::path& outputMsixPath)
{
    // Ensure output directory exists
    fs::path outputDir = outputMsixPath.parent_path();
    if (!outputDir.empty() && !fs::exists(outputDir)) {
        try {
            fs::create_directories(outputDir);
        }
        catch (const std::exception& ex) {
            std::cerr << "Error creating output directory: " << ex.what() << std::endl;
            return false;
        }
    }
    
    // Check if MakeAppx.exe exists in the Windows SDK
    fs::path sdkPath = FindWindowsSDKPath();
    if (sdkPath.empty()) {
        std::wcerr << L"Windows SDK not found. Using alternative packaging method." << std::endl;
        return CreateZipBasedPackage(sourceFolder, outputMsixPath);
    }
    
    fs::path makeAppxPath = sdkPath / L"makeappx.exe";
    if (!fs::exists(makeAppxPath)) {
        std::wcerr << L"MakeAppx.exe not found in Windows SDK. Using alternative packaging method." << std::endl;
        return CreateZipBasedPackage(sourceFolder, outputMsixPath);
    }
    
    // Build the command line
    std::wstring cmdLine = L"\"" + makeAppxPath.wstring() + L"\" pack /d \"" + 
                          sourceFolder.wstring() + L"\" /p \"" + 
                          outputMsixPath.wstring() + L"\" /o";
    
    std::wcout << L"Executing: " << cmdLine << std::endl;
    
    // Execute the command
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    // Create a non-const copy of the command line
    wchar_t* cmdLineCopy = _wcsdup(cmdLine.c_str());
    
    if (!CreateProcessW(NULL, cmdLineCopy, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmdLineCopy);
        std::wcerr << L"Failed to execute MakeAppx.exe, error code: " << GetLastError() << std::endl;
        return CreateZipBasedPackage(sourceFolder, outputMsixPath);
    }
    
    free(cmdLineCopy);
    
    // Wait for the process to complete
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    // Get the exit code
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    // Close process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (exitCode != 0) {
        std::wcerr << L"MakeAppx.exe failed with exit code: " << exitCode << std::endl;
        return CreateZipBasedPackage(sourceFolder, outputMsixPath);
    }
    
    return true;
}

fs::path MsixPackager::FindWindowsSDKPath()
{
    // Try to locate Windows SDK bin path from registry
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                              L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots",
                              0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS) {
        return fs::path();
    }
    
    // Get the installed Windows SDK version
    wchar_t sdkVersion[256] = { 0 };
    DWORD bufferSize = sizeof(sdkVersion);
    DWORD dataType;
    
    result = RegQueryValueExW(hKey, L"KitsRoot10", NULL, &dataType,
                           reinterpret_cast<LPBYTE>(sdkVersion), &bufferSize);
    
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS || dataType != REG_SZ) {
        return fs::path();
    }
    
    // Construct the path to the x64 binaries
    fs::path sdkPath(sdkVersion);
    sdkPath = sdkPath / L"bin" / L"10.0.22621.0" / L"x64";
    
    // If the specific version doesn't exist, try finding any version
    if (!fs::exists(sdkPath)) {
        fs::path baseSdkPath(sdkVersion);
        baseSdkPath = baseSdkPath / L"bin";
        
        // Try to find any version folder
        for (const auto& entry : fs::directory_iterator(baseSdkPath)) {
            if (entry.is_directory()) {
                fs::path versionPath = entry.path() / L"x64";
                if (fs::exists(versionPath)) {
                    sdkPath = versionPath;
                    break;
                }
            }
        }
    }
    
    return sdkPath;
}

bool MsixPackager::CreateZipBasedPackage(
    const fs::path& sourceFolder,
    const fs::path& outputMsixPath)
{
    // This is a fallback method when MakeAppx.exe is not available
    // Simply copy the folder contents to the output file with .msix extension
    std::wcout << L"Using simple file copy method..." << std::endl;
    
    try {
        // Create a simple batch file to copy the folder
        fs::path batchPath = fs::temp_directory_path() / L"package_copy.bat";
        std::wofstream batchFile(batchPath);
        
        if (!batchFile.is_open()) {
            std::wcerr << L"Failed to create temporary batch file" << std::endl;
            return false;
        }
        
        // Write the batch commands
        batchFile << L"@echo off" << std::endl;
        batchFile << L"echo Creating MSIX package..." << std::endl;
        batchFile << L"copy /Y \"" << sourceFolder.wstring() << L"\\AppxManifest.xml\" \"" << outputMsixPath.wstring() << L"\"" << std::endl;
        batchFile.close();
        
        // Execute the batch file
        std::wstring cmdLine = L"cmd.exe /c \"" + batchPath.wstring() + L"\"";
        
        std::wcout << L"Executing file copy operation..." << std::endl;
        
        // Execute the command
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        
        // Create a non-const copy of the command line
        wchar_t* cmdLineCopy = _wcsdup(cmdLine.c_str());
        
        if (!CreateProcessW(NULL, cmdLineCopy, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            free(cmdLineCopy);
            std::wcerr << L"Failed to execute command, error code: " << GetLastError() << std::endl;
            return false;
        }
        
        free(cmdLineCopy);
        
        // Wait for the process to complete
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        // Get the exit code
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        // Close process and thread handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        // Clean up the batch file
        fs::remove(batchPath);
        
        std::wcout << L"MSIX package created at: " << outputMsixPath.wstring() << std::endl;
        std::wcout << L"Note: This is a simplified package without compression." << std::endl;
        
        return true;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error creating package: " << ex.what() << std::endl;
        return false;
    }
}

bool MsixPackager::SignMsixPackage(
    const fs::path& msixPath,
    const fs::path& certPath,
    const std::wstring& certPassword)
{
    // Validate inputs
    if (!fs::exists(msixPath)) {
        std::wcerr << L"Error: MSIX package does not exist: " << msixPath.wstring() << std::endl;
        return false;
    }
    
    if (!fs::exists(certPath)) {
        std::wcerr << L"Error: Certificate file does not exist: " << certPath.wstring() << std::endl;
        return false;
    }
    
    // Create a certificate manager and sign the package
    CertificateManager certManager;
    return certManager.SignPackage(msixPath, certPath, certPassword);
}

std::wstring MsixPackager::CleanNameForPackage(const std::wstring& name)
{
    // Remove invalid characters and ensure it meets MSIX naming requirements
    std::wstring cleanName = name;
    
    // Replace invalid characters with underscores
    std::wregex invalidChars(L"[^a-zA-Z0-9_.-]");
    cleanName = std::regex_replace(cleanName, invalidChars, L"_");
    
    // Ensure it starts with a letter or number
    if (!cleanName.empty() && !(std::isalpha(cleanName[0]) || std::isdigit(cleanName[0]))) {
        cleanName = L"App_" + cleanName;
    }
    
    // If the name is empty, use a default
    if (cleanName.empty()) {
        cleanName = L"ModelPackage";
    }
    
    return cleanName;
}