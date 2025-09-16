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
        ShowHelp
    };
    
    Command command = Command::None;
    std::wstring inputPath;         // Folder path or URI
    fs::path outputPath;            // Output MSIX path
    bool verbose = false;           // Verbose output
};

class CommandLineParser
{
public:
    // Parse command-line arguments
    static CommandLineOptions Parse(int argc, wchar_t* argv[]);
    
    // Show usage information
    static void ShowUsage();
};