# .tool —— SmartHome 工程本地工具

本目录存放工程侧的本地辅助脚本（**不参与固件编译**，仅在 PC 上运行）。

## 文件清单

| 文件 | 作用 |
|---|---|
| `pull_sdk.py` | 主脚本：封装 `sync_lib.py`，从 my-stm32-sdk 拉取所选模块到工程 |
| `pull_sdk.bat` | Windows 入口：找到 python 后调用 `pull_sdk.py`（双击 / 命令行均可） |

## 用途

SDK 是单源真相仓库，工程里的 `MySDK/` 是 `sync_lib.py` 镜像出来的产物，
每次拉取会 **先整体清空再重建**。本脚本把那条长命令收敛成一行。

## 用法

```bash
# 方式一：Windows 直接跑 bat（推荐）
.\.tool\pull_sdk.bat

# 方式二：直接用 python（Git Bash / PowerShell / CMD 均可）
python .tool/pull_sdk.py

# 演习：只打印将要做什么，不写盘
python .tool/pull_sdk.py --dry-run

# 指定别的 toml / 覆盖 SDK 根
python .tool/pull_sdk.py --toml path/to/sdk.toml
python .tool/pull_sdk.py --sdk C:/path/to/my-stm32-sdk
```

## 前置条件

- **Python >= 3.11**（sync_lib.py 用 tomllib）。脚本优先使用本机托管运行时
  `C:/Users/ZOZE/.workbuddy/binaries/python/versions/3.13.12/python.exe`，
  找不到时回退到 PATH 上的 `py` / `python`。
- `User/sdk.toml` 必须存在，且其中的 `sdk` 字段指向真实的 SDK 根目录
  （注意路径带连字符 `my-stm32-sdk`）。

## 拉取行为 / 注意

- 输出目录 `MySDK/` 会被整体 `rmtree` 后重建：**不要在 `MySDK/` 内手改任何文件**。
- `User/board_cfg.h` 是工程硬件绑定资产，sync 只在它确实不存在时才生成模板，
  已存在则保留（不会被覆盖）。
- `protocols.wol` 依赖 lwIP（`external:lwip`），不进 `MySDK/`，由工程侧提供。
- SmartHome 的 CMake 已 EXCLUDE 掉 LwIP 自带的 `apps/mqtt/mqtt.c`，避免与
  `protocols.mqtt` 同名符号冲突——若你换工程需自行处理。
- 路径一律用 Windows 风格 `C:/...`，避免 Git Bash 把 `/c/...` 解析成 `c:\c\...`。

## 工程布局

```
SmartHome/
├── .tool/            # 本目录（本地工具，不进固件构建）
│   ├── pull_sdk.py
│   └── pull_sdk.bat
├── User/
│   ├── sdk.toml      # 选模块配置（输入）
│   ├── board_cfg.h   # 硬件绑定（资产）
│   ├── Inc/          # 工程头（app_main.h / config_network.h / task_*.h ...）
│   └── Src/
├── MySDK/            # sync 生成（可整体删，rmtree 重建）
└── CMakeLists.txt    # add_subdirectory(MySDK) + link mystm32
```
