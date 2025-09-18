#include "HuggingFaceDownloader.h"
#include <iostream>
#include <fstream>
#include <winerror.h> // For E_FAIL
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <regex>
#include <string>
#include <vector>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::Http;
using namespace Windows::Web::Http::Headers;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

HuggingFaceDownloader::HuggingFaceDownloader() : m_cancelRequested(false)
{
    // Set default headers
    m_httpClient.DefaultRequestHeaders().UserAgent().Append(
        HttpProductInfoHeaderValue(L"ModelPackagingTool", L"1.0"));
}

winrt::Windows::Foundation::IAsyncAction HuggingFaceDownloader::DownloadFileAsync(
    const std::wstring& repoOwner,
    const std::wstring& repoName,
    const std::wstring& branch,
    const std::wstring& filePath,
    const fs::path& destinationPath,
    ProgressCallback progressCallback)
{
    m_cancelRequested = false;
    
    // Construct the download URL
    std::wstring downloadUrl = BuildDownloadUrl(repoOwner, repoName, branch, filePath);
    
    // Create URI from the URL
    Uri uri(downloadUrl);
    
    // Ensure the directory exists
    EnsureDirectoryExists(destinationPath);
    
    // Start the download
    auto fileName = GetFileName(filePath);
    
    try
    {
        // Use GetAsync to download the file
        auto response = co_await m_httpClient.GetAsync(uri);
        
        // Check if the request was successful
        response.EnsureSuccessStatusCode();
        
        // Get content length for progress reporting
        uint64_t totalBytes = 0;
        auto contentLengthHeader = response.Content().Headers().ContentLength();
        if (contentLengthHeader) {
            totalBytes = contentLengthHeader.Value();
        }
        
        // Get the buffer from the response
        auto buffer = co_await response.Content().ReadAsBufferAsync();
        
        // Create a StorageFile for the destination
        auto folder = co_await StorageFolder::GetFolderFromPathAsync(destinationPath.parent_path().wstring());
        auto file = co_await folder.CreateFileAsync(
            fileName,
            CreationCollisionOption::ReplaceExisting
        );
        
        // Write the buffer to the file
        co_await FileIO::WriteBufferAsync(file, buffer);
        
        // Update progress after download is complete
        if (progressCallback) {
            progressCallback(fileName, buffer.Length(), totalBytes);
        }
    }
    catch (const winrt::hresult_error& ex)
    {
        // If we've manually cancelled, just return
        if (m_cancelRequested) {
            co_return;
        }
        
        // Re-throw any other errors
        throw ex;
    }
}

