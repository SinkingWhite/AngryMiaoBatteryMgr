# AngryMiaoBatteryMgr

**中文** | [English](docs/README.en.md)

为 **AngryMiao INFINITY MOUSE .100** 制作的非官方 Windows 原生托盘工具。
> **目前仅在 AngryMiao INFINITY MOUSE .100 上进行过实机测试。** 其他型号和固件尚未验证，请勿将协议相似视为兼容保证。当前应用界面为中文；英文界面仍在计划中。

## 功能

- 猫爪托盘图标：鼠标电量 **0–30% 红色 / 31–50% 橙色 / 51–100% 绿色**，未知显示灰色问号。
- 悬停提示 `⚡️97%/89%·800`：依次为鼠标电量、备用电量、当前 DPI；未读到的值显示 `?`。
- 点击图标打开托盘附近的浅灰小浮窗，点击外部收起。
- 显示当前 X/Y DPI，切换设备已有档位，并读取确认；保留其他档位参数和颜色。
- 浮窗内调整刷新间隔、悬停提示、启动隐藏及开机启动，自动保存。
- 后台串行读取，失败退避与重连恢复，不需要官方网页保持打开。

## 使用

目标系统：**Windows 10/11 x64**。已编译版本如已发布，可从 [Releases](https://github.com/SinkingWhite/AngryMiaoBatteryMgr/releases) 获取；若没有附件，请按下方步骤自行构建。

1. 将 `Miao.exe` 放到固定目录，双击运行，无需安装。
2. 在通知区域寻找猫爪；Windows 可能将它收起到溢出菜单。
3. 点击查看电量或修改偏好，右键可刷新或退出。

开机启动默认关闭，需要主动启用。若之前已明确启用同名工具的启动项，新版会保留选择并更新路径。开启后请不要直接移动 EXE；移动后运行一次程序以更新路径。

## 构建与测试

安装 Visual Studio 2022 Build Tools 的 **Desktop development with C++** 工作负载和 Windows SDK，在仓库根目录运行：

```powershell
./scripts/build.ps1
```

输出 `artifacts/native/Miao.exe` 和对应源码包。使用 `/MT /O2 /W4 /WX`，静态链接 C++ 运行库；脚本自动执行 21 项无硬件协议检查。可复现的构建步骤不依赖本机绝对路径。

手动实机检查前请先退出托盘工具和官方网页驱动，避免并发访问接收器：

```powershell
./artifacts/native/native-tests.exe --hardware     # 读取双电量及 DPI
./artifacts/native/native-tests.exe --switch-test  # 实际切换下一档并恢复原档
./artifacts/native/Miao.exe --smoke-test            # 启动、读取、退出；不修改 DPI
```

已经验证：原生构建、21 项协议检查、实机电量/DPI 读取、800 → 1200 → 800 切换恢复，以及浮窗启动与退出。休眠、长期运行、多屏混合缩放和不同固件还需更多测试。GitHub Actions 仅构建及运行无硬件检查。

## 配置与已知问题

偏好：`%LOCALAPPDATA%/AngryMiaoBattery/native.ini`；错误日志：同目录 `native.log`（1 MiB 轮转）。默认刷新 30 秒，可设置 5–3600 秒，失败退避最多 300 秒。程序无账号、云服务或遥测；分享日志前请检查内容。

- **备用电池曾出现同一块电池未交换却读数突降、重新插入恢复的反馈，根因尚未解决。** 当前显示接收器上报值，不保证备用百分比始终准确。
- 避免与官方网页同时修改配置，两个客户端可能竞争接收器通道。
- 设备断连或读取失败显示未知，不将失败显示为 0%；驱动阻塞同步 HID 调用时，退出可能延迟。
- 支持的是通知区域图标，不是在固定任务栏图标上显示自定义文字。

## 项目结构

```text
native/       原生程序、协议、HID 访问和测试
scripts/      构建与源码打包
docs/        英文 README、协议说明、测试与贡献文档
LICENSES/     第三方许可声明
.github/      Windows CI
```

[贡献指南](docs/CONTRIBUTING.md) · [更新记录](docs/CHANGELOG.md) · [协议说明](docs/PORTING_ANALYSIS.md) · [原生验证](docs/NATIVE_PORT.md)

## 许可与致谢

[GPL-3.0-only](LICENSE)，保留 [NOTICE](NOTICE.md) 与 LICENSES 声明。协议移植参考 [LukePeltier/miaobat](https://github.com/LukePeltier/miaobat)、[HaloBattery](https://github.com/HeyOkay/HaloBattery) 和 [AJAZZ Control Center](https://github.com/Aiacos/ajazz-control-center)；DPI 通信依据 [官方 AM30 网页驱动](https://ammaster.angrymiao.com/mouse/) 核对，网页脚本不随项目分发。

本项目与 Angry Miao 无隶属关系，不是官方驱动。分发二进制时请同时提供对应源码和构建脚本。

## TODO

- [ ] **EN Support**：英文应用界面。
- [ ] 更多功能支持：逐步支持怒喵官方驱动中的更多功能。
- [ ] 更多设备和固件的兼容性验证。

以上为计划方向，不代表已实现或承诺完成时间。
