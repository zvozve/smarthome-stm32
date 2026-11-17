# tools/ —— 工程自带 host 工具（由 sdk-pull.py 生成，请勿手工修改）

本目录是从 mysdk-stm32 按 `User/sdk.toml` 的 `[tools]` 拉下来的工具子集，
统一经工程根 `sdk_run.py` 调用（`.vscode/tasks.json` 只认 `sdk_run.py <子命令>`）。

**这批文件随工程提交进版本库**：拿到工程的人即使没有 SDK 检出，也能直接烧录 / 发 OTA /
做行尾体检。`sdk_run.py` 的查找顺序是「本目录 → 工程根 tools/ → SDK/tools」，
所以本地副本优先级最高，行为由工程锁定。

| 工具 | 子命令 | 作用 |
|---|---|---|
| `check-eol.py` | `sdk_run.py audit` | 行尾体检：CR 加倍 / 孤立 CR / 混用行尾 / git 索引侧 -text。支持位置参数指定根目录。 |
| `format-gbk2utf8.py` | `sdk_run.py trans` | GBK/GB2312 源码批量转 UTF-8。 |
| `format-utf16be2utf8.py` | `sdk_run.py trans16` | UTF-16 LE/BE 与 UTF-8-BOM 归一化为 UTF-8 无 BOM。 |
| `fw-flash.py` | `sdk_run.py flash` | STM32 J-Link 烧录：自检 J-Link 安装目录与 .ioc 器件名，从 .ld 解析槽地址，回写 settings.json 供 cortex-debug 用。零 SDK 路径依赖，可单文件带走。 |
| `fw-ota-pack.py` | `sdk_run.py pack` | bin → .otapkg 打包（80 字节头，与 services/ota_core 的 ota_image.h 对应）。raw-bin 流程下可选。 |
| `fw-ota-ymodem.py` | `sdk_run.py ymodem` | OTA 主机端：经 YMODEM 把 raw .bin 发给设备（设备侧自动选非运行槽）。协议常量需与 library/protocols/ymodem 一致。 |
| `gen-cmake-paths.py` | `sdk_run.py gencmake` | 扫描工程目录生成 CMake 源/包含路径片段（面向手工维护 User/、未走 sdk-pull 接线的工程）。 |
| `sdk-check-oop.py` | `sdk_run.py audit` | HAL 泄漏红线检查。支持 --root 与布局自适应（library/<layer> 或镜像的 <layer> 直挂），无 SDK 时也能审工程 MySDK/。 |

SDK 版本：0.13.1　拉取时间：2026-11-17 16:22:31
