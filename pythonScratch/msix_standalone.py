# msix_test.py
# Standalone MSIX packaging utility

import os
import sys
import subprocess
import tempfile
import shutil
import contextlib
from pathlib import Path

def create_appx_manifest(
    output_dir, 
    package_name="Olive", 
    publisher_name="Microsoft"
):
    """
    Create an AppxManifest.xml file for MSIX packaging.
    
    Args:
        output_dir: Directory to create the manifest
        package_name: Name of the package (typically repo name)
        publisher_name: Publisher name (typically repo owner)
        
    Returns:
        Path to the created manifest file
    """
    # Clean package and publisher names - remove spaces and special characters
    clean_package_name = ''.join(c for c in package_name if c.isalnum())
    clean_publisher_name = ''.join(c for c in publisher_name if c.isalnum())
    
    manifest_content = f"""<?xml version="1.0" encoding="utf-8"?>
<Package
  xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
  xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
  IgnorableNamespaces="uap">

  <Identity
    Name="{clean_package_name}ModelPackage"
    Publisher="CN={clean_publisher_name}"
    Version="1.0.0.0" />

  <Properties>
    <DisplayName>{clean_package_name} Model Package</DisplayName>
    <PublisherDisplayName>{clean_publisher_name}</PublisherDisplayName>
    <Logo>Images\\StoreLogo.png</Logo>
    <Framework>true</Framework>
  </Properties>

  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.17763.0" MaxVersionTested="10.0.22621.0" />
  </Dependencies>

  <Resources>
    <Resource Language="en-us" />
  </Resources>
</Package>"""

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    manifest_path = output_dir / "AppxManifest.xml"
    with open(manifest_path, "w", encoding="utf-8") as f:
        f.write(manifest_content)
        
    return manifest_path


def create_placeholder_images(output_dir):
    """
    Create placeholder image files required for MSIX packaging.
    
    Args:
        output_dir: Directory to create image files
    """
    output_dir = Path(output_dir)
    images_dir = output_dir / "Images"
    images_dir.mkdir(exist_ok=True, parents=True)
    
    try:
        # Create simple placeholder PNG files of required sizes
        from PIL import Image
        
        # Create a simple colored image for each required size
        sizes = {
            "AppList.png": (44, 44),
            "MedTile.png": (150, 150),
            "StoreLogo.png": (50, 50)
        }
        
        for filename, size in sizes.items():
            img = Image.new('RGBA', size, color=(0, 120, 212, 255))  # Microsoft blue
            img.save(images_dir / filename)
            print(f"Created image: {images_dir / filename}")
    except ImportError:
        print("PIL not installed. Creating empty placeholder files.")
        # Create empty files as placeholders
        for filename in ["AppList.png", "MedTile.png", "StoreLogo.png"]:
            with open(images_dir / filename, "wb") as f:
                f.write(b"")
            print(f"Created empty placeholder: {images_dir / filename}")


