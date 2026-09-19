# Basecamp - Vulkan Rendering Engine

A GPU-first Vulkan rendering sandbox for learning and experimentation with graphics and simulation techniques.

## Project Overview

Basecamp is designed to support incremental, testable graphics milestones with a modular architecture. Current capabilities include:

- **Vulkan Core**: Instance/device/surface creation, swapchain management, synchronization primitives
- **Dynamic Rendering**: Modern VK_KHR_dynamic_rendering path
- **Shader Pipeline**: HLSL compilation to SPIR-V via DXC
- **Resource Management**: Buffer/image allocation via Vulkan Memory Allocator (VMA)
- **Bindless Rendering**: Descriptor-based texture sampling
- **Compute Support**: Technique registration and compute pipeline dispatch
- **Raytracing**: BLAS/TLAS acceleration structures, ray tracing pipelines, and SBT setup
- **Framegraph**: Compute-to-raster sequencing with automatic synchronization

## Prerequisites

### Windows (MSVC)
- **Visual Studio 2022** or later (with C++ workload)
- **Vulkan SDK** (1.3.x or later) - [Download](https://vulkan.lunarg.com/sdk/home)
- **CMake** 3.22 or later - [Download](https://cmake.org/download/)
- **Ninja** build tool - [Download](https://github.com/ninja-build/ninja/releases)

### Linux (Ubuntu/Debian)
- **GCC 12+** or **Clang 20+**
- **Vulkan SDK** - Install via package manager or from [LunarG](https://vulkan.lunarg.com/sdk/home)
- **CMake** 3.22 or later
- **Ninja** build tool

## Build Instructions

### Windows (MSVC Debug)

1. **Install Dependencies**
   - Download and install Vulkan SDK from https://vulkan.lunarg.com/sdk/home
   - Ensure `VULKAN_SDK` environment variable is set (usually done by installer)
   - Install CMake and Ninja (or use vcpkg/chocolatey)

2. **Configure the Project**
   ```powershell
   cmake --preset windows-msvc-debug
   ```

3. **Build**
   ```powershell
   cmake --build build --config Debug
   ```

4. **Run**
   ```powershell
   ./bin/vulkan_test_debug.exe
   ```

### Linux (Clang)

1. **Install Dependencies**
   
   **Ubuntu/Debian:**
   ```bash
   sudo apt-get update
   sudo apt-get install -y \
       build-essential \
       cmake \
       ninja-build \
       clang-20 \
       clang++-20
   
   # Install Vulkan SDK
   # The SDK is required here because this project uses DXC and SPIRV-Reflect;
   # the Ubuntu Vulkan packages alone do not provide those components.
   wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
   sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list https://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list
   sudo apt-get update
   sudo apt-get install -y vulkan-sdk
   ```

2. **Configure the Project**
   ```bash
   cmake --preset linux
   ```

3. **Build**
   ```bash
   cmake --build build-linux --config Debug
   ```

4. **Run**
   ```bash
   ./bin/vulkan_test_debug
   ```

## Project Structure

```
basecamp_vkn/
├── src/
│   ├── common/              # Shared utilities and common code
│   ├── vulkan_test/         # Main Vulkan application
│   └── shaders/             # HLSL shader sources
├── ext/                     # External dependencies
│   ├── glm/                 # Math library
│   ├── imgui/               # UI library
│   ├── assimp/              # Model loading
│   └── ois/                 # Input handling
├── bin/                     # Output binaries
├── build/                   # Windows build directory
├── build-linux/             # Linux build directory
└── CMakeLists.txt           # Root CMake configuration
```

## Development Workflow

### Building in VS Code

The workspace includes CMake Tools integration. You can:

1. Select preset: `Windows MSVC Debug` or `Linux` (from CMake preset selector in VS Code)
2. Configure: CMake will automatically configure based on selected preset
3. Build: Use the build button or `Ctrl+Shift+B`

### Shader Development

Shaders are written in HLSL and compiled to SPIR-V:

- Shader sources: `src/shaders/`
- DXC compiler is invoked during the CMake build
- Compiled SPIR-V binaries go to `bin/shader_spirv/`

### Running with Validation

The Vulkan SDK includes validation layers. Enable them via:

```bash
# Windows
VK_INSTANCE_EXTENSIONS=VK_EXT_debug_utils ./bin/vulkan_test_debug.exe

# Linux
VK_INSTANCE_EXTENSIONS=VK_EXT_debug_utils ./bin/vulkan_test_debug
```

## Troubleshooting

### CMake can't find Vulkan SDK
- **Windows**: Ensure `VULKAN_SDK` environment variable is set. Reinstall Vulkan SDK if needed.
- **Linux**: Run `source ~/.bashrc` or restart terminal after installing Vulkan SDK.

### "ninja: command not found"
- Install Ninja: 
  - Windows: `choco install ninja` or download from GitHub
  - Linux: `sudo apt-get install ninja-build`

### Shader compilation errors
- Ensure DXC is installed with Vulkan SDK
- Check `bin/shader_dxil/` and `bin/shader_spirv/` for compilation artifacts

## Current Development Focus

See [feature-list.md](.github/feature-list.md) and [current-task.md](.github/current-task.md) for the active implementation roadmap.

## License

[Add your license information here]
