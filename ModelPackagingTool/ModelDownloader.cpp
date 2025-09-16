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
    // If a specific path is provided, download just that file or folder
    if (!repoInfo.path.empty()) {
        // Check if the path appears to be a file (no trailing slash)
        if (repoInfo.path.back() != L'/' && repoInfo.path.back() != L'\\') {
            // Looks like a file path
            fs::path filePath = destinationFolder / fs::path(repoInfo.path).filename();
            co_await m_huggingFaceDownloader.DownloadFileAsync(
                repoInfo.owner,
                repoInfo.name,
                repoInfo.branch,
                repoInfo.path,
                filePath,
                progressCallback
            );
        }
        else {
            // Looks like a folder path
            co_await m_huggingFaceDownloader.DownloadFolderAsync(
                repoInfo.owner,
                repoInfo.name,
                repoInfo.branch,
                repoInfo.path,
                destinationFolder,
                progressCallback
            );
        }
    }
    else {
        // No specific path provided, download the entire repo
        // For HuggingFace, we would need to implement this with their API
        // For now, throw an error
        throw winrt::hresult_not_implemented(L"Downloading entire HuggingFace repositories is not yet supported");
    }
}

winrt::Windows::Foundation::IAsyncAction ModelDownloader::DownloadFromGitHubAsync(
    const RepositoryInfo& repoInfo,
    const fs::path& destinationFolder,
    ProgressCallback progressCallback)
{
    // If a specific path is provided, download just that file or folder
    if (!repoInfo.path.empty()) {
        // Check if the path appears to be a file (no trailing slash)
        if (repoInfo.path.back() != L'/' && repoInfo.path.back() != L'\\') {
            // Looks like a file path
            fs::path filePath = destinationFolder / fs::path(repoInfo.path).filename();
            co_await m_githubDownloader.DownloadFileAsync(
                repoInfo.owner,
                repoInfo.name,
                repoInfo.branch,
                repoInfo.path,
                filePath,
                progressCallback
            );
        }
        else {
            // Looks like a folder path
            co_await m_githubDownloader.DownloadFolderAsync(
                repoInfo.owner,
                repoInfo.name,
                repoInfo.branch,
                repoInfo.path,
                destinationFolder,
                progressCallback
            );
        }
    }
    else {
        // No specific path provided, download the entire repo
        // For GitHub, we would need to implement this with their API
        // For now, throw an error
        throw winrt::hresult_not_implemented(L"Downloading entire GitHub repositories is not yet supported");
    }
}