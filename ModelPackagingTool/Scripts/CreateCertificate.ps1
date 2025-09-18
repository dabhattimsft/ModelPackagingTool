# PowerShell script to create a self-signed certificate
# Parameters:
#   - PublisherName: The publisher name to use in the certificate
#   - CertificatePath: The full path where to save the PFX file
#   - Password: The password to protect the certificate (optional)

param(
    [Parameter(Mandatory=$true)]
    [string]$PublisherName,
    
    [Parameter(Mandatory=$true)]
    [string]$CertificatePath,
    
    [Parameter(Mandatory=$false)]
    [string]$Password = ""
)

# Create a self-signed certificate
$cert = New-SelfSignedCertificate -Type Custom `
    -Subject "CN=$PublisherName" `
    -KeyUsage DigitalSignature `
    -FriendlyName "$PublisherName MSIX Signing Certificate" `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")

# Create a secure string for the password
$securePassword = ConvertTo-SecureString -String $Password -Force -AsPlainText

# Export the certificate to a PFX file
Export-PfxCertificate -Cert $cert -FilePath $CertificatePath -Password $securePassword

# Output the certificate thumbprint
Write-Host "Certificate created with thumbprint: $($cert.Thumbprint)"