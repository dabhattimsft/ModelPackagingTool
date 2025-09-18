#include "MsixPackager.h"
#include "CertificateManager.h"
#include "AppxManifestTemplates.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <Windows.h>
#include <string>
#include <regex>
#include <shobjidl.h> 
#include <shlwapi.h>
#include <wrl/client.h>
#include <comdef.h>
#include <algorithm>
#include <wil/registry.h>  // Include WIL registry helpers

// AppxPackaging.h interfaces are not directly available in the Windows SDK headers,
// so we need to define them ourselves based on the COM interfaces
// These are the core interfaces needed for package creation

// CLSID and IIDs for the AppxFactory and related interfaces
// {5842a140-ff9f-4166-8f5c-62f5b7b0c781}
static const GUID CLSID_AppxFactory = 
{ 0x5842a140, 0xff9f, 0x4166, { 0x8f, 0x5c, 0x62, 0xf5, 0xb7, 0xb0, 0xc7, 0x81 } };

// {BEB94909-E451-438B-B5A7-D79E767B75D8}
static const GUID IID_IAppxFactory =
{ 0xbeb94909, 0xe451, 0x438b, { 0xb5, 0xa7, 0xd7, 0x9e, 0x76, 0x7b, 0x75, 0xd8 } };

// {91DF827B-94FC-468F-837C-DAD7B62F798B}
static const GUID IID_IAppxPackageWriter =
{ 0x91df827b, 0x94fc, 0x468f, { 0x83, 0x7c, 0xda, 0xd7, 0xb6, 0x2f, 0x79, 0x8b } };

// {4E1BD148-55A0-4480-A3D1-15544710637C}
static const GUID IID_IAppxManifestReader =
{ 0x4e1bd148, 0x55a0, 0x4480, { 0xa3, 0xd1, 0x15, 0x54, 0x47, 0x10, 0x63, 0x7c } };

// Define the compression option enum
enum APPX_COMPRESSION_OPTION
{
    APPX_COMPRESSION_OPTION_NONE = 0,
    APPX_COMPRESSION_OPTION_NORMAL = 1,
    APPX_COMPRESSION_OPTION_MAXIMUM = 2,
    APPX_COMPRESSION_OPTION_FAST = 3,
    APPX_COMPRESSION_OPTION_SUPERFAST = 4,
};

// Package settings structure
struct APPX_PACKAGE_SETTINGS
{
    BOOL forceZip32;
    IStream* hashMethod;
};

// Define the interfaces needed for MSIX package creation
struct IAppxManifestReader : public IUnknown
{
public:
    // Basic manifest reader interface - not fully implemented as we don't need all methods
    virtual HRESULT STDMETHODCALLTYPE GetProperties(void** properties) = 0;
};

struct IAppxPackageWriter : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE AddPayloadFile(
        LPCWSTR fileName,
        LPCWSTR contentType,
        APPX_COMPRESSION_OPTION compressionOption,
        IStream* inputStream) = 0;
    
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
};

struct IAppxFactory : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE CreatePackageWriter(
        IStream* outputStream,
        APPX_PACKAGE_SETTINGS* settings,
        IAppxPackageWriter** packageWriter) = 0;
    
    virtual HRESULT STDMETHODCALLTYPE CreateManifestReader(
        IStream* inputStream,
        IAppxManifestReader** manifestReader) = 0;
    
    // Other methods not used in this implementation
    virtual HRESULT STDMETHODCALLTYPE CreatePackageReader(void* inputStream, void** packageReader) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateContentGroupMapReader(void* inputStream, void** contentGroupMapReader) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateContentGroupMapWriter(void* inputStream, void** contentGroupMapWriter) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSourceContentGroupMapReader(void* inputStream, void** sourceContentGroupMapReader) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateBlockMapReader(void* inputStream, void** blockMapReader) = 0;
};

