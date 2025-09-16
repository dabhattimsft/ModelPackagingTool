// ModelPackagingTool.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <iostream>
#include <Windows.h>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <shellapi.h>
#include <winrt/base.h>
#include "HuggingFaceDownloader.h"
#include "GitHubDownloader.h"
#include "ModelDownloader.h"
#include "MsixPackager.h"
#include "CommandLineParser.h"

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
        
        // Verify the input folder exists
        fs::path inputFolder(options.inputPath);
        if (!fs::exists(inputFolder) || !fs::is_directory(inputFolder)) {
            std::wcerr << L"Error: Input folder does not exist or is not a directory: " << options.inputPath << std::endl;
            return 1;
        }
        
        // Create the MSIX package
        MsixPackager packager;
        bool success = packager.CreateMsixPackage(inputFolder, options.outputPath);
        
        if (success) {
            std::wcout << L"MSIX package created successfully: " << options.outputPath.wstring() << std::endl;
            return 0;
        }
        else {
            std::wcerr << L"Error: Failed to create MSIX package" << std::endl;
            return 1;
        }
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
        
        if (options.verbose) {
            std::wcout << L"Temporary download folder: " << downloadFolder.wstring() << std::endl;
        }
        
        // Initialize the downloader
        ModelDownloader downloader;
        
        // Start the download
        auto downloadTask = downloader.DownloadModelAsync(
            options.inputPath,
            downloadFolder,
            DownloadProgressCallback
        );
        
        // Wait for the download to complete
        downloadTask.get();
        
        std::wcout << std::endl << L"Download completed successfully!" << std::endl;
        
        // Now package the downloaded files
        MsixPackager packager;
        
        // If the output path is just a directory, create a default MSIX filename
        fs::path outputPath = options.outputPath;
        if (fs::is_directory(outputPath)) {
            // Extract a name from the URI
            std::wstring uriName = L"Model";
            size_t lastSlash = options.inputPath.find_last_of(L"/\\");
            if (lastSlash != std::wstring::npos && lastSlash < options.inputPath.length() - 1) {
                uriName = options.inputPath.substr(lastSlash + 1);
            }
            
            outputPath /= (uriName + L".msix");
        }
        
        bool success = packager.CreateMsixPackage(downloadFolder, outputPath);
        
        // Clean up the temporary download folder
        if (!options.verbose) {
            fs::remove_all(downloadFolder);
        }
        
        if (success) {
            std::wcout << L"MSIX package created successfully: " << outputPath.wstring() << std::endl;
            return 0;
        }
        else {
            std::wcerr << L"Error: Failed to create MSIX package" << std::endl;
            return 1;
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

