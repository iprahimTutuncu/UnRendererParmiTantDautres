# Olaf Snow Simulator

## Build (Windows)

### Visual Studio

Open the directory with Visual Studio. If needed, adjust the generator in `CMakePresets.txt`.

If you encounter a problem, make sure your Visual Studio installation includes Windows 10/11 SDK, latest MSVC, and CMake tools. 

### CLI

1. Make sure winget is installed by opening a command prompt and running `winget --version`.  
1.1 If winget isn't installed, follow this tutorial https://www.youtube.com/watch?v=8BUA2vc-pXY

2. Install CMake if needed with `winget install -e --id Kitware.CMake`

3. Install Ninja if needed with `winget install -e --id Ninja-build.Ninja`.

4. Restart command prompt.

5. Run `build.bat` to build release. 

Alternatively, load x64 native tools and run the following:

```bash
`cmake --preset [preset]`
`cmake --build --preset [preset]`
```