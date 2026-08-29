#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pull_sdk.py —— 从 my-stm32-sdk 重新拉取所选模块到工程。

本脚本是对 SDK 仓库 tools/sync_lib.py 的薄封装：
  - 自动定位工程根（本文件父目录的父目录）
  - 默认读取 <工程根>/User/sdk.toml
  - 从 toml 的 [sdk] 字段拿到 SDK 根，定位 tools/sync_lib.py
  - 选一个可用的 python（优先托管运行时 3.13.12，回退 py / python）
  - 转发其余参数（如 --dry-run）

用法:
    python pull_sdk.py [--toml <path>] [--sdk <sdk_root>] [--dry-run]

依赖:
    python >= 3.11（sync_lib.py 用 tomllib）
"""
import argparse
import os
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)  # .sdktool 的父目录 = 工程根

# 本机托管运行时（环境相关，找不到时回退到 PATH 上的 py / python）
MANAGED_PYTHON = r"C:/Users/ZOZE/.workbuddy/binaries/python/versions/3.13.12/python.exe"


def find_python():
    if os.path.isfile(MANAGED_PYTHON):
        return MANAGED_PYTHON
    for name in ("py", "python3", "python"):
        p = shutil.which(name)
        if p:
            return p
    return None


def main():
    ap = argparse.ArgumentParser(description="Pull my-stm32-sdk into project")
    ap.add_argument("--toml", default=os.path.join(PROJECT_ROOT, "User", "sdk.toml"),
                    help="sdk.toml 路径（默认 <工程根>/User/sdk.toml）")
    ap.add_argument("--sdk", default=None,
                    help="覆盖 SDK 根目录（否则从 toml 的 [sdk] 读取）")
    ap.add_argument("--dry-run", action="store_true",
                    help="只打印将要执行的操作，不写盘")
    args = ap.parse_args()

    if not os.path.isfile(args.toml):
        print(f"[ERR] sdk.toml 未找到: {args.toml}", file=sys.stderr)
        sys.exit(1)

    # 读取 toml 拿到 SDK 根（需要 python >= 3.11 的 tomllib）
    try:
        import tomllib
    except ModuleNotFoundError:
        print("[ERR] 需要 python >= 3.11（缺少 tomllib）", file=sys.stderr)
        sys.exit(1)

    with open(args.toml, "rb") as f:
        cfg = tomllib.load(f)
    sdk_root = os.path.abspath(args.sdk or cfg.get("sdk", ""))
    if not sdk_root or not os.path.isdir(sdk_root):
        print(f"[ERR] SDK 根目录无效: {sdk_root}", file=sys.stderr)
        sys.exit(1)

    sync_lib = os.path.join(sdk_root, "tools", "sync_lib.py")
    if not os.path.isfile(sync_lib):
        print(f"[ERR] sync_lib.py 未找到: {sync_lib}", file=sys.stderr)
        sys.exit(1)

    py = find_python()
    if py is None:
        print("[ERR] 未找到 python >= 3.11", file=sys.stderr)
        sys.exit(1)

    cmd = [py, sync_lib, os.path.abspath(args.toml)]
    if args.sdk:
        cmd += ["--sdk", sdk_root]
    if args.dry_run:
        cmd.append("--dry-run")

    print(">>", " ".join(cmd))
    sys.exit(subprocess.call(cmd))


if __name__ == "__main__":
    main()