def create_msix_package(
    model_dir, 
    output_msix_path,
    package_name="Olive",
    publisher_name="Microsoft",
    packaging_dir=None
):
    """
    Create an MSIX package from a model directory.
    
    Args:
        model_dir: Directory containing the model files
        output_msix_path: Path to save the MSIX package
        package_name: Name of the package (defaults to "Olive")
        publisher_name: Publisher name (defaults to "Microsoft")
        packaging_dir: Specific directory to use for packaging. If None, a temporary directory will be created
        
    Returns:
        Path to the created MSIX package, or None if creation failed
    """
    # Ensure we're on Windows
    if os.name != 'nt':
        print("MSIX packaging is only supported on Windows.")
        return None
    
    model_dir = Path(model_dir)
    output_msix_path = Path(output_msix_path)
    
    # Convert packaging_dir to Path if provided
    if packaging_dir:
        packaging_dir = Path(packaging_dir)
    
    # Create a context manager that either uses the specified packaging_dir or creates a temp dir
    @contextlib.contextmanager
    def get_packaging_directory():
        if packaging_dir:
            # Copy model files to the packaging directory
            print(f"Copying model files from {model_dir} to {packaging_dir}")
            for item in model_dir.glob('**/*'):
                if item.is_file():
                    relative_path = item.relative_to(model_dir)
                    dest_path = packaging_dir / relative_path
                    dest_path.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(item, dest_path)
                    print(f"Copied {item} to {dest_path}")
            yield packaging_dir
        else:
            with tempfile.TemporaryDirectory() as temp_dir:
                temp_dir_path = Path(temp_dir)
                
                # Copy model files to temp dir
                print(f"Copying model files from {model_dir} to {temp_dir_path}")
                for item in model_dir.glob('**/*'):
                    if item.is_file():
                        relative_path = item.relative_to(model_dir)
                        dest_path = temp_dir_path / relative_path
                        dest_path.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(item, dest_path)
                        print(f"Copied {item} to {dest_path}")
                yield temp_dir_path
    
    # Use the packaging directory (either model_dir or temp_dir)
    with get_packaging_directory() as dir_path:
        # Create AppxManifest.xml
        print("Creating AppxManifest.xml")
        manifest_path = create_appx_manifest(dir_path, package_name, publisher_name)
        print(f"Created manifest at {manifest_path}")
        
        # Create placeholder images
        print("Creating placeholder images")
        create_placeholder_images(dir_path)
        
        # Ensure output directory exists
        output_msix_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Remove existing file to avoid overwrite prompt
        if output_msix_path.exists():
            print(f"Removing existing file {output_msix_path}")
            os.remove(output_msix_path)
        
        # Call makeappx.exe to create the MSIX package
        try:
            # Try to find makeappx in the Windows SDK using Windows Registry
            makeappx_path = None
            
            try:
                import winreg
                
                # Open the Windows Kits registry key
                with winreg.OpenKey(
                    winreg.HKEY_LOCAL_MACHINE,
                    r"SOFTWARE\Microsoft\Windows Kits\Installed Roots"
                ) as key:
                    # Get the Windows SDK installation path
                    sdk_root = winreg.QueryValueEx(key, "KitsRoot10")[0]
                    
                    # Get all version directories
                    sdk_versions = []
                    i = 0
                    while True:
                        try:
                            version = winreg.EnumKey(key, i)
                            if version.startswith("10."):
                                sdk_versions.append(version)
                            i += 1
                        except OSError:
                            break
                    
                    # Sort versions to get the latest
                    sdk_versions.sort(reverse=True)
                    
                    # Try each version, starting with the latest
                    for version in sdk_versions:
                        bin_path = Path(sdk_root) / "bin" / version / "x64" / "makeappx.exe"
                        if bin_path.exists():
                            makeappx_path = str(bin_path)
                            print(f"Found makeappx.exe in Windows SDK version {version}")
                            break
                    
                    # If no version-specific path found, try the x64 folder directly
                    if not makeappx_path:
                        bin_path = Path(sdk_root) / "bin" / "x64" / "makeappx.exe"
                        if bin_path.exists():
                            makeappx_path = str(bin_path)
                            print("Found makeappx.exe in Windows SDK common bin folder")
            except (ImportError, FileNotFoundError, OSError) as e:
                print(f"Could not query Windows SDK from registry: {str(e)}")
                
                # Fall back to well-known paths if registry query fails
                sdk_paths = [
                    "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22621.0\\x64\\makeappx.exe",
                    "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.19041.0\\x64\\makeappx.exe",
                    "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.18362.0\\x64\\makeappx.exe",
                    "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.17763.0\\x64\\makeappx.exe",
                    "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\makeappx.exe"
                ]
                
                for path in sdk_paths:
                    if os.path.exists(path):
                        makeappx_path = path
                        print(f"Found makeappx.exe at hardcoded path: {path}")
                        break
            
            # Try environment variable as a last resort
            if not makeappx_path:
                sdk_path = os.environ.get("WindowsSdkDir")
                if sdk_path:
                    for bin_dir in Path(sdk_path).glob("**/x64"):
                        possible_path = bin_dir / "makeappx.exe"
                        if possible_path.exists():
                            makeappx_path = str(possible_path)
                            print(f"Found makeappx.exe in WindowsSdkDir: {makeappx_path}")
                            break
            
            # If not found in SDK, use PATH
            if not makeappx_path:
                makeappx_path = "makeappx.exe"
                print("Using makeappx.exe from PATH - may fail if not in PATH")
                
            print(f"Using makeappx at: {makeappx_path}")
            
            # Create the MSIX package
            cmd = [
                makeappx_path,
                "pack", 
                "/d", str(dir_path),
                "/p", str(output_msix_path)
            ]
            
            print(f"Creating MSIX package with command: {' '.join(cmd)}")
            process = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=False
            )
            
            if process.returncode != 0:
                print(f"Failed to create MSIX package: {process.stderr}")
                print(f"makeappx output: {process.stdout}")
                return None
                
            print(f"Successfully created MSIX package at {output_msix_path}")
            return output_msix_path
            
        except FileNotFoundError as e:
            print(f"makeappx.exe not found: {str(e)}")
            print("Please install Windows SDK or add it to PATH.")
            return None
        except Exception as e:
            print(f"Error creating MSIX package: {str(e)}")
            return None


def test_create_msix():
    """Test creating a complete MSIX package"""
    # First create a simple test model directory
    model_dir = Path("./temp_model_dir")
    model_dir.mkdir(exist_ok=True)
    
    # Create a dummy model file
    with open(model_dir / "model.onnx", "w") as f:
        f.write("This is a dummy ONNX model file for testing")
    
    # Create a specific packaging directory
    package_dir = Path("./to_be_packed")
    if package_dir.exists():
        shutil.rmtree(package_dir)
    package_dir.mkdir(exist_ok=True)
    
    try:
        output_path = Path("./test_model.msix")
        result = create_msix_package(
            model_dir=model_dir,
            output_msix_path=output_path,
            package_name="Olive",
            publisher_name="Microsoft",
            packaging_dir=package_dir
        )
        
        if result:
            print(f"Successfully created MSIX package at {result}")
        else:
            print("Failed to create MSIX package")
    except Exception as e:
        print(f"Error creating MSIX package: {str(e)}")


if __name__ == "__main__":
    print("Testing standalone MSIX package creation...")
    
    # Run the test
    test_create_msix()
    
    print("Test completed")