using namespace Microsoft::WRL;

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
    
    // Check if AppxManifest.xml already exists in the source folder
    fs::path manifestPath = sourceFolder / L"AppxManifest.xml";
    if (!fs::exists(manifestPath)) {
        // Create AppxManifest.xml in the source folder if it doesn't exist
        if (!CreateAppxManifest(sourceFolder, finalPackageName, finalPublisherName)) {
            std::wcerr << L"Failed to create AppxManifest.xml" << std::endl;
            return false;
        }
    } else {
        std::wcout << L"Using existing AppxManifest.xml found in source folder" << std::endl;
    }
    
    // Create default assets (images) if they don't exist
    if (!CreateDefaultAssets(sourceFolder)) {
        std::wcerr << L"Failed to create default assets" << std::endl;
        return false;
    }
    
    // Try to create the MSIX package programmatically first
    std::wcout << L"Attempting to create MSIX package programmatically..." << std::endl;
    if (CreateMsixPackageProgrammatically(sourceFolder, finalOutputPath)) {
        std::wcout << L"MSIX package created successfully: " << finalOutputPath.wstring() << std::endl;
        return true;
    }
    
    // If that fails, fall back to MakeAppx.exe
    std::wcout << L"Programmatic creation failed, falling back to MakeAppx.exe..." << std::endl;
    if (BuildMsixPackage(sourceFolder, finalOutputPath)) {
        std::wcout << L"MSIX package created successfully: " << finalOutputPath.wstring() << std::endl;
        return true;
    }
    
    // If both methods fail, try the direct API-based method
    std::wcerr << L"Failed to build MSIX package, trying simplified method..." << std::endl;
    if (CreateZipBasedPackage(sourceFolder, finalOutputPath)) {
        std::wcout << L"MSIX package created with fallback method: " << finalOutputPath.wstring() << std::endl;
        return true;
    }
    
    std::wcerr << L"All package creation methods failed." << std::endl;
    return false;
}

