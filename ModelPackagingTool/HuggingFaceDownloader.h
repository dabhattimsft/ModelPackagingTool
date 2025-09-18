#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <functional>
#include <memory>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Storage.Streams.h>

namespace fs = std::filesystem;

class HuggingFaceDownloader
{
public:
    // Progress reporting callback
    using ProgressCallback = std::function<void(const std::wstring& fileName, uint64_t bytesReceived, uint64_t totalBytes)>;

    HuggingFaceDownloader();
    ~HuggingFaceDownloader() = default;

    // Download a single file from HuggingFace
    winrt::Windows::Foundation::IAsyncAction DownloadFileAsync(
        const std::wstring& repoOwner,
        const std::wstring& repoName,
        const std::wstring& branch,
        const std::wstring& filePath,
        const fs::path& destinationPath,
        ProgressCallback progressCallback = nullptr);

    // Download all files from a HuggingFace folder
    winrt::Windows::Foundation::IAsyncAction DownloadFolderAsync(
        const std::wstring& repoOwner,
        const std::wstring& repoName,
        const std::wstring& branch,
        const std::wstring& folderPath,
        const fs::path& destinationFolder,
        ProgressCallback progressCallback = nullptr);

    // Cancel any ongoing downloads
    void CancelDownloads();

private:
    // Helper function to parse JSON response and extract file paths
    std::vector<std::wstring> ParseJsonFilesResponse(
        const std::string& jsonStr);

    // Build a download URL for a file in a HuggingFace repo
    std::wstring BuildDownloadUrl(
        const std::wstring& repoOwner,
        const std::wstring& repoName,
        const std::wstring& branch,
        const std::wstring& filePath);

    // Create necessary directories for a file path
    void EnsureDirectoryExists(const fs::path& filePath);

    // Extract file name from path
    std::wstring GetFileName(const std::wstring& filePath);

    // Http client for making requests
    winrt::Windows::Web::Http::HttpClient m_httpClient;
    
    // Flag to track cancellation requests
    bool m_cancelRequested;
};