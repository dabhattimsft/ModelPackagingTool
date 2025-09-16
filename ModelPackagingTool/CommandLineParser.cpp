#include "CommandLineParser.h"
#include <iostream>

CommandLineOptions CommandLineParser::Parse(int argc, wchar_t* argv[])
{
    CommandLineOptions options;
    
    // Need at least one argument (command)
    if (argc < 2) {
        options.command = CommandLineOptions::Command::ShowHelp;
        return options;
    }
    
    std::wstring command = argv[1];
    
    if (command == L"/package" || command == L"-package") {
        options.command = CommandLineOptions::Command::Package;
        
        // Need a path to the folder
        if (argc < 3) {
            std::wcerr << L"Error: Missing folder path for /package command" << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
        
        options.inputPath = argv[2];
        
        // Optional output path
        if (argc > 3) {
            options.outputPath = argv[3];
        }
        else {
            // Default output path: current directory with repo name as file name
            fs::path inputFolder(options.inputPath);
            options.outputPath = fs::current_path() / (inputFolder.filename().wstring() + L".msix");
        }
    }
    else if (command == L"/downloadAndPackage" || command == L"-downloadAndPackage") {
        options.command = CommandLineOptions::Command::DownloadAndPackage;
        
        // Need a URI
        if (argc < 3) {
            std::wcerr << L"Error: Missing URI for /downloadAndPackage command" << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
        
        options.inputPath = argv[2];
        
        // Optional output path
        if (argc > 3) {
            options.outputPath = argv[3];
        }
        else {
            // Default output path will be determined based on the downloaded content
            options.outputPath = fs::current_path();
        }
    }
    else if (command == L"/help" || command == L"-help" || command == L"/?" || command == L"-?") {
        options.command = CommandLineOptions::Command::ShowHelp;
    }
    else {
        std::wcerr << L"Error: Unknown command: " << command << std::endl;
        options.command = CommandLineOptions::Command::ShowHelp;
    }
    
    // Check for additional flags
    for (int i = 3; i < argc; i++) {
        std::wstring arg = argv[i];
        if (arg == L"/verbose" || arg == L"-verbose") {
            options.verbose = true;
        }
    }
    
    return options;
}

void CommandLineParser::ShowUsage()
{
    std::wcout << L"ModelPackagingTool - Tool for packaging model files into MSIX packages" << std::endl;
    std::wcout << L"Usage:" << std::endl;
    std::wcout << L"  ModelPackagingTool /package <path-to-folder> [output-path]" << std::endl;
    std::wcout << L"  ModelPackagingTool /downloadAndPackage <uri> [output-path]" << std::endl;
    std::wcout << L"  ModelPackagingTool /help" << std::endl;
    std::wcout << std::endl;
    std::wcout << L"Commands:" << std::endl;
    std::wcout << L"  /package              Package a local folder into an MSIX package" << std::endl;
    std::wcout << L"  /downloadAndPackage   Download model files from a URI and package them" << std::endl;
    std::wcout << L"  /help                 Show this help information" << std::endl;
    std::wcout << std::endl;
    std::wcout << L"Options:" << std::endl;
    std::wcout << L"  /verbose              Enable verbose output" << std::endl;
    std::wcout << std::endl;
    std::wcout << L"Examples:" << std::endl;
    std::wcout << L"  ModelPackagingTool /package C:\\Models\\MyModel" << std::endl;
    std::wcout << L"  ModelPackagingTool /downloadAndPackage https://huggingface.co/openai-community/gpt2" << std::endl;
    std::wcout << L"  ModelPackagingTool /downloadAndPackage https://github.com/microsoft/onnxruntime" << std::endl;
}