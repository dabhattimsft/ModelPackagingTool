#pragma once

#include <string>

namespace AppxManifestTemplates {

    // Standard template for model packages
    inline std::wstring GetStandardManifestTemplate(
        const std::wstring& cleanPackageName,
        const std::wstring& cleanPublisherName)
    {
        std::wstring manifestContent = 
            L"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            L"<Package\r\n"
            L"  xmlns=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10\"\r\n"
            L"  xmlns:uap=\"http://schemas.microsoft.com/appx/manifest/uap/windows10\"\r\n"
            L"  xmlns:uap15=\"http://schemas.microsoft.com/appx/manifest/uap/windows10/15\"\r\n"
            L"  IgnorableNamespaces=\"uap uap15\">\r\n"
            L"\r\n"
            L"  <Identity\r\n"
            L"    Name=\"" + cleanPackageName + L"ModelPackage\"\r\n"
            L"    Publisher=\"CN=" + cleanPublisherName + L"\"\r\n"
            L"    Version=\"1.0.0.0\" />\r\n"
            L"\r\n"
            L"  <Properties>\r\n"
            L"    <DisplayName>" + cleanPackageName + L" Model Package</DisplayName>\r\n"
            L"    <PublisherDisplayName>" + cleanPublisherName + L"</PublisherDisplayName>\r\n"
            L"    <Logo>Images\\StoreLogo.png</Logo>\r\n"
            L"    <uap15:DependencyTarget>true</uap15:DependencyTarget>\r\n"
            L"  </Properties>\r\n"
            L"\r\n"
            L"  <Dependencies>\r\n"
            L"    <TargetDeviceFamily Name=\"Windows.Desktop\" MinVersion=\"10.0.17763.0\" MaxVersionTested=\"10.0.22621.0\" />\r\n"
            L"  </Dependencies>\r\n"
            L"\r\n"
            L"  <Resources>\r\n"
            L"    <Resource Language=\"en-us\" />\r\n"
            L"  </Resources>\r\n"
            L"\r\n"
            L"  <Applications>\r\n"
            L"    <Application Id=\"App\">\r\n"
            L"      <uap:VisualElements\r\n"
            L"        DisplayName=\"" + cleanPackageName + L" Model Package\"\r\n"
            L"        Description=\"" + cleanPackageName + L" Model Package\"\r\n"
            L"        BackgroundColor=\"transparent\"\r\n"
            L"        Square150x150Logo=\"Images\\MedTile.png\"\r\n"
            L"        Square44x44Logo=\"Images\\AppList.png\"\r\n"
            L"        AppListEntry=\"none\">\r\n"
            L"      </uap:VisualElements>\r\n"
            L"    </Application>\r\n"
            L"  </Applications>\r\n"
            L"</Package>";

        return manifestContent;
    }

    // Framework package template - doesn't include Applications node
    inline std::wstring GetFrameworkManifestTemplate(
        const std::wstring& cleanPackageName,
        const std::wstring& cleanPublisherName)
    {
        std::wstring manifestContent = 
            L"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            L"<Package\r\n"
            L"  xmlns=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10\"\r\n"
            L"  xmlns:uap=\"http://schemas.microsoft.com/appx/manifest/uap/windows10\"\r\n"
            L"  xmlns:rescap=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities\"\r\n"
            L"  IgnorableNamespaces=\"uap rescap\">\r\n"
            L"\r\n"
            L"  <Identity\r\n"
            L"    Name=\"" + cleanPackageName + L"Framework\"\r\n"
            L"    Publisher=\"CN=" + cleanPublisherName + L"\"\r\n"
            L"    Version=\"1.0.0.0\" />\r\n"
            L"\r\n"
            L"  <Properties>\r\n"
            L"    <DisplayName>" + cleanPackageName + L" Framework Package</DisplayName>\r\n"
            L"    <PublisherDisplayName>" + cleanPublisherName + L"</PublisherDisplayName>\r\n"
            L"    <Logo>Images\\StoreLogo.png</Logo>\r\n"
            L"    <Framework>true</Framework>\r\n"
            L"  </Properties>\r\n"
            L"\r\n"
            L"  <Dependencies>\r\n"
            L"    <TargetDeviceFamily Name=\"Windows.Desktop\" MinVersion=\"10.0.17763.0\" MaxVersionTested=\"10.0.22621.0\" />\r\n"
            L"  </Dependencies>\r\n"
            L"\r\n"
            L"  <Resources>\r\n"
            L"    <Resource Language=\"en-us\" />\r\n"
            L"  </Resources>\r\n"
            L"\r\n"
            L"  <Capabilities>\r\n"
            L"    <rescap:Capability Name=\"runFullTrust\" />\r\n"
            L"  </Capabilities>\r\n"
            L"</Package>";

        return manifestContent;
    }

    // Additional templates can be added here for different types of packages
}