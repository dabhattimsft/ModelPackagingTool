#pragma once

#include <string>
#include <filesystem>
#include <functional>
#include <memory>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include "HuggingFaceDownloader.h"
#include "GitHubDownloader.h"

namespace fs = std::filesystem;

// Supported repository types
enum class RepositoryType
{
    Unknown,
    HuggingFace,
    GitHub
};

// Structure to hold parsed URI information
struct RepositoryInfo
{
    RepositoryType type = RepositoryType::Unknown;
    std::wstring owner;
    std::wstring name;
    std::wstring branch = L"main";
    std::wstring path;
};

class ModelDownloader
{
public:
    // Progress reporting callback
    using ProgressCallback = std::function<void(const std::wstring& fileName, uint64_t bytesReceived, uint64_t totalBytes)>;

    ModelDownloader();
    ~ModelDownloader() = default;

    // Parse a URI to determine repository type and components
    RepositoryInfo ParseUri(const std::wstring& uri);

    // Download model files from a URI
    winrt::Windows::Foundation::IAsyncAction DownloadModelAsync(
        const std::wstring& uri,
        const fs::path& destinationFolder,
        ProgressCallback progressCallback = nullptr);

    // Cancel any ongoing downloads
    void CancelDownloads();

private:
    // Download model from HuggingFace
    winrt::Windows::Foundation::IAsyncAction DownloadFromHuggingFaceAsync(
        const RepositoryInfo& repoInfo,
        const fs::path& destinationFolder,
        ProgressCallback progressCallback);

    // Download model from GitHub
    winrt::Windows::Foundation::IAsyncAction DownloadFromGitHubAsync(
        const RepositoryInfo& repoInfo,
        const fs::path& destinationFolder,
        ProgressCallback progressCallback);

    // Downloaders for different repositories
    HuggingFaceDownloader m_huggingFaceDownloader;
    GitHubDownloader m_githubDownloader;
};