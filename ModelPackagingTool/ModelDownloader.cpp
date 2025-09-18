#include "ModelDownloader.h"
#include <iostream>
#include <regex>

ModelDownloader::ModelDownloader()
{
}

RepositoryInfo ModelDownloader::ParseUri(const std::wstring& uri)
{
    RepositoryInfo info;
    
    // Parse HuggingFace URI: https://huggingface.co/{owner}/{repo}/tree/{branch}/{path}
    // or https://huggingface.co/{owner}/{repo}
    std::wregex huggingFacePattern(L"https?://(?:www\\.)?huggingface\\.co/([^/]+)/([^/]+)(?:/tree/([^/]+)(?:/(.+))?)?");
    
    // Parse GitHub URI: https://github.com/{owner}/{repo}/blob/{branch}/{path}
    // or https://github.com/{owner}/{repo}
    std::wregex githubPattern(L"https?://(?:www\\.)?github\\.com/([^/]+)/([^/]+)(?:/(?:blob|tree)/([^/]+)(?:/(.+))?)?");
    
    std::wsmatch matches;
    
    if (std::regex_match(uri, matches, huggingFacePattern)) {
        info.type = RepositoryType::HuggingFace;
        info.owner = matches[1].str();
        info.name = matches[2].str();
        
        // Branch is optional, default to "main"
        if (matches[3].matched) {
            info.branch = matches[3].str();
        }
        
        // Path is optional
        if (matches[4].matched) {
            info.path = matches[4].str();
        }
    }
    else if (std::regex_match(uri, matches, githubPattern)) {
        info.type = RepositoryType::GitHub;
        info.owner = matches[1].str();
        info.name = matches[2].str();
        
        // Branch is optional, default to "main"
        if (matches[3].matched) {
            info.branch = matches[3].str();
        }
        
        // Path is optional
        if (matches[4].matched) {
            info.path = matches[4].str();
        }
    }
    
    return info;
}

winrt::Windows::Foundation::IAsyncAction ModelDownloader::DownloadModelAsync(
    const std::wstring& uri,
    const fs::path& destinationFolder,
    ProgressCallback progressCallback)
{
    // Parse the URI to determine the repository type and components
    RepositoryInfo repoInfo = ParseUri(uri);
    
    switch (repoInfo.type) {
        case RepositoryType::HuggingFace:
            co_await DownloadFromHuggingFaceAsync(repoInfo, destinationFolder, progressCallback);
            break;
            
        case RepositoryType::GitHub:
            co_await DownloadFromGitHubAsync(repoInfo, destinationFolder, progressCallback);
            break;
            
        default:
            throw winrt::hresult_invalid_argument(L"Unsupported repository URI format");
    }
}

void ModelDownloader::CancelDownloads()
{
    m_huggingFaceDownloader.CancelDownloads();
    m_githubDownloader.CancelDownloads();
}

winrt::Windows::Foundation::IAsyncAction ModelDownloader::DownloadFromHuggingFaceAsync(
    const RepositoryInfo& repoInfo,
    const fs::path& destinationFolder,
    ProgressCallback progressCallback)
{
    // Always treat the path as a folder path and use the API to list and download files
    // Ensure the path is properly formatted for use with the HuggingFace API
    std::wstring folderPath = repoInfo.path;
    
    // If no path is specified, use the root folder
    if (folderPath.empty()) {
        // Use the API to download files from the root of the repository
        co_await m_huggingFaceDownloader.DownloadFolderAsync(
            repoInfo.owner,
            repoInfo.name,
            repoInfo.branch,
            L"/", // Root folder
            destinationFolder,
            progressCallback
        );
    }
    else {
        // Add trailing slash if not present to indicate a folder
        if (folderPath.back() != L'/' && folderPath.back() != L'\\') {
            folderPath += L'/';
        }
        
        // Download all files in the specified folder
        co_await m_huggingFaceDownloader.DownloadFolderAsync(
            repoInfo.owner,
            repoInfo.name,
            repoInfo.branch,
            folderPath,
            destinationFolder,
            progressCallback
        );
    }
}

winrt::Windows::Foundation::IAsyncAction ModelDownloader::DownloadFromGitHubAsync(
    const RepositoryInfo& repoInfo,
    const fs::path& destinationFolder,
    ProgressCallback progressCallback)
{
    // Similar to HuggingFace, always treat the path as a folder path
    std::wstring folderPath = repoInfo.path;
    
    // If no path is specified, use the root folder
    if (folderPath.empty()) {
        // Use the API to download files from the root of the repository
        co_await m_githubDownloader.DownloadFolderAsync(
            repoInfo.owner,
            repoInfo.name,
            repoInfo.branch,
            L"/", // Root folder
            destinationFolder,
            progressCallback
        );
    }
    else {
        // Add trailing slash if not present to indicate a folder
        if (folderPath.back() != L'/' && folderPath.back() != L'\\') {
            folderPath += L'/';
        }
        
        // Download all files in the specified folder
        co_await m_githubDownloader.DownloadFolderAsync(
            repoInfo.owner,
            repoInfo.name,
            repoInfo.branch,
            folderPath,
            destinationFolder,
            progressCallback
        );
    }
}