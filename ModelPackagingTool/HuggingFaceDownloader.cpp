#include "HuggingFaceDownloader.h"
#include <iostream>
#include <fstream>
#include <winerror.h> // For E_FAIL
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Web.Http.Headers.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::Http;
using namespace Windows::Web::Http::Headers;
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

winrt::Windows::Foundation::IAsyncAction HuggingFaceDownloader::DownloadFolderAsync(
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
    
    // The API for fetching folder contents is not straightforward for HuggingFace
    // For simplicity in this implementation, we'd need to know file paths in advance
    // or parse the HTML content of the repository page
    
    // This is a placeholder for a more sophisticated implementation
    // In a real implementation, you would:
    // 1. Query the repository API to get a list of files in the folder
    // 2. Download each file one by one
    
    // For demonstration, let's just log that this would require additional API handling
    // and return an empty async action
    
    co_return;
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