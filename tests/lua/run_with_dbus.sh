#!/bin/bash
# 启动临时 D-Bus session 并运行 Lua 测试

set -e

# 使用 dbus-run-session 或 dbus-launch 启动临时 D-Bus daemon
if command -v dbus-run-session &> /dev/null; then
    # dbus-run-session 是更现代的方式
    exec dbus-run-session -- "$@"
elif command -v dbus-launch &> /dev/null; then
    # 使用 dbus-launch 启动并获取环境变量
    eval $(dbus-launch --sh-syntax)
    trap "kill $DBUS_SESSION_BUS_PID" EXIT
    exec "$@"
else
    echo "ERROR: Neither dbus-run-session nor dbus-launch found" >&2
    echo "Please install dbus-tools or ensure D-Bus is available" >&2
    exit 1
fi
