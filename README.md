# OpenLuminaire-Software
Software for OSLUV OpenLuminaire - an [OSHWA certified](https://certification.oshwa.org/us002747.html) Open Source 222nm luminiare.

![certification-mark-US002747-stacked](https://github.com/user-attachments/assets/749f9e49-821d-415f-969e-e3ed7226dc48)


# OpenLuminaire Software Build Instructions

These instructions will guide you through building the OpenLuminaire software for the RP2040-based hardware.

## 1. Prerequisites

Before you begin, ensure you have the necessary environment set up based on your operating system:

* **Windows:**
    * Install and activate **Windows Subsystem for Linux (WSL)**.
    * Install **Ubuntu 24.04 LTS** (or newer) from the Microsoft Store. Earlier versions are not recommended.
    * Ensure your WSL environment is active for the following steps.

* **macOS:**
    * Install **Homebrew** by following the instructions at [brew.sh](https://brew.sh/).
    * You will need to use Homebrew to install the equivalent build tools (see step 2).

* **Linux:**
    * We recommend using **Ubuntu 24.04 LTS**.
    * If using another distribution, ensure you can install the equivalent packages.

## Install Build Tools

Once your environment is ready, install the required build tools and libraries:

* **On Ubuntu / WSL (Ubuntu):**
    ```bash
    sudo apt update && sudo apt install cmake python3 build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
    ```
* **On macOS (using Homebrew):**
    * You will need to install `cmake`, `python3`, and an ARM GCC toolchain. You might need to search for the specific Homebrew formulas, but generally:
        ```bash
        brew install cmake python3 arm-none-eabi-gcc
        ```
    * *Note: Specific library names might differ; you may need to find equivalents for `libnewlib` and `libstdc++-arm-none-eabi-newlib` or confirm they are included with the toolchain.*

## Clone Repositories

Next, you need to download the source code for the Pico SDK and the OpenLuminaire Software.

1.  **Clone the Pico SDK:**
    * It is crucial to use the `--recursive` flag to fetch all necessary submodules.
    * Choose a location for these files (e.g., your home directory).
    ```bash
    git clone --recursive https://github.com/raspberrypi/pico-sdk.git
    ```

2.  **Clone the OpenLuminaire Software:**
    ```bash
    git clone https://github.com/OSLUV/OpenLuminaire-Software.git
    ```

## Build the Firmware

Now, you can compile the software.

1.  **Navigate to the Source Directory:**
    ```bash
    cd OpenLuminaire-Software/rp
    ```

2.  **Create a Build Directory:**
    * It's standard practice to build outside the source directory.
    ```bash
    mkdir build
    cd build
    ```

3.  **Run CMake:**
    * This step configures the build system.
    * You **must** tell CMake where to find the Pico SDK you cloned earlier.
    * Replace `{path_where_you_cloned_pico-sdk.git_to}` with the *full, absolute path* to the `pico-sdk` directory.
    ```bash
    cmake ../ -DPICO_SDK_PATH={path_where_you_cloned_pico-sdk.git_to}
    ```
    * *Example: If you cloned `pico-sdk` into `/home/user/pico-sdk`, the command would be:*
        ```bash
        cmake ../ -DPICO_SDK_PATH=/home/user/pico-sdk
        ```

4.  **Compile the Code:**
    ```bash
    make
    ```

5.  **Locate the Firmware File:**
    * If the build is successful, it will generate a file named `app.uf2` inside the `build` directory. This is the file you'll upload to your lamp.

## Flash the Firmware

Follow these steps to upload the `app.uf2` file to your OpenLuminaire lamp:

1.  **Disconnect External Power:** Ensure the lamp is **NOT** connected to power via the barrel jack. It must only be powered via USB for this step.
2.  **Locate the Bootloader Button:** Find the small, recessed button near the LCD display.
3.  **Enter Bootloader Mode:** **Press and hold** the bootloader button.
4.  **Connect USB:** While still holding the button, plug the lamp into your computer's USB port.
5.  **Mount as Drive:** The lamp should appear on your computer as a USB mass storage device (like a flash drive), usually named `RPI-RP2`. You can now release the bootloader button.
6.  **Copy the Firmware:** Drag and drop the `app.uf2` file (from your `build` directory) onto this `RPI-RP2` drive.
7.  **Auto-Restart:** The lamp will automatically copy the file, eject itself, and restart with the new firmware.

**Done!** Your OpenLuminaire lamp is now updated.

# Dev Instructions

## Changing the firmware splash image

The easiest way to change the splash image that appears when the lamp first turns on is by using GIMP.

1. Download and install GIMP at https://www.gimp.org/downloads/
2. Open the desired image in GIMP. Resize it to 240x240 pixels. 
3. Go to `File > Export as...` and rename the image to `image.c`
4. In the export menu, use these settings.
    - [ ] Save comment to file
	- [ ] Use GLib types (guint8*)
	- [ ] Use macros instead of struct
	- [ ] Use 1 bit Run-Length-Encoding
	- [x] **Save alpha channel (RGBA/RGB)**
	- [x] **Save as RGB565 (16-bit)**
5. Drop the `image.c` file in `OpenLuminaire-Software/v4/rp2040`
6. Rebuild with `make` from inside the build directory and reflash. 
