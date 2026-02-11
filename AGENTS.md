# ESP-IDF Development Environment Setup

## Required Terminal Setup

**IMPORTANT**: All terminal sessions for building, flashing, or any ESP-IDF development tasks MUST be performed after the `get_idf` alias has been run.

### How to Start a Terminal Session

Before running any ESP-IDF commands (build, flash, monitor, etc.), you must:

1. Start a new terminal session
2. Run the following command:
   ```bash
   get_idf
   ```

This command sets up the ESP-IDF environment variables and paths necessary for all development operations.

### Common Development Commands

After running `get_idf`, you can use these standard ESP-IDF commands:

- **Build the project**: `idf.py build`
- **Flash to device**: `idf.py flash`
- **Monitor serial output**: `idf.py monitor`
- **Clean build**: `idf.py clean`
- **Full clean**: `idf.py fullclean`
- **Build, flash, and monitor**: `idf.py build flash monitor`

### Why This is Required

The `get_idf` command:
- Sets up the ESP-IDF toolchain paths
- Configures environment variables for the compiler, linker, and other tools
- Ensures compatibility with the specific ESP-IDF version used in this project
- Prevents build errors and flashing issues

### Troubleshooting

If you encounter build errors or "command not found" errors:
1. Close your current terminal
2. Open a new terminal
3. Run `get_idf` before attempting any ESP-IDF commands

### Project Structure

This is an ESP-IDF based project for the SplinterOps badge firmware. The main source files are located in the standard ESP-IDF directories:
- `main/` - Main application source code
- `components/` - Custom components (if any)
- `CMakeLists.txt` - Build configuration
