// ModelPackagingTool.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <iostream>
#include <Windows.h>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <shellapi.h>
#include <winrt/base.h>
#include <regex>
#include "HuggingFaceDownloader.h"
#include "GitHubDownloader.h"
#include "ModelDownloader.h"
#include "MsixPackager.h"
#include "CommandLineParser.h"
#include "CertificateManager.h"

// Progress callback for the downloader
void DownloadProgressCallback(const std::wstring& fileName, uint64_t bytesReceived, uint64_t totalBytes)
{
    if (totalBytes > 0) {
        double percentage = static_cast<double>(bytesReceived) / totalBytes * 100.0;
        std::wcout << L"\rDownloading " << fileName << L": " << bytesReceived << L"/" << totalBytes 
                  << L" bytes (" << static_cast<int>(percentage) << L"%)";
    }
    else {
        std::wcout << L"\rDownloading " << fileName << L": " << bytesReceived << L" bytes";
    }
}

// Execute the Package command
int ExecutePackageCommand(const CommandLineOptions& options)
{
    try {
        std::wcout << L"Packaging folder: " << options.inputPath << std::endl;
        
        // Create the MSIX package
        MsixPackager packager;
        bool success = packager.CreateMsixPackage(
            options.inputPath, 
            options.outputPath,
            options.packageName,
            options.publisherName
        );
        
        if (!success) {
            std::wcerr << L"Error: Failed to create MSIX package" << std::endl;
            return 1;
        }
        
        // If signing is requested, sign the package
        if (options.shouldSign) {
            fs::path msixPath;
            
            // Determine the MSIX file path
            if (fs::is_directory(options.outputPath) || !options.outputPath.has_extension()) {
                // The file will be in the output directory with a name based on publisher and package
                std::wstring cleanPackageName = packager.CleanNameForPackage(options.packageName);
                std::wstring cleanPublisherName = packager.CleanNameForPackage(options.publisherName);
                std::wstring msixFilename = cleanPublisherName + L"_" + cleanPackageName + L".msix";
                msixPath = options.outputPath / msixFilename;
            }
            else {
                // The file is already specified with full path
                msixPath = options.outputPath;
            }
            
            std::wcout << L"Signing MSIX package: " << msixPath.wstring() << std::endl;
            
            bool signSuccess = packager.SignMsixPackage(
                msixPath,
                options.certPath,
                options.certPassword
            );
            
            if (!signSuccess) {
                std::wcerr << L"Error: Failed to sign MSIX package" << std::endl;
                return 1;
            }
            
            std::wcout << L"MSIX package signed successfully" << std::endl;
        }
        
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}

// Execute the DownloadAndPackage command
int ExecuteDownloadAndPackageCommand(const CommandLineOptions& options)
{
    try {
        std::wcout << L"Downloading and packaging from URI: " << options.inputPath << std::endl;
        
        // Create a temporary download folder
        fs::path downloadFolder = fs::temp_directory_path() / L"ModelPackagingTool_Download";
        if (fs::exists(downloadFolder)) {
            fs::remove_all(downloadFolder);
        }
        fs::create_directories(downloadFolder);
        
        // Always print the download folder, it's useful information
        std::wcout << L"Files will be downloaded to: " << downloadFolder.wstring() << std::endl;
        
        // Initialize the downloader
        ModelDownloader downloader;
        
        // Parse the URI to extract repository information for naming inference
        RepositoryInfo repoInfo = downloader.ParseUri(options.inputPath);
        
        // Show inference information if name or publisher not provided
        std::wstring finalPackageName = options.packageName;
        std::wstring finalPublisherName = options.publisherName;
        
        if (finalPackageName.empty()) {
            finalPackageName = repoInfo.name;
            std::wcout << L"Package name will be inferred from repository: " << finalPackageName << std::endl;
        }
        
        if (finalPublisherName.empty()) {
            finalPublisherName = repoInfo.owner;
            std::wcout << L"Publisher name will be inferred from repository owner: " << finalPublisherName << std::endl;
        }
        
        // Start the download
        auto downloadTask = downloader.DownloadModelAsync(
            options.inputPath,
            downloadFolder,
            DownloadProgressCallback
        );
        
        // Wait for the download to complete
        downloadTask.get();
        
        std::wcout << std::endl << L"Download completed successfully!" << std::endl;
        std::wcout << L"Downloaded files are in: " << downloadFolder.wstring() << std::endl;
        
        // Now package the downloaded files
        MsixPackager packager;
        
        bool success = packager.CreateMsixPackage(
            downloadFolder, 
            options.outputPath,
            finalPackageName,
            finalPublisherName
        );
        
        if (!success) {
            std::wcerr << L"Error: Failed to create MSIX package" << std::endl;
            return 1;
        }
        
        // If signing is requested, sign the package
        if (options.shouldSign) {
            fs::path msixPath;
            
            // Determine the MSIX file path
            if (fs::is_directory(options.outputPath) || !options.outputPath.has_extension()) {
                // The file will be in the output directory with a name based on publisher and package
                std::wstring cleanPackageName = packager.CleanNameForPackage(finalPackageName);
                std::wstring cleanPublisherName = packager.CleanNameForPackage(finalPublisherName);
                std::wstring msixFilename = cleanPublisherName + L"_" + cleanPackageName + L".msix";
                msixPath = options.outputPath / msixFilename;
            }
            else {
                // The file is already specified with full path
                msixPath = options.outputPath;
            }
            
            std::wcout << L"Signing MSIX package: " << msixPath.wstring() << std::endl;
            
            bool signSuccess = packager.SignMsixPackage(
                msixPath,
                options.certPath,
                options.certPassword
            );
            
            if (!signSuccess) {
                std::wcerr << L"Error: Failed to sign MSIX package" << std::endl;
                return 1;
            }
            
            std::wcout << L"MSIX package signed successfully" << std::endl;
        }
        
        // Clean up the temporary download folder
        if (!options.verbose) {
            std::wcout << L"Cleaning up temporary download folder..." << std::endl;
            fs::remove_all(downloadFolder);
        }
        else {
            std::wcout << L"Temporary download folder preserved at: " << downloadFolder.wstring() << std::endl;
        }
        
        return 0;
    }
    catch (const winrt::hresult_error& ex) {
        std::wcerr << L"Error: " << ex.message().c_str() << std::endl;
        return 1;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}

// Execute the GenerateCert command
int ExecuteGenerateCertCommand(const CommandLineOptions& options)
{
    try {
        std::wcout << L"Generating self-signed certificate..." << std::endl;
        std::wcout << L"Publisher: " << options.publisherName << std::endl;
        std::wcout << L"Output path: " << options.outputPath.wstring() << std::endl;
        
        if (!options.certPassword.empty()) {
            std::wcout << L"Using provided password for certificate" << std::endl;
        }
        
        // Generate the certificate
        CertificateManager certManager;
        bool success = certManager.GenerateSelfSignedCertificate(
            options.outputPath,
            options.publisherName,
            options.certPassword
        );
        
        if (success) {
            std::wcout << L"Certificate generated successfully at: " << options.outputPath.wstring() << std::endl;
            std::wcout << L"You can use this certificate with the /sign option when packaging." << std::endl;
            return 0;
        }
        else {
            std::wcerr << L"Error: Failed to generate certificate" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}

int wmain(int argc, wchar_t* argv[])
{
    try {
        // Initialize WinRT
        winrt::init_apartment();
        
        // Parse command-line arguments
        CommandLineOptions options = CommandLineParser::Parse(argc, argv);
        
        // Execute the appropriate command
        switch (options.command) {
            case CommandLineOptions::Command::Package:
                return ExecutePackageCommand(options);
                
            case CommandLineOptions::Command::DownloadAndPackage:
                return ExecuteDownloadAndPackageCommand(options);
                
            case CommandLineOptions::Command::GenerateCert:
                return ExecuteGenerateCertCommand(options);
                
            case CommandLineOptions::Command::ShowHelp:
            default:
                CommandLineParser::ShowUsage();
                return options.command == CommandLineOptions::Command::ShowHelp ? 0 : 1;
        }
    }
    catch (const winrt::hresult_error& ex) {
        std::wcerr << L"Error: " << ex.message().c_str() << std::endl;
        return 1;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    
    return 0;
}

