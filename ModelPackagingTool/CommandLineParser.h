#pragma once

#include <string>
#include <map>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// Structure to hold command-line options
struct CommandLineOptions
{
    enum class Command
    {
        None,
        Package,
        DownloadAndPackage,
        GenerateCert,
        ShowHelp
    };
    
    Command command = Command::None;
    std::wstring inputPath;         // Folder path or URI
    fs::path outputPath;            // Output MSIX path
    bool verbose = false;           // Verbose output
    std::wstring packageName;       // Custom package name
    std::wstring publisherName;     // Custom publisher name
    
    // Certificate options
    fs::path certPath;              // Path to certificate file for signing
    std::wstring certPassword;      // Password for certificate
    bool shouldSign = false;        // Whether to sign the package
};

class CommandLineParser
{
public:
    // Parse command-line arguments
    static CommandLineOptions Parse(int argc, wchar_t* argv[]);
    
    // Show usage information
    static void ShowUsage();
};