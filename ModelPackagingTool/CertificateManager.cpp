#include "CertificateManager.h"
#include <iostream>
#include <Windows.h>
#include <sstream>
#include <regex>
#include <filesystem>
#include <memory>

// Include WIL for RAII resource management
#include <wil/resource.h>

bool CertificateManager::SignPackage(
    const fs::path& msixPath,
    const fs::path& certPath,
    const std::wstring& password)
{
    // Find SignTool.exe
    fs::path signToolPath = FindSignToolPath();
    if (signToolPath.empty() || !fs::exists(signToolPath)) {
        std::wcerr << L"SignTool.exe not found in Windows SDK" << std::endl;
        return false;
    }
    
    // Build the SignTool command
    std::wstringstream cmdStream;
    cmdStream << L"\"" << signToolPath.wstring() << L"\" sign /fd SHA256";
    
    // Add password if provided
    if (!password.empty()) {
        cmdStream << L" /p " << password;
    }
    
    // Add certificate path and MSIX path
    cmdStream << L" /f \"" << certPath.wstring() << L"\" \"" << msixPath.wstring() << L"\"";
    
    std::wstring cmdLine = cmdStream.str();
    
    std::wcout << L"Signing MSIX package..." << std::endl;
    std::wcout << L"Executing: " << cmdLine << std::endl;
    
    // Execute the command using WIL's RAII wrappers
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    // Use wil::unique_process_information for PROCESS_INFORMATION cleanup
    wil::unique_process_information processInfo;
    
    // Use wil::make_cotaskmem_string for command line management
    auto cmdLineCopy = wil::make_cotaskmem_string(cmdLine.c_str());
    if (!cmdLineCopy) {
        std::wcerr << L"Failed to allocate memory for command line" << std::endl;
        return false;
    }
    
    if (!CreateProcessW(NULL, cmdLineCopy.get(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &processInfo)) {
        std::wcerr << L"Failed to execute SignTool.exe, error code: " << GetLastError() << std::endl;
        return false;
    }
    
    // Wait for the process to complete
    WaitForSingleObject(processInfo.hProcess, INFINITE);
    
    // Get the exit code
    DWORD exitCode = 0;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    
    if (exitCode != 0) {
        std::wcerr << L"SignTool.exe failed with exit code: " << exitCode << std::endl;
        return false;
    }
    
    std::wcout << L"MSIX package signed successfully: " << msixPath.wstring() << std::endl;
    return true;
}

fs::path CertificateManager::FindSignToolPath()
{
    // Try to locate Windows SDK bin path from registry using WIL's unique_hkey
    wil::unique_hkey regKey;
    LSTATUS result = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots",
        0,
        KEY_READ,
        &regKey);
    
    if (result != ERROR_SUCCESS) {
        std::wcerr << L"Failed to open Windows Kits registry key, error: " << result << std::endl;
        return fs::path();
    }
    
    // Get the installed Windows SDK path
    wchar_t sdkPath[MAX_PATH] = { 0 };
    DWORD bufferSize = sizeof(sdkPath);
    DWORD dataType;
    
    result = RegQueryValueExW(
        regKey.get(),
        L"KitsRoot10",
        NULL,
        &dataType,
        reinterpret_cast<LPBYTE>(sdkPath),
        &bufferSize);
    
    if (result != ERROR_SUCCESS || dataType != REG_SZ) {
        std::wcerr << L"Failed to read KitsRoot10 registry value, error: " << result << std::endl;
        return fs::path();
    }
    
    // Path to the bin directory
    fs::path baseBinPath(sdkPath);
    baseBinPath = baseBinPath / L"bin";
    
    // Check if bin directory exists
    if (!fs::exists(baseBinPath)) {
        std::wcerr << L"Windows SDK bin directory not found at: " << baseBinPath.wstring() << std::endl;
        return fs::path();
    }
    
    // Find all version directories in the bin folder
    std::vector<fs::path> versionPaths;
    try {
        for (const auto& entry : fs::directory_iterator(baseBinPath)) {
            if (entry.is_directory()) {
                versionPaths.push_back(entry.path());
            }
        }
    }
    catch (const std::exception& ex) {
        std::wcerr << L"Error enumerating SDK bin directories: " << ex.what() << std::endl;
        return fs::path();
    }
    
    // Sort version paths in descending order to use the newest version first
    std::sort(versionPaths.begin(), versionPaths.end(), std::greater<fs::path>());
    
    // Try to find signtool.exe in x64 or x86 folders
    for (const auto& versionPath : versionPaths) {
        // Try x64 first
        fs::path x64Path = versionPath / L"x64" / L"signtool.exe";
        if (fs::exists(x64Path)) {
            std::wcout << L"Found SignTool.exe at: " << x64Path.wstring() << std::endl;
            return x64Path;
        }
        
        // Then try x86
        fs::path x86Path = versionPath / L"x86" / L"signtool.exe";
        if (fs::exists(x86Path)) {
            std::wcout << L"Found SignTool.exe at: " << x86Path.wstring() << std::endl;
            return x86Path;
        }
    }
    
    // If we got here, we couldn't find SignTool.exe
    std::wcerr << L"SignTool.exe not found in any Windows SDK version" << std::endl;
    return fs::path();
}