#include "GitHubDownloader.h"
#include <iostream>
#include <fstream>
#include <winerror.h> // For E_FAIL
#include <winrt/base.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Web.Http.Headers.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::Http;
using namespace Windows::Web::Http::Headers;
using namespace Windows::Storage::Streams;

GitHubDownloader::GitHubDownloader() : m_cancelRequested(false)
{
    // Set default headers
    m_httpClient.DefaultRequestHeaders().UserAgent().Append(
        HttpProductInfoHeaderValue(L"ModelPackagingTool", L"1.0"));
}

winrt::Windows::Foundation::IAsyncAction GitHubDownloader::DownloadFileAsync(
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
        // Use GetAsync for streaming downloads
        auto response = co_await m_httpClient.GetAsync(
            uri, 
            HttpCompletionOption::ResponseHeadersRead
        );
        
        // Check if the request was successful
        response.EnsureSuccessStatusCode();
        
        // Get content length for progress reporting
        uint64_t totalBytes = 0;
        auto contentLengthHeader = response.Content().Headers().ContentLength();
        if (contentLengthHeader) {
            totalBytes = contentLengthHeader.Value();
        }
        
        // Get the input stream from the response
        auto inputStream = co_await response.Content().ReadAsInputStreamAsync();
        
        // Create a file stream for writing
        std::ofstream fileStream(destinationPath.string(), std::ios::binary | std::ios::trunc);
        if (!fileStream.is_open()) {
            throw winrt::hresult_error(E_FAIL, L"Failed to open file for writing");
        }
        
        // Read data in chunks for better performance and to provide progress updates
        const uint32_t bufferSize = 64 * 1024; // 64 KB buffer
        Buffer buffer(bufferSize);
        uint64_t bytesReceived = 0;
        
        while (true)
        {
            if (m_cancelRequested) {
                co_return;
            }
            
            // Read a chunk of data
            auto ibuffer = co_await inputStream.ReadAsync(buffer, bufferSize, InputStreamOptions::Partial);
            
            // Check if we reached the end of the stream
            if (ibuffer.Length() == 0) {
                break;
            }
            
            // Get raw data from the buffer
            auto data = ibuffer.data();
            auto dataSize = ibuffer.Length();
            
            // Write to file
            fileStream.write(reinterpret_cast<const char*>(data), dataSize);
            
            // Update progress
            bytesReceived += dataSize;
            if (progressCallback) {
                progressCallback(fileName, bytesReceived, totalBytes);
            }
        }
        
        // Close the file
        fileStream.close();
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

winrt::Windows::Foundation::IAsyncAction GitHubDownloader::DownloadFolderAsync(
    const std::wstring& repoOwner,
    const std::wstring& repoName,
    const std::wstring& branch,
    const std::wstring& folderPath,
    const fs::path& destinationFolder,
    ProgressCallback progressCallback)
{
    m_cancelRequested = false;
    
    // Ensure the destination folder exists
    if (!fs::exists(destinationFolder)) {
        fs::create_directories(destinationFolder);
    }
    
    // For a complete implementation, we would need to use the GitHub API to list files in a directory
    // This would require API tokens and parsing JSON responses
    // For now, this is a placeholder that would need to be expanded with GitHub API integration
    
    co_return;
}

void GitHubDownloader::CancelDownloads()
{
    m_cancelRequested = true;
}

std::wstring GitHubDownloader::BuildDownloadUrl(
    const std::wstring& repoOwner,
    const std::wstring& repoName,
    const std::wstring& branch,
    const std::wstring& filePath)
{
    // Format: https://raw.githubusercontent.com/{owner}/{repo}/{branch}/{path}
    std::wstring url = L"https://raw.githubusercontent.com/";
    url += repoOwner + L"/" + repoName + L"/" + branch + L"/";
    
    // Remove leading slash if present
    std::wstring path = filePath;
    if (!path.empty() && path[0] == L'/') {
        path = path.substr(1);
    }
    
    url += path;
    return url;
}

void GitHubDownloader::EnsureDirectoryExists(const fs::path& filePath)
{
    auto directory = filePath.parent_path();
    if (!fs::exists(directory)) {
        fs::create_directories(directory);
    }
}

std::wstring GitHubDownloader::GetFileName(const std::wstring& filePath)
{
    size_t pos = filePath.find_last_of(L"/\\");
    if (pos != std::wstring::npos) {
        return filePath.substr(pos + 1);
    }
    return filePath;
}