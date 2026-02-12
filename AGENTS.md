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

### ESP-IDF Path

The `get_idf` alias is defined in `~/.zshrc` and sources:
```bash
. /Users/ethanpuchaty/Repos/side-projects/2024-badge-dev-env/esp-idf/export.sh
```

When running commands non-interactively (e.g. from scripts or `bash -c`), use the full source path instead of the alias:
```bash
. /Users/ethanpuchaty/Repos/side-projects/2024-badge-dev-env/esp-idf/export.sh
```

---

## Espressif QEMU (ESP32 Emulation)

### QEMU Binary Path

The Espressif QEMU fork is installed at:
```
/Users/ethanpuchaty/.espressif/tools/qemu-xtensa/esp_develop_9.0.0_20240606/qemu/bin/qemu-system-xtensa
```

If not installed, install via:
```bash
python $IDF_PATH/tools/idf_tools.py install qemu-xtensa
```

### Building for QEMU

QEMU builds require a clean build with the QEMU sdkconfig overlay. The base config is `sdkconfig.old` (the normal hardware sdkconfig) and `sdkconfig.ci.qemu` provides QEMU-specific overrides:

```bash
# Clean build for QEMU (required when switching between normal and QEMU mode)
rm -rf build
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.old;sdkconfig.ci.qemu" set-target esp32
idf.py build
```

**Note**: `set-target` renames `sdkconfig` to `sdkconfig.old`, so subsequent QEMU builds use `sdkconfig.old` as the base.

### Creating the Flash Image

QEMU requires a merged flash image padded to a power-of-2 size:

```bash
cd build
esptool.py --chip esp32 merge_bin --output flash_image.bin @flash_args
truncate -s $((16 * 1024 * 1024)) flash_image.bin
cd ..
```

### Running QEMU

Basic QEMU launch with OpenCores Ethernet (WiFi emulation) and serial ports:

```bash
~/.espressif/tools/qemu-xtensa/esp_develop_9.0.0_20240606/qemu/bin/qemu-system-xtensa \
    -nographic \
    -machine esp32 \
    -m 4M \
    -drive file=build/flash_image.bin,if=mtd,format=raw \
    -nic user,model=open_eth \
    -serial mon:stdio \
    -serial tcp:localhost:1234,server,nowait \
    -serial tcp:localhost:1235,server,nowait
```

Key flags:
- **`-nic user,model=open_eth`** — OpenCores Ethernet MAC with SLIRP NAT (provides network access through host)
- **`-m 4M`** — 4 MB PSRAM
- **`-serial mon:stdio`** — UART0 (console) on terminal
- **`-serial tcp:localhost:1234,server,nowait`** — UART1 (HCI/BLE bridge)
- **`-serial tcp:localhost:1235,server,nowait`** — UART2 (LED/Touch viz bridge)

### Running with the Full Visualization Stack

The `tools/run_viz.sh` script handles building, flash image creation, mock server, viz bridge, and QEMU launch in one command:

```bash
./tools/run_viz.sh                    # Full build + launch
./tools/run_viz.sh --no-build         # Skip build, use existing flash image
./tools/run_viz.sh --badge CREST      # Specify badge type
```

### Mock Game Server (WiFi Emulation)

For WiFi/network testing, start the mock game server before QEMU:

```bash
python3 tools/mock_game_server.py --port 9080
```

The QEMU guest reaches the host at `10.0.2.2` (standard QEMU SLIRP convention). The mock server URL is configured in `sdkconfig.ci.qemu` as `http://10.0.2.2:9080/heartbeat`.

---

### Project Structure

This is an ESP-IDF based project for the SplinterOps badge firmware. The main source files are located in the standard ESP-IDF directories:
- `main/` - Main application source code
- `components/` - Custom components (if any)
- `CMakeLists.txt` - Build configuration
