# type: ignore  # Disable Pylance warnings

# Import required for PlatformIO build system
Import("env")

# Standard library imports for file operations
import os
import shutil
from pathlib import Path

def ensure_build_dirs(target=None, source=None, env=None):
    """
    Create and verify all necessary build directories for PlatformIO.
    This function runs before key build steps to ensure proper directory structure.
    
    Args:
        target, source, env: Standard SCons parameters (may be None during pre-build)
    """
    try:
        # Get build directory from environment or use default
        build_dir = env.subst("$BUILD_DIR") if env else ".pio/build/esp32dev"
        
        # Define all required directories in a dictionary for clarity
        required_dirs = {
            "main": Path(build_dir),
            "src": Path(build_dir) / "src",
            "spiffs": Path(build_dir) / "spiffs",
            "temp": Path(build_dir) / "tmp",
            "lib": Path(".pio") / "libdeps" / "esp32dev"
        }
        
        # Create each directory with proper permissions
        for name, directory in required_dirs.items():
            try:
                # Create directory if it doesn't exist
                directory.mkdir(parents=True, exist_ok=True)
                
                # Ensure directory has correct permissions (0o755 = rwxr-xr-x)
                directory.chmod(0o755)
                
                print(f"Verified directory: {directory}")
                
            except Exception as e:
                print(f"Warning: Issue with {name} directory ({directory}): {e}")
        
        # Clean up any stale build files that might cause issues
        stale_files = [
            Path(build_dir) / ".sconsign.dblite",
            Path(build_dir) / ".sconsign311.dblite",
            Path(build_dir) / ".sconsign311.tmp"
        ]
        
        for file_path in stale_files:
            try:
                if file_path.exists():
                    file_path.unlink()
                    print(f"Removed stale file: {file_path}")
            except Exception as e:
                print(f"Warning: Could not remove {file_path}: {e}")
        
        # Create .gitkeep file in src directory
        gitkeep = required_dirs["src"] / ".gitkeep"
        try:
            gitkeep.touch()
            print(f"Created/Updated .gitkeep in {required_dirs['src']}")
        except Exception as e:
            print(f"Warning: Could not create .gitkeep: {e}")
            
        # Verify write permissions by attempting a test write
        test_file = required_dirs["temp"] / "write_test"
        try:
            test_file.write_text("test")
            test_file.unlink()  # Clean up test file
            print("Write permission test passed")
        except Exception as e:
            print(f"Warning: Write permission test failed: {e}")
            
    except Exception as e:
        print(f"Error in build directory setup: {e}")
        raise  # Re-raise the exception to ensure PlatformIO knows about the failure

# Register function to run before specific build steps
build_targets = ["buildfs", "uploadfs", "buildprog", "upload"]
for target in build_targets:
    env.AddPreAction(target, ensure_build_dirs)

print("Build directory script initialized")