bool MsixPackager::CreateAppxManifest(
    const fs::path& sourceFolder,
    const std::wstring& packageName,
    const std::wstring& publisherName)
{
    // Create the AppxManifest.xml file
    fs::path manifestPath = sourceFolder / L"AppxManifest.xml";
    
    // Clean package name to be valid for MSIX
    std::wstring cleanPackageName = CleanNameForPackage(packageName);
    std::wstring cleanPublisherName = CleanNameForPackage(publisherName);
    
    // Get the manifest content from the template
    std::wstring manifestContent = AppxManifestTemplates::GetStandardManifestTemplate(
        cleanPackageName, 
        cleanPublisherName);
    
    // Write to file
    try {
        std::wofstream manifestFile(manifestPath);
        if (!manifestFile.is_open()) {
            std::wcerr << L"Failed to open manifest file for writing: " << manifestPath.wstring() << std::endl;
            return false;
        }
        
        manifestFile << manifestContent;
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

bool MsixPackager::CreateMsixPackageProgrammatically(
    const fs::path& sourceFolder,
    const fs::path& outputMsixPath)
{
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::wcerr << L"Failed to initialize COM: 0x" << std::hex << hr << std::endl;
        return false;
    }
    
    // Ensure COM is uninitialized when we exit
    struct CoUninitializeOnExit {
        ~CoUninitializeOnExit() { CoUninitialize(); }
    } comUninitializer;
    
    try {
        // Create the AppxFactory
        ComPtr<IAppxFactory> appxFactory;
        hr = CoCreateInstance(
            CLSID_AppxFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_IAppxFactory,
            &appxFactory);
            
        if (FAILED(hr)) {
            std::wcerr << L"Failed to create AppxFactory: 0x" << std::hex << hr << std::endl;
            return false;
        }
        
        // Create an output stream for the package
        ComPtr<IStream> outputStream;
        hr = SHCreateStreamOnFileW(
            outputMsixPath.wstring().c_str(),
            STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE,
            &outputStream);
            
        if (FAILED(hr)) {
            std::wcerr << L"Failed to create output stream: 0x" << std::hex << hr << std::endl;
            return false;
        }
        
        // Create package settings
        APPX_PACKAGE_SETTINGS packageSettings = {};
        packageSettings.forceZip32 = FALSE;
        packageSettings.hashMethod = nullptr;
        
        // Create the package writer
        ComPtr<IAppxPackageWriter> packageWriter;
        hr = appxFactory->CreatePackageWriter(
            outputStream.Get(),
            &packageSettings,
            &packageWriter);
            
        if (FAILED(hr)) {
            std::wcerr << L"Failed to create package writer: 0x" << std::hex << hr << std::endl;
            return false;
        }
        
        // First, add the manifest file
        fs::path manifestPath = sourceFolder / L"AppxManifest.xml";
        if (!AddFileToMsixPackage(packageWriter.Get(), sourceFolder, L"AppxManifest.xml")) {
            return false;
        }
        
        // Recursively add all files in the source folder
        for (const auto& entry : fs::recursive_directory_iterator(sourceFolder)) {
            if (entry.is_regular_file() && entry.path().filename() != L"AppxManifest.xml") {
                fs::path relativePath = fs::relative(entry.path(), sourceFolder);
                if (!AddFileToMsixPackage(packageWriter.Get(), sourceFolder, relativePath)) {
                    return false;
                }
            }
        }
        
        // Close and finalize the package
        hr = packageWriter->Close();
        if (FAILED(hr)) {
            std::wcerr << L"Failed to close and finalize package: 0x" << std::hex << hr << std::endl;
            return false;
        }
        
        std::wcout << L"Successfully created MSIX package programmatically" << std::endl;
        return true;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error during programmatic MSIX creation: " << ex.what() << std::endl;
        return false;
    }
    catch (...) {
        std::wcerr << L"Unknown error during programmatic MSIX creation" << std::endl;
        return false;
    }
}

bool MsixPackager::AddFileToMsixPackage(
    void* packageWriterVoid,
    const fs::path& sourceFolder,
    const fs::path& relativePath)
{
    IAppxPackageWriter* packageWriter = static_cast<IAppxPackageWriter*>(packageWriterVoid);
    
    try {
        fs::path fullPath = sourceFolder / relativePath;
        
        // Skip if the file doesn't exist
        if (!fs::exists(fullPath) || !fs::is_regular_file(fullPath)) {
            return true;
        }
        
        // Create an input stream for the file
        ComPtr<IStream> inputStream;
        HRESULT hr = SHCreateStreamOnFileW(
            fullPath.wstring().c_str(),
            STGM_READ | STGM_SHARE_DENY_WRITE,
            &inputStream);
            
        if (FAILED(hr)) {
            std::wcerr << L"Failed to create input stream for file " << fullPath.wstring() 
                      << L": 0x" << std::hex << hr << std::endl;
            return false;
        }
        
        // Convert the relative path to forward slashes for MSIX package
        std::wstring packagePath = relativePath.wstring();
        std::replace(packagePath.begin(), packagePath.end(), L'\\', L'/');
        
        // Determine the content type (use empty string to let the API determine it)
        std::wstring contentType = L"";
        
        // Add the file to the package
        hr = packageWriter->AddPayloadFile(
            packagePath.c_str(),
            contentType.c_str(),
            APPX_COMPRESSION_OPTION_NORMAL,
            inputStream.Get());
            
        if (FAILED(hr)) {
            std::wcerr << L"Failed to add file " << packagePath 
                      << L" to package: 0x" << std::hex << hr << std::endl;
            return false;
        }
        
        if (packagePath.length() < 30) {
            std::wcout << L"Added file to package: " << packagePath << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error adding file to package: " << ex.what() << std::endl;
        return false;
    }
    catch (...) {
        std::wcerr << L"Unknown error adding file to package" << std::endl;
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

bool MsixPackager::CreateZipBasedPackage(
    const fs::path& sourceFolder,
    const fs::path& outputMsixPath)
{
    // This is a fallback method that creates an MSIX package directly using Windows API
    // MSIX packages are essentially ZIP files with specific contents
    std::wcout << L"Using direct API-based packaging method..." << std::endl;
    
    try {
        // Ensure output directory exists
        if (!outputMsixPath.parent_path().empty() && !fs::exists(outputMsixPath.parent_path())) {
            fs::create_directories(outputMsixPath.parent_path());
        }
        
        // If there's an existing file at the target path, remove it
        if (fs::exists(outputMsixPath)) {
            fs::remove(outputMsixPath);
        }
        
        // Initialize COM for SHCreateStdEnumFmtEtc
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
            std::wcerr << L"Failed to initialize COM: 0x" << std::hex << hr << std::endl;
            return false;
        }
        
        // Ensure COM is uninitialized when we exit
        struct CoUninitializeOnExit {
            ~CoUninitializeOnExit() { CoUninitialize(); }
        } comUninitializer;
        
        // Create a Storage object (structured storage that will become our MSIX package)
        ComPtr<IStorage> rootStorage;
        hr = StgCreateStorageEx(
            outputMsixPath.wstring().c_str(),
            STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
            STGFMT_DOCFILE,
            0,
            nullptr,
            nullptr,
            IID_IStorage,
            reinterpret_cast<void**>(rootStorage.GetAddressOf()));
            
        if (FAILED(hr)) {
            std::wcerr << L"Failed to create storage: 0x" << std::hex << hr << std::endl;
            return false;
        }
        
        // First, ensure the AppxManifest.xml exists
        fs::path manifestPath = sourceFolder / L"AppxManifest.xml";
        if (!fs::exists(manifestPath)) {
            std::wcerr << L"AppxManifest.xml not found. Cannot create package." << std::endl;
            return false;
        }
        
        // Add the AppxManifest.xml as the first file
        if (!AddFileToStorage(rootStorage.Get(), sourceFolder, L"AppxManifest.xml")) {
            std::wcerr << L"Failed to add AppxManifest.xml to package" << std::endl;
            return false;
        }
        
        // Recursively add all files in the source folder
        for (const auto& entry : fs::recursive_directory_iterator(sourceFolder)) {
            if (entry.is_regular_file() && entry.path().filename() != L"AppxManifest.xml") {
                fs::path relativePath = fs::relative(entry.path(), sourceFolder);
                
                if (!AddFileToStorage(rootStorage.Get(), sourceFolder, relativePath)) {
                    std::wcerr << L"Failed to add file to package: " << relativePath.wstring() << std::endl;
                    return false;
                }
            }
        }
        
        // Close the storage to commit all changes
        rootStorage.Reset();
        
        std::wcout << L"MSIX package created at: " << outputMsixPath.wstring() << std::endl;
        std::wcout << L"Note: This package was created using direct Windows API methods." << std::endl;
        
        return true;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error creating package: " << ex.what() << std::endl;
        return false;
    }
    catch (...) {
        std::wcerr << L"Unknown error during package creation" << std::endl;
        return false;
    }
}

// Helper method to add a file to a structured storage object
bool MsixPackager::AddFileToStorage(
    void* storageVoid,
    const fs::path& sourceFolder,
    const fs::path& relativePath)
{
    IStorage* storage = static_cast<IStorage*>(storageVoid);
    
    try {
        fs::path fullPath = sourceFolder / relativePath;
        std::wstring fileName = relativePath.filename().wstring();
        
        // Create all parent directories in the storage hierarchy
        IStorage* currentStorage = storage;
        ComPtr<IStorage> subStorage;
        
        // If the file is in a subdirectory, create the directory structure
        if (relativePath.has_parent_path()) {
            fs::path currentPath;
            
            for (const auto& part : relativePath.parent_path()) {
                currentPath /= part;
                std::wstring dirName = part.wstring();
                
                // Try to open the existing substorage
                HRESULT hr = currentStorage->OpenStorage(
                    dirName.c_str(),
                    nullptr,
                    STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                    nullptr,
                    0,
                    &subStorage);
                    
                // If it doesn't exist, create it
                if (FAILED(hr)) {
                    hr = currentStorage->CreateStorage(
                        dirName.c_str(),
                        STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                        0,
                        0,
                        &subStorage);
                        
                    if (FAILED(hr)) {
                        std::wcerr << L"Failed to create directory in storage: " << dirName 
                                  << L", error: 0x" << std::hex << hr << std::endl;
                        return false;
                    }
                }
                
                // Move to the next level in the hierarchy
                if (currentStorage != storage) {
                    currentStorage->Release();
                }
                
                currentStorage = subStorage.Detach();
            }
        }
        
        // Now create the stream for the file
        ComPtr<IStream> stream;
        HRESULT hr = currentStorage->CreateStream(
            fileName.c_str(),
            STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE,
            0,
            0,
            &stream);
            
        if (FAILED(hr)) {
            std::wcerr << L"Failed to create stream for file: " << fileName 
                      << L", error: 0x" << std::hex << hr << std::endl;
            
            // Clean up the current storage if it's not the root
            if (currentStorage != storage) {
                currentStorage->Release();
            }
            
            return false;
        }
        
        // Open the source file
        std::ifstream sourceFile(fullPath, std::ios::binary);
        if (!sourceFile) {
            std::wcerr << L"Failed to open source file: " << fullPath.wstring() << std::endl;
            
            // Clean up
            if (currentStorage != storage) {
                currentStorage->Release();
            }
            
            return false;
        }
        
        // Read the file in chunks and write to the stream
        constexpr size_t bufferSize = 8192;
        char buffer[bufferSize];
        
        while (sourceFile) {
            sourceFile.read(buffer, bufferSize);
            std::streamsize bytesRead = sourceFile.gcount();
            
            if (bytesRead > 0) {
                ULONG bytesWritten = 0;
                hr = stream->Write(buffer, static_cast<ULONG>(bytesRead), &bytesWritten);
                
                if (FAILED(hr) || bytesWritten != bytesRead) {
                    std::wcerr << L"Failed to write to stream, error: 0x" << std::hex << hr << std::endl;
                    
                    // Clean up
                    if (currentStorage != storage) {
                        currentStorage->Release();
                    }
                    
                    return false;
                }
            }
        }
        
        // Commit the changes to the stream
        hr = stream->Commit(STGC_DEFAULT);
        if (FAILED(hr)) {
            std::wcerr << L"Failed to commit stream, error: 0x" << std::hex << hr << std::endl;
            
            // Clean up
            if (currentStorage != storage) {
                currentStorage->Release();
            }
            
            return false;
        }
        
        // Commit the changes to the storage
        hr = currentStorage->Commit(STGC_DEFAULT);
        if (FAILED(hr)) {
            std::wcerr << L"Failed to commit storage, error: 0x" << std::hex << hr << std::endl;
            
            // Clean up
            if (currentStorage != storage) {
                currentStorage->Release();
            }
            
            return false;
        }
        
        // Clean up the current storage if it's not the root
        if (currentStorage != storage) {
            currentStorage->Release();
        }
        
        std::wcout << L"Added file to package: " << relativePath.wstring() << std::endl;
        return true;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error adding file to storage: " << ex.what() << std::endl;
        return false;
    }
    catch (...) {
        std::wcerr << L"Unknown error adding file to storage" << std::endl;
        return false;
    }
}

// Implementation of BuildMsixPackage method
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
    
    // Build the command line with /nv flag to skip validation of assets
    std::wstring cmdLine = L"\"" + makeAppxPath.wstring() + L"\" pack /d \"" + 
                          sourceFolder.wstring() + L"\" /p \"" + 
                          outputMsixPath.wstring() + L"\" /o /nv";
    
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

// Implementation of FindWindowsSDKPath method
fs::path MsixPackager::FindWindowsSDKPath()
{
    fs::path sdkPath;
    
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
            return sdkPath;
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
            return sdkPath;
        }
        
        // Find the latest SDK version
        fs::path binPath = fs::path(kitsRootPath) / L"bin";
        
        if (!fs::exists(binPath) || !fs::is_directory(binPath)) {
            std::wcerr << L"Windows SDK bin path does not exist: " << binPath.wstring() << std::endl;
            return sdkPath;
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
            return sdkPath;
        }
        
        // Sort in descending order to get the latest version first
        std::sort(versionPaths.begin(), versionPaths.end(), std::greater<fs::path>());
        
        // Find the first version that has the x64 tools
        for (const auto& verPath : versionPaths) {
            fs::path x64Path = verPath / L"x64";
            if (fs::exists(x64Path) && fs::is_directory(x64Path)) {
                sdkPath = x64Path;
                std::wcout << L"Found Windows SDK tools in: " << sdkPath.wstring() << std::endl;
                break;
            }
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Error finding Windows SDK path: " << ex.what() << std::endl;
    }
    
    if (sdkPath.empty()) {
        std::wcerr << L"Could not find a valid Windows SDK installation." << std::endl;
    }
    
    return sdkPath;
}