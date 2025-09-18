# ModelPackagingTool

A command-line utility for packaging machine learning models into MSIX packages for Windows applications.

## Overview

ModelPackagingTool simplifies the process of distributing machine learning models with Windows applications by creating MSIX packages from model files. It can either package local model files or directly download and package models from repositories like Hugging Face.

## Features

- **Package Local Models**: Convert local model files into MSIX packages
- **Download and Package**: Directly download models from repositories (Hugging Face, GitHub) and package them
- **Package Signing**: Sign packages with certificates for secure distribution
- **Certificate Generation**: Built-in tools for creating self-signed certificates

## Requirements

- Windows 10/11
- Windows SDK (for MakeAppx.exe and SignTool.exe)
- PowerShell (for certificate generation)
- Visual C++ Redistributable 2019 or newer

## Installation

1. Download the latest release from the releases page
2. Extract the files to a directory of your choice
3. Add the directory to your PATH (optional)

## Usage

### Package a Local Model

```
ModelPackagingTool /pack <path-to-folder> /name <package-name> /publisher <publisher-name> /o <output-dir>
```

### Download and Package a Model

```
ModelPackagingTool /downloadAndPack <uri> /o <output-dir> [/name <package-name>] [/publisher <publisher-name>]
```

When using `/downloadAndPack`, the package name and publisher can be inferred from the repository URI.

### Signing Packages

To sign packages, first generate a certificate:

```powershell
.\Scripts\GenerateMsixCertificate.ps1 -PublisherName "YourName"
```

Then use the certificate with the signing options:

```
ModelPackagingTool /pack <path-to-folder> /name <n> /publisher <p> /o <output-dir> /sign <cert-path>
```

For password-protected certificates:

```
ModelPackagingTool /pack <path-to-folder> /name <n> /publisher <p> /o <output-dir> /sign <cert-path> /pwd <password>
```

### Certificate Generation

The tool includes a PowerShell script for creating self-signed certificates:

1. Create a certificate in the current directory:
   ```powershell
   .\Scripts\GenerateMsixCertificate.ps1 -PublisherName "YourName"
   ```

2. Create a password-protected certificate:
   ```powershell
   .\Scripts\GenerateMsixCertificate.ps1 -PublisherName "YourName" -Password "YourPassword"
   ```

3. Specify a custom path:
   ```powershell
   .\Scripts\GenerateMsixCertificate.ps1 -PublisherName "YourName" -CertificatePath "C:\path\to\certificate.pfx"
   ```

## Command Reference

### Commands

- `/pack`: Package a local folder into an MSIX package
- `/downloadAndPack`: Download model files from a URI and package them
- `/help`: Show help information

### Options

- `/o <dir>`: Specify output directory (required)
- `/name <name>`: Specify package name (required for `/pack`)
- `/publisher <name>`: Specify publisher name (required for `/pack`)
- `/sign <cert-path>`: Sign the MSIX package with the specified certificate
- `/pwd <password>`: Specify password for certificate (only needed if certificate is password-protected)
- `/verbose`: Enable verbose output

## Examples

1. Package a local model:
   ```
   ModelPackagingTool /pack C:\Models\MyModel /name MyModel /publisher Contoso /o C:\Output
   ```

2. Download and package a model from Hugging Face:
   ```
   ModelPackagingTool /downloadAndPack https://huggingface.co/openai-community/gpt2/tree/main/onnx /o C:\Output
   ```

3. Download and package with custom name and publisher:
   ```
   ModelPackagingTool /downloadAndPack https://huggingface.co/openai-community/gpt2 /o C:\Output /name gpt2 /publisher openai-community
   ```

4. Package and sign with a certificate:
   ```
   ModelPackagingTool /pack C:\Models\MyModel /name MyModel /publisher Contoso /o C:\Output /sign C:\Certs\MyCert.pfx
   ```

5. Package and sign with a password-protected certificate:
   ```
   ModelPackagingTool /pack C:\Models\MyModel /name MyModel /publisher Contoso /o C:\Output /sign C:\Certs\MyCert.pfx /pwd mypassword
   ```

## Supported Model Repositories

- [Hugging Face](https://huggingface.co/)
- GitHub repositories

## Notes

- Signing is optional but recommended for production packages
- For development purposes, a self-signed certificate is sufficient
- Only use the `/pwd` option if your certificate is password-protected
- For production, consider using a certificate from a trusted certificate authority

## Building from Source

1. Clone the repository
2. Open the solution in Visual Studio 2019 or newer
3. Build the solution

## License

[License information here]

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.