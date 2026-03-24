# CopilotClaw

[English](README.md)

CopilotClaw 是一个面向 Windows 11 x86_64 的极简后台程序，用来一键切换 OpenClaw 网关的开启与关闭。

开源协议：[MIT](LICENSE)
商标与图标说明：[NOTICE.zh-CN.md](NOTICE.zh-CN.md)

它的设计目标就是配合 Microsoft PowerToys 的按键映射功能使用：
- 把 Windows 上的 Copilot 键映射为启动这个程序
- 按一下就切换 OpenClaw 的开关状态
- 程序本身不显示主窗口、不显示控制台、不显示托盘图标，执行完成后立即退出
- 程序不会自动打开浏览器，也不会弹出额外界面
- 这是 `Stud-MuYi` 的非官方独立项目

## 行为说明

每次启动时，程序会在后台：
- 静默执行 `openclaw gateway status --no-color`
- 探测 `127.0.0.1:18789`
- 如果判断 OpenClaw 正在运行，则静默执行 `openclaw gateway stop`
- 否则静默执行 `openclaw gateway start`
- 状态切换完成或超时后立即退出

为了避免重复触发造成并发切换，同一时刻只允许一个实例运行。如果在前一次执行尚未结束时再次触发，后一次启动会立刻退出。

## 运行要求

- Windows 11 x86_64
- 安装了带 MSVC 和 CMake 3.25 及以上版本的 Visual Studio 2022
- 已正确安装 OpenClaw，并且 PowerShell 中可以直接使用 `openclaw`
- 在 MSVC 下，项目会使用 `/std:c++latest`，因为当前云端的 CMake/MSVC 组合还不能直接选择 `CXX26` 方言。

建议先在 PowerShell 中确认以下命令可用：

```powershell
openclaw gateway status --no-color
openclaw gateway start
openclaw gateway stop
```

## 构建方法

在 Windows PowerShell 中执行：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

生成的可执行文件路径：

```text
build\Release\CopilotClaw.exe
```

可执行文件会嵌入项目图标，相关文件是：
- `assets/CopilotClaw.svg`
- `assets/CopilotClaw.ico`
- `app.rc`

注意：
- 只有仓库中的原创代码部分按 MIT 开源。
- 当前图标文件基于 Microsoft Copilot 图标，不属于 MIT 授权范围。
- 仓库中所有涉及 Microsoft 的内容，仅用于学习、兼容性测试或本地参考。
- 如果你要公开分发二进制或公开发布该项目，请先阅读 [NOTICE.zh-CN.md](NOTICE.zh-CN.md)，必要时再对照 [NOTICE.md](NOTICE.md)，并优先替换这些涉及 Microsoft 的文件。
- 如果你希望进一步降低法律风险，也应考虑在公开发布前修改应用名和仓库名。

## 配合 PowerToys 使用

1. 安装并打开 Microsoft PowerToys。
2. 进入 `Keyboard Manager`。
3. 对 Copilot 键做重新映射。
4. 将目标动作设置为启动 `CopilotClaw.exe`。
5. 保存映射。

之后按下映射后的按键，就会在后台静默切换 OpenClaw。

## GitHub Actions

工作流文件是 `.github/workflows/build-windows.yml`。

它会在以下情况下运行：
- 你向 `main` 分支推送代码
- 你发起 Pull Request
- 你在 GitHub 的 `Actions` 页面手动触发

工作流完成后：
- 打开对应的工作流运行记录
- 下载 `CopilotClaw-windows-x64` artifact
- 解压后取得 `CopilotClaw.exe`

## GitHub

如果你将项目发布到自己的账号下，仓库地址通常会是：

```text
https://github.com/Stud-MuYi/CopilotClaw
```

## 项目结构

- `main.cpp`：无界面的静默切换逻辑
- `CMakeLists.txt`：面向 Windows 11 x86_64 的最新可用编译模式构建配置
- `assets/CopilotClaw.svg`：基于 Microsoft Copilot 图标的 SVG 文件
- `assets/CopilotClaw.ico`：由 SVG 生成并嵌入到程序中的 Windows 图标
- `app.rc`：Windows 图标资源脚本
- `.github/workflows/build-windows.yml`：GitHub Actions Windows 构建工作流
- `NOTICE.md`：英文商标与图标分发说明
- `NOTICE.zh-CN.md`：中文商标与图标分发说明
