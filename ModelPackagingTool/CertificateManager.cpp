#include "CertificateManager.h"
#include <iostream>
#include <Windows.h>
#include <sstream>
#include <regex>

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
    fs::path signToolPath = sdkPath / L"bin" / L"10.0.22621.0" / L"x64" / L"signtool.exe";
    
    // If the specific version doesn't exist, try finding any version
    if (!fs::exists(signToolPath)) {
        fs::path baseSdkPath(sdkVersion);
        baseSdkPath = baseSdkPath / L"bin";
        
        // Try to find any version folder
        for (const auto& entry : fs::directory_iterator(baseSdkPath)) {
            if (entry.is_directory()) {
                fs::path versionPath = entry.path() / L"x64" / L"signtool.exe";
                if (fs::exists(versionPath)) {
                    signToolPath = versionPath;
                    break;
                }
            }
        }
    }
    
    return signToolPath;
}