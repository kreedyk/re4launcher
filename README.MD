# Resident Evil 4 Launcher and Patcher Tool
A customized launcher for Resident Evil 4 (UHD) that provides executable patching and DLL injection.

I created this tool as a complement to my [Internal Trainer](https://www.patreon.com/posts/internal-trainer-123898766)

![re4launcher](https://i.imgur.com/MPJvb8M.png)

## Features
- Automatically detects and locates Resident Evil 4 installation
- Patches the game executable with a modified 4GB Patch approch (to enable my trainer injection)
- Sets windowed mode automatically in the game configuration (trainer requirement)
- Includes a built-in DLL injector for manual injection
- Verifies executable version and offers replacement if needed
- Monitors game process and handles lifecycle management

## Building
### Using Visual Studio
1. Open the solution file (.sln) in Visual Studio
2. Select the Release/Win32 configuration
3. Build the solution

### Using CMake
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```
The executable will be in the `build/bin/Release` directory.

## Usage
1. Launch the application
2. If the game is not automatically detected, browse for `bio4.exe`
3. Click "Play Game" to launch the game with all patches applied
4. To inject a DLL, switch to the "DLL Injector" tab, select your DLL, and click "Inject DLL"

## Command Line Options
- `--replace-exe` - Replaces the game executable with the embedded correct version

## Special Thanks
- [Dear ImGui](https://github.com/ocornut/imgui) for the user interface.# re4launcher
