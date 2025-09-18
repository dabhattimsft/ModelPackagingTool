# GenerateMsixCertificate.ps1
# PowerShell script to create a self-signed certificate for MSIX package signing
# 
# Usage:
#   .\GenerateMsixCertificate.ps1 -PublisherName "YourName" [-CertificatePath "C:\path\to\output.pfx"] [-Password "YourPassword"]
#
# Parameters:
#   -PublisherName:    The publisher name to use in the certificate CN field
#   -CertificatePath:  The full path where to save the PFX certificate file (default is .\<PublisherName>.pfx in current directory)
#   -Password:         Optional password to protect the certificate (default is no password)

param(
    [Parameter(Mandatory=$true)]
    [string]$PublisherName,
    
    [Parameter(Mandatory=$false)]
    [string]$CertificatePath = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Password
)

# Check if the password was explicitly provided
$passwordProvided = $PSBoundParameters.ContainsKey('Password')

# If certificate path is not specified, use current directory with publisher name
if ([string]::IsNullOrEmpty($CertificatePath)) {
    $safePublisherName = $PublisherName -replace '[^a-zA-Z0-9_.-]', '_'
    $CertificatePath = Join-Path -Path (Get-Location) -ChildPath "$safePublisherName.pfx"
    Write-Host "Certificate path not specified. Using current directory: $CertificatePath" -ForegroundColor Yellow
}

# Show script header
Write-Host "MSIX Certificate Generator" -ForegroundColor Cyan
Write-Host "------------------------" -ForegroundColor Cyan
Write-Host "This script will create a self-signed certificate for MSIX package signing."
Write-Host "Publisher Name: $PublisherName"
Write-Host "Certificate Path: $CertificatePath"
if ($passwordProvided) {
    Write-Host "Certificate Password: [Custom password provided]"
} else {
    Write-Host "Certificate Password: [No password]"
}
Write-Host

# Create the directory for the certificate if it doesn't exist
$certificateDir = Split-Path -Path $CertificatePath -Parent
if (!(Test-Path -Path $certificateDir)) {
    try {
        New-Item -ItemType Directory -Path $certificateDir -Force | Out-Null
        Write-Host "Created directory: $certificateDir" -ForegroundColor Green
    }
    catch {
        Write-Host "Error creating directory: $_" -ForegroundColor Red
        exit 1
    }
}

# Create a self-signed certificate
try {
    Write-Host "Creating self-signed certificate..." -NoNewline
    
    $cert = New-SelfSignedCertificate -Type Custom `
        -Subject "CN=$PublisherName" `
        -KeyUsage DigitalSignature `
        -FriendlyName "$PublisherName MSIX Signing Certificate" `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
    
    Write-Host " Done!" -ForegroundColor Green
    Write-Host "Certificate created with thumbprint: $($cert.Thumbprint)"
}
catch {
    Write-Host " Failed!" -ForegroundColor Red
    Write-Host "Error creating certificate: $_" -ForegroundColor Red
    exit 1
}

# Export the certificate to a PFX file
try {
    Write-Host "Exporting certificate to $CertificatePath..." -NoNewline
    
    # Only use password if it was explicitly provided
    if ($passwordProvided) {
        $securePassword = ConvertTo-SecureString -String $Password -Force -AsPlainText
        Export-PfxCertificate -Cert $cert -FilePath $CertificatePath -Password $securePassword | Out-Null
    } else {
        # Export without password
        $securePassword = New-Object System.Security.SecureString
        Export-PfxCertificate -Cert $cert -FilePath $CertificatePath -Password $securePassword | Out-Null
    }
    
    Write-Host " Done!" -ForegroundColor Green
}
catch {
    Write-Host " Failed!" -ForegroundColor Red
    Write-Host "Error exporting certificate: $_" -ForegroundColor Red
    exit 1
}

# Success message
Write-Host
Write-Host "Certificate successfully created at: $CertificatePath" -ForegroundColor Green
Write-Host
Write-Host "To use this certificate with ModelPackagingTool, add the following parameters:"

if ($passwordProvided) {
    Write-Host "/sign `"$CertificatePath`" /pwd `"$Password`"" -ForegroundColor Yellow
    Write-Host
    Write-Host "Example:"
    Write-Host "ModelPackagingTool /pack C:\Models\MyModel /name MyModel /publisher `"$PublisherName`" /o C:\Output /sign `"$CertificatePath`" /pwd `"$Password`"" -ForegroundColor Yellow
} else {
    Write-Host "/sign `"$CertificatePath`"" -ForegroundColor Yellow
    Write-Host
    Write-Host "Example:"
    Write-Host "ModelPackagingTool /pack C:\Models\MyModel /name MyModel /publisher `"$PublisherName`" /o C:\Output /sign `"$CertificatePath`"" -ForegroundColor Yellow
}