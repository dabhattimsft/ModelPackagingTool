#include "CertificateManager.h"
#include <iostream>
#include <Windows.h>
#include <sstream>
#include <regex>
#include <algorithm>
#include <wil/resource.h>  // Include WIL resource helpers

bool CertificateManager::GenerateSelfSignedCertificate(
    const fs::path& outputCertPath,
    const std::wstring& publisherName,
    const std::wstring& password)
{
    // Create a safe publisher name for the certificate
    std::wstring safePublisherName = publisherName;
    
    // Replace invalid characters with underscores
    std::wregex invalidChars(L"[^a-zA-Z0-9_.-]");
    safePublisherName = std::regex_replace(safePublisherName, invalidChars, L"_");
    
    // Ensure output directory exists
    fs::path outputDir = outputCertPath.parent_path();
    if (!outputDir.empty() && !fs::exists(outputDir)) {
        try {
            fs::create_directories(outputDir);
        }
        catch (const std::exception& ex) {
            std::cerr << "Error creating certificate output directory: " << ex.what() << std::endl;
            return false;
        }
    }
    
    // Create a PowerShell command to generate a self-signed certificate
    std::wstringstream psCmd;
    
    // Build the New-SelfSignedCertificate command
    psCmd << L"$cert = New-SelfSignedCertificate -Type Custom -Subject \"CN=" << safePublisherName 
          << L"\" -KeyUsage DigitalSignature -FriendlyName \"" << safePublisherName 
          << L" MSIX Signing Certificate\" -CertStoreLocation \"Cert:\\CurrentUser\\My\" "
          << L"-TextExtension @(\"2.5.29.37={text}1.3.6.1.5.5.7.3.3\", \"2.5.29.19={text}\")";
    
    // Export the certificate to a PFX file
    if (!password.empty()) {
        psCmd << L"; $pwd = ConvertTo-SecureString -String \"" << password << L"\" -Force -AsPlainText; "
              << L"Export-PfxCertificate -Cert $cert -FilePath \"" << outputCertPath.wstring() 
              << L"\" -Password $pwd; ";
    }
    else {
        // Use an empty password if none provided
        psCmd << L"; $pwd = ConvertTo-SecureString -String \"\" -Force -AsPlainText; "
              << L"Export-PfxCertificate -Cert $cert -FilePath \"" << outputCertPath.wstring() 
              << L"\" -Password $pwd; ";
    }
    
    // Output the certificate thumbprint for reference
    psCmd << L"Write-Host \"Certificate created with thumbprint: \" $cert.Thumbprint";
    
    // Execute the PowerShell command
    std::wstring cmdLine = L"powershell.exe -Command \"" + psCmd.str() + L"\"";
    
    std::wcout << L"Generating self-signed certificate..." << std::endl;
    
    // Execute the command
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    // Create a non-const copy of the command line
    wchar_t* cmdLineCopy = _wcsdup(cmdLine.c_str());
    
    if (!CreateProcessW(NULL, cmdLineCopy, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmdLineCopy);
        std::wcerr << L"Failed to execute PowerShell, error code: " << GetLastError() << std::endl;
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
    
    if (exitCode != 0) {
        std::wcerr << L"PowerShell failed with exit code: " << exitCode << std::endl;
        return false;
    }
    
    if (fs::exists(outputCertPath)) {
        std::wcout << L"Certificate created successfully at: " << outputCertPath.wstring() << std::endl;
        return true;
    }
    else {
        std::wcerr << L"Certificate creation failed: File not found at " << outputCertPath.wstring() << std::endl;
        return false;
    }
}

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
    
    // Execute the command
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    // Create a non-const copy of the command line
    wchar_t* cmdLineCopy = _wcsdup(cmdLine.c_str());
    
    if (!CreateProcessW(NULL, cmdLineCopy, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmdLineCopy);
        std::wcerr << L"Failed to execute SignTool.exe, error code: " << GetLastError() << std::endl;
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
    
    if (exitCode != 0) {
        std::wcerr << L"SignTool.exe failed with exit code: " << exitCode << std::endl;
        return false;
    }
    
    std::wcout << L"MSIX package signed successfully: " << msixPath.wstring() << std::endl;
    return true;
}

fs::path CertificateManager::FindSignToolPath()
{
    fs::path signToolPath;
    
    try {
        // Use standard Windows API for registry access with error handling
        wil::unique_hkey rootKey;
        HRESULT hr = RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots",
            0,
            KEY_READ,
            &rootKey);
            
        if (FAILED(hr)) {
            std::wcerr << L"Failed to open Windows Kits registry key: 0x" << std::hex << hr << std::endl;
            return signToolPath;
        }
        
        // Get the KitsRoot10 value
        wchar_t kitsRootPath[MAX_PATH] = { 0 };
        DWORD bufferSize = sizeof(kitsRootPath);
        DWORD type = 0;
        
        hr = RegQueryValueExW(
            rootKey.get(),
            L"KitsRoot10",
            nullptr,
            &type,
            reinterpret_cast<LPBYTE>(kitsRootPath),
            &bufferSize);
            
        if (FAILED(hr) || type != REG_SZ) {
            std::wcerr << L"Failed to get KitsRoot10 value: 0x" << std::hex << hr << std::endl;
            return signToolPath;
        }
        
        // Find the latest SDK version
        fs::path binPath = fs::path(kitsRootPath) / L"bin";
        
        if (!fs::exists(binPath) || !fs::is_directory(binPath)) {
            std::wcerr << L"Windows SDK bin path does not exist: " << binPath.wstring() << std::endl;
            return signToolPath;
        }
        
        // Collect all version directories and sort them to find the latest
        std::vector<fs::path> versionPaths;
        for (const auto& entry : fs::directory_iterator(binPath)) {
            if (entry.is_directory()) {
                // Check if this looks like a version number (10.0.xxxxx.x)
                std::wstring dirName = entry.path().filename().wstring();
                if (dirName.find(L"10.0.") == 0) {
                    versionPaths.push_back(entry.path());
                }
            }
        }
        
        if (versionPaths.empty()) {
            std::wcerr << L"No Windows SDK versions found in " << binPath.wstring() << std::endl;
            return signToolPath;
        }
        
        // Sort in descending order to get the latest version first
        std::sort(versionPaths.begin(), versionPaths.end(), std::greater<fs::path>());
        
        // Find the first version that has signtool.exe in the x64 directory
        for (const auto& verPath : versionPaths) {
            fs::path toolPath = verPath / L"x64" / L"signtool.exe";
            if (fs::exists(toolPath)) {
                signToolPath = toolPath;
                std::wcout << L"Found SignTool.exe in: " << signToolPath.wstring() << std::endl;
                break;
            }
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Error finding SignTool.exe path: " << ex.what() << std::endl;
    }
    
    if (signToolPath.empty()) {
        std::wcerr << L"Could not find SignTool.exe in any Windows SDK installation." << std::endl;
    }
    
    return signToolPath;
}