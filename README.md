# Tensyoki Linux Port

《天翔记 HD》 (信长之野望・天道) Linux 原生移植项目。

## 状态

**Phase 1 (架构骨架) - 完成** ✅
- 核心模块：配置、日志、VFS、数学工具
- 平台层：SDL3 + OpenGL 4.5 窗口/输入/剪贴板
- 资源解析：`.bin` 容器 (BinArchive)、`.grp` 图像、`.dat` 结构化数据
- 渲染抽象：OpenGL 后端、Sprite/Text 渲染器框架
- 调试 UI：ImGui 资源浏览器、十六进制查看器

**Phase 2 (绿色构建/基础验证) - 进行中** 🔧
- 修复构建系统、API 漂移、所有权问题

## 构建要求

- CMake 3.20+
- C++20 编译器 (GCC 11+, Clang 13+, MSVC 19.30+)
- SDL3 开发库
- spdlog, fmt, glm (通过 CMake FetchContent 自动获取)
- DirectX Shader Compiler (DXC) - 用于 HLSL→SPIR-V
- OpenGL 4.5+ / Vulkan 1.2+ 驱动

### Ubuntu/Debian
```bash
sudo apt install cmake g++ libsdl3-dev libvulkan-dev
```

### Arch Linux
```bash
sudo pacman -S cmake gcc sdl3 vulkan-headers vulkan-icd-loader
```

## 构建

```bash
# 配置
cmake -S . -B build -G Ninja

# 编译
cmake --build build

# 运行测试
ctest --test-dir build --output-on-failure
```

## 运行

```bash
# 指定游戏数据目录
./build/tensyoki /path/to/NOBUNAGA'S AMBITION Tenshouki with Power Up Kit HD version

# 或设置环境变量
export TENSYOKI_DATA_ROOT=/path/to/game/data
./build/tensyoki
```

## 项目结构

```
src/
├── app/              # 应用入口 (未来)
├── core/             # 核心工具
│   ├── config.hpp/cpp      # TOML 配置管理
│   ├── filesystem.hpp/cpp  # VFS 虚拟文件系统
│   ├── logging.hpp/cpp     # spdlog 日志封装
│   └── math_utils.hpp/cpp  # 数学工具
├── platform/         # 平台抽象
│   ├── platform.hpp        # 接口定义
│   └── platform_linux.cpp  # SDL3 Linux 实现
├── render/           # 渲染抽象
│   ├── renderer.hpp/cpp    # 渲染器核心 (OpenGL/Vulkan)
│   └── shader_compiler.hpp/cpp  # DXC HLSL→SPIR-V
├── resource/         # 资源解析
│   ├── bin_parser.hpp/cpp  # .bin 容器 (LINK 魔数)
│   ├── grp_parser.hpp/cpp  # 图像解码
│   └── dat_parser.hpp/cpp  # 结构化数组 (武将/城池)
├── ui/               # 调试 UI
│   └── imgui_backend.hpp/cpp  # ImGui SDL3+OpenGL 后端
└── test/             # 单元测试
```

## 调试 UI

- **F1** - 显示/隐藏资源浏览器
- **Esc** - 退出程序
- **F11** - 切换全屏

资源浏览器功能：
- 选择 `.bin` 容器 (res_shader.bin, res_else.bin, res_grp.bin 等)
- 过滤、树状列表显示条目
- 查看元数据 (偏移、大小、类型、哈希)
- 十六进制转储 (前 256 字节/全部)
- 提取单个/全部资源到 `extracted/` 目录

## 许可证

MIT License - 详见 [LICENSE](LICENSE)

## 致谢

- [SDL3](https://github.com/libsdl-org/SDL) - 跨平台窗口/输入
- [Dear ImGui](https://github.com/ocornut/imgui) - 即时模式 GUI
- [spdlog](https://github.com/gabime/spdlog) - 高性能日志
- [fmt](https://github.com/fmtlib/fmt) - 格式化库
- [GLM](https://github.com/g-truc/glm) - 数学库
- [DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler) - HLSL 编译器

---

*本项目仅供学习和研究使用，不包含任何游戏原版资源文件。*