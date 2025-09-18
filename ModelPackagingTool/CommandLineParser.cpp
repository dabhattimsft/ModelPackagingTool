#include "CommandLineParser.h"
#include <iostream>

CommandLineOptions CommandLineParser::Parse(int argc, wchar_t* argv[])
{
    CommandLineOptions options;
    
    // Check if we have at least a command
    if (argc < 2) {
        options.command = CommandLineOptions::Command::ShowHelp;
        return options;
    }
    
    std::wstring command = argv[1];
    
    // Parse the command
    if (command == L"/pack") {
        options.command = CommandLineOptions::Command::Package;
        
        // Check if we have the required input path
        if (argc < 3) {
            std::wcerr << L"Error: Missing input folder path" << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
        
        options.inputPath = argv[2];
        
        // Parse all arguments to check for required options
        bool hasName = false;
        bool hasPublisher = false;
        bool hasOutputDir = false;
        
        for (int i = 3; i < argc; i++) {
            std::wstring arg = argv[i];
            
            if ((arg == L"/name" || arg == L"-name") && i + 1 < argc) {
                options.packageName = argv[++i];
                hasName = true;
            }
            else if ((arg == L"/publisher" || arg == L"-publisher") && i + 1 < argc) {
                options.publisherName = argv[++i];
                hasPublisher = true;
            }
            else if ((arg == L"/o" || arg == L"-o") && i + 1 < argc) {
                options.outputPath = argv[++i];
                hasOutputDir = true;
            }
            else if ((arg == L"/sign" || arg == L"-sign") && i + 1 < argc) {
                options.certPath = argv[++i];
                options.shouldSign = true;
            }
            else if ((arg == L"/pwd" || arg == L"-pwd") && i + 1 < argc) {
                options.certPassword = argv[++i];
            }
            else if (arg == L"/verbose" || arg == L"-verbose") {
                options.verbose = true;
            }
            else if (arg.substr(0, 1) == L"/" || arg.substr(0, 1) == L"-") {
                std::wcerr << L"Error: Unknown option: " << arg << std::endl;
            }
            else if (!hasOutputDir && options.outputPath.empty()) {
                // Legacy support: third positional argument is output path
                options.outputPath = arg;
                hasOutputDir = true;
            }
        }
        
        // Check for required options for /pack command
        if (!hasName) {
            std::wcerr << L"Error: Missing required option /name for /pack command" << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
        
        if (!hasPublisher) {
            std::wcerr << L"Error: Missing required option /publisher for /pack command" << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
        
        if (!hasOutputDir) {
            std::wcerr << L"Error: Missing required output directory. Use /o option to specify output directory" << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
        
        // Validate the input folder exists
        fs::path inputFolder(options.inputPath);
        if (!fs::exists(inputFolder)) {
            std::wcerr << L"Error: Input folder does not exist: " << options.inputPath << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
        
        if (!fs::is_directory(inputFolder)) {
            std::wcerr << L"Error: Input path is not a directory: " << options.inputPath << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
        
        // Validate the certificate path if signing is requested
        if (options.shouldSign && !fs::exists(options.certPath)) {
            std::wcerr << L"Error: Certificate file does not exist: " << options.certPath.wstring() << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
    }
    else if (command == L"/downloadAndPack") {
        options.command = CommandLineOptions::Command::DownloadAndPackage;
        
        // Check if we have the required URI
        if (argc < 3) {
            std::wcerr << L"Error: Missing input URI" << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
        
        options.inputPath = argv[2];
        
        // Parse all arguments
        bool hasOutputDir = false;
        
        for (int i = 3; i < argc; i++) {
            std::wstring arg = argv[i];
            
            if ((arg == L"/name" || arg == L"-name") && i + 1 < argc) {
                options.packageName = argv[++i];
            }
            else if ((arg == L"/publisher" || arg == L"-publisher") && i + 1 < argc) {
                options.publisherName = argv[++i];
            }
            else if ((arg == L"/o" || arg == L"-o") && i + 1 < argc) {
                options.outputPath = argv[++i];
                hasOutputDir = true;
            }
            else if ((arg == L"/sign" || arg == L"-sign") && i + 1 < argc) {
                options.certPath = argv[++i];
                options.shouldSign = true;
            }
            else if ((arg == L"/pwd" || arg == L"-pwd") && i + 1 < argc) {
                options.certPassword = argv[++i];
            }
            else if (arg == L"/verbose" || arg == L"-verbose") {
                options.verbose = true;
            }
            else if (arg.substr(0, 1) == L"/" || arg.substr(0, 1) == L"-") {
                std::wcerr << L"Error: Unknown option: " << arg << std::endl;
            }
            else if (!hasOutputDir && options.outputPath.empty()) {
                // Legacy support: third positional argument is output path
                options.outputPath = arg;
                hasOutputDir = true;
            }
        }
        
        // Make /o mandatory for download command as well
        if (!hasOutputDir) {
            std::wcerr << L"Error: Missing required output directory. Use /o option to specify output directory" << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
        
        // For name and publisher, they are optional but we'll warn that they will be inferred
        if (options.packageName.empty() || options.publisherName.empty()) {
            std::wcout << L"Note: Missing package name or publisher. They will be inferred from the repository URI." << std::endl;
        }
        
        // Validate the certificate path if signing is requested
        if (options.shouldSign && !fs::exists(options.certPath)) {
            std::wcerr << L"Error: Certificate file does not exist: " << options.certPath.wstring() << std::endl;
            options.command = CommandLineOptions::Command::ShowHelp;
            return options;
        }
    }
    else if (command == L"/help" || command == L"-help" || command == L"/?" || command == L"-?") {
        options.command = CommandLineOptions::Command::ShowHelp;
    }
    else {
        std::wcerr << L"Error: Unknown command: " << command << std::endl;
        options.command = CommandLineOptions::Command::ShowHelp;
    }
    
    return options;
}

void CommandLineParser::ShowUsage()
{
    std::wcout << L"ModelPackagingTool - Tool for packaging model files into MSIX packages" << std::endl;
    std::wcout << L"Usage:" << std::endl;
    std::wcout << L"  ModelPackagingTool /pack <path-to-folder> /name <n> /publisher <publisher> /o <output-dir> [/sign <cert-path>]" << std::endl;
    std::wcout << L"  ModelPackagingTool /downloadAndPack <uri> /o <output-dir> [/name <n>] [/publisher <publisher>] [/sign <cert-path>]" << std::endl;
    std::wcout << L"  ModelPackagingTool /help" << std::endl;
    std::wcout << std::endl;
    std::wcout << L"Commands:" << std::endl;
    std::wcout << L"  /pack                 Package a local folder into an MSIX package" << std::endl;
    std::wcout << L"  /downloadAndPack      Download model files from a URI and package them" << std::endl;
    std::wcout << L"  /help                 Show this help information" << std::endl;
    std::wcout << std::endl;
    std::wcout << L"Options:" << std::endl;
    std::wcout << L"  /o <dir>              Specify output directory (required for all package commands)" << std::endl;
    std::wcout << L"  /name <n>             Specify package name (required for /pack, optional for /downloadAndPack)" << std::endl;
    std::wcout << L"  /publisher <n>        Specify publisher name (required for /pack)" << std::endl;
    std::wcout << L"  /sign <cert-path>     Sign the MSIX package with the specified certificate" << std::endl;
    std::wcout << L"  /pwd <password>       Specify password for certificate" << std::endl;
    std::wcout << L"  /verbose              Enable verbose output" << std::endl;
    std::wcout << std::endl;
    std::wcout << L"Examples:" << std::endl;
    std::wcout << L"  ModelPackagingTool /pack C:\\Models\\MyModel /name MyModel /publisher Contoso /o C:\\Output" << std::endl;
    std::wcout << L"  ModelPackagingTool /downloadAndPack https://huggingface.co/openai-community/gpt2/tree/main/onnx /o C:\\Output" << std::endl;
    std::wcout << L"  ModelPackagingTool /downloadAndPack https://huggingface.co/openai-community/gpt2 /o C:\\Output /name gpt2 /publisher openai-community" << std::endl;
    std::wcout << L"  ModelPackagingTool /pack C:\\Models\\MyModel /name MyModel /publisher Contoso /o C:\\Output /sign C:\\Certs\\MyCert.pfx /pwd mypassword" << std::endl;
    std::wcout << std::endl;
    std::wcout << L"Signing Options:" << std::endl;
    std::wcout << L"  /sign <cert-file>     Specify certificate file for signing (required for signed packages)" << std::endl;
    std::wcout << L"  /pwd <password>       Specify password for the certificate (if any)" << std::endl;
    std::wcout << std::endl;
    std::wcout << L"Note:" << std::endl;
    std::wcout << L"  - Signing is optional for /pack and /downloadAndPack commands" << std::endl;
    std::wcout << L"  - If signing is enabled, the specified certificate must be valid" << std::endl;
}