// Helper function to parse a simplified JSON string and extract file paths
std::vector<std::wstring> HuggingFaceDownloader::ParseJsonFilesResponse(
    const std::string& jsonStr)
{
    std::vector<std::wstring> filePaths;
    
    // Use regex to find all the "path" fields in the JSON
    std::regex pathPattern("\"path\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch match;
    
    std::string::const_iterator searchStart(jsonStr.cbegin());
    while (std::regex_search(searchStart, jsonStr.cend(), match, pathPattern)) {
        std::string path = match[1];
        
        // Convert std::string to std::wstring
        std::wstring wpath(path.begin(), path.end());
        filePaths.push_back(wpath);
        
        // Move search position
        searchStart = match.suffix().first;
    }
    
    return filePaths;
}

winrt::Windows::Foundation::IAsyncAction HuggingFaceDownloader::DownloadFolderAsync(
    const std::wstring& repoOwner,
    const std::wstring& repoName,
    const std::wstring& branch,
    const std::wstring& folderPath,
    const fs::path& destinationFolder,
    ProgressCallback progressCallback)
{
    m_cancelRequested = false;
    
    // Create a subfolder with the repository name
    fs::path repoFolder = destinationFolder / repoName;
    
    // Ensure the destination folder exists
    if (!fs::exists(repoFolder)) {
        fs::create_directories(repoFolder);
    }
    
    // Clean up the folder path
    std::wstring cleanFolderPath = folderPath;
    
    // Remove leading slash if present
    if (!cleanFolderPath.empty() && cleanFolderPath[0] == L'/') {
        cleanFolderPath = cleanFolderPath.substr(1);
    }
    
    // Remove trailing slash if present
    if (!cleanFolderPath.empty() && cleanFolderPath.back() == L'/') {
        cleanFolderPath = cleanFolderPath.substr(0, cleanFolderPath.length() - 1);
    }
    
    // Create the API URL to get the file list
    // Format: https://huggingface.co/api/models/{owner}/{repo}/tree/{branch}/{path}
    std::wstring apiUrl = L"https://huggingface.co/api/models/";
    apiUrl += repoOwner + L"/" + repoName + L"/tree/" + branch;
    
    // Add folder path if it's not empty
    if (!cleanFolderPath.empty()) {
        apiUrl += L"/" + cleanFolderPath;
    }
    
    // Create URI from the URL
    Uri uri(apiUrl);
    
    std::vector<std::wstring> filePaths;
    
    try {
        // Make the HTTP request to get the file list
        auto response = co_await m_httpClient.GetAsync(uri);
        response.EnsureSuccessStatusCode();
        
        // Get the JSON content as a string
        auto jsonContent = co_await response.Content().ReadAsStringAsync();
        std::string jsonStr = winrt::to_string(jsonContent);
        
        // Parse the JSON response to extract all file paths
        filePaths = ParseJsonFilesResponse(jsonStr);
        
        // Filter files to only include those directly in the specified folder
        if (!cleanFolderPath.empty()) {
            // This filtering logic ensures we only download files that are directly 
            // in the specified folder, not in subfolders.
            // For example, if folderPath is "onnx", we want files like "onnx/file.txt"
            // but not files like "onnx/subfolder/file.txt"
            std::wstring prefix = cleanFolderPath + L"/";
            
            std::vector<std::wstring> filteredPaths;
            for (const auto& path : filePaths) {
                // Check if the path starts with the folder prefix
                if (path.find(prefix) == 0) {
                    // Extract the part after the prefix
                    std::wstring relativePath = path.substr(prefix.length());
                    
                    // Only include files directly in this folder (no additional slashes)
                    if (relativePath.find(L'/') == std::wstring::npos) {
                        filteredPaths.push_back(path);
                    }
                }
            }
            
            filePaths = std::move(filteredPaths);
        }
    }
    catch (const winrt::hresult_error& ex) {
        std::wcerr << L"Error fetching file list: " << ex.message().c_str() << std::endl;
        throw;
    }
    
    if (filePaths.empty()) {
        std::wcout << L"No files found in the specified folder path." << std::endl;
        co_return;
    }
    
    // Download each file
    for (const auto& filePath : filePaths) {
        if (m_cancelRequested) {
            co_return;
        }
        
        // Extract the file name for the destination path
        fs::path fileName = fs::path(filePath).filename();
        fs::path destPath = repoFolder / fileName;
        
        std::wcout << L"Downloading: " << filePath << L" to " << destPath.wstring() << std::endl;
        
        // Download the file
        try {
            co_await DownloadFileAsync(
                repoOwner,
                repoName,
                branch,
                filePath,
                destPath,
                progressCallback
            );
        }
        catch (const winrt::hresult_error& ex) {
            std::wcerr << L"Error downloading file " << filePath << L": " << ex.message().c_str() << std::endl;
            // Continue with other files instead of failing completely
        }
    }
}

void HuggingFaceDownloader::CancelDownloads()
{
    m_cancelRequested = true;
}

std::wstring HuggingFaceDownloader::BuildDownloadUrl(
    const std::wstring& repoOwner,
    const std::wstring& repoName,
    const std::wstring& branch,
    const std::wstring& filePath)
{
    // Format: https://huggingface.co/{owner}/{repo}/resolve/{branch}/{path}
    std::wstring url = L"https://huggingface.co/";
    url += repoOwner + L"/" + repoName + L"/resolve/" + branch + L"/";
    
    // Remove leading slash if present
    std::wstring path = filePath;
    if (!path.empty() && path[0] == L'/') {
        path = path.substr(1);
    }
    
    url += path;
    return url;
}

void HuggingFaceDownloader::EnsureDirectoryExists(const fs::path& filePath)
{
    auto directory = filePath.parent_path();
    if (!fs::exists(directory)) {
        fs::create_directories(directory);
    }
}

std::wstring HuggingFaceDownloader::GetFileName(const std::wstring& filePath)
{
    size_t pos = filePath.find_last_of(L"/\\");
    if (pos != std::wstring::npos) {
        return filePath.substr(pos + 1);
    }
    return filePath;
}