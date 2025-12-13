#!/bin/bash
#
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# openUBMC is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
#
# 循环运行测试脚本（使用 GDB 调试模式）
#
# 功能：
#   - 循环运行测试，自动重建项目
#   - 使用 GDB 运行测试，便于调试
#   - 当测试超时或阻塞时，自动记录所有线程的 backtrace
#   - 保存测试日志和 backtrace 到 test_failure_logs 目录
#
# 使用方法：
#   ./scripts/run_tests_loop.sh [选项]
#
# 选项：
#   -n, --no-build      跳过构建，直接运行测试（需要 builddir 已存在）
#   -t, --timeout SEC   设置测试超时时间（秒，默认: 300）
#   -i, --iterations N  设置循环次数（默认: 100）
#   -f, --filter FILTER 设置 gtest 过滤器（例如: "-ModuleManagerTest.*:-mMicroComponentTest*"）
#   -h, --help          显示帮助信息
#
# 环境变量：
#   MAX_ITERATIONS: 最大循环次数（默认: 100）
#   TEST_TIMEOUT: 测试超时时间，单位秒（默认: 300）
#   GTEST_FILTER: 传递给 gtest 的过滤器（默认: 空，不过滤）
#
# 示例：
#   # 完整构建并测试
#   ./scripts/run_tests_loop.sh
#
#   # 跳过构建，直接测试（需要先构建）
#   ./scripts/run_tests_loop.sh --no-build
#
#   # 自定义超时和循环次数
#   ./scripts/run_tests_loop.sh --timeout 600 --iterations 10
#

set -uo pipefail

# 脚本配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/builddir"
LOG_DIR="${PROJECT_ROOT}/test_failure_logs"
MAX_ITERATIONS="${MAX_ITERATIONS:-100}"

# 测试超时时间（秒），可通过环境变量覆盖
# 例如: TEST_TIMEOUT=600 ./scripts/run_tests_loop.sh
TEST_TIMEOUT="${TEST_TIMEOUT:-300}"

# gtest 过滤器，默认不过滤（可通过命令行参数或环境变量设置）
GTEST_FILTER="${GTEST_FILTER:-}"

# 颜色输出（在使用前定义，避免 set -u 报未绑定变量）
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 解析命令行参数
SKIP_BUILD=false
SHOW_HELP=false
STOP_REQUESTED=false
test_child_pid=""

handle_sigint() {
    echo ""
    echo "收到中断信号，准备退出..."
    STOP_REQUESTED=true
    if [ -n "${test_child_pid}" ] && kill -0 "${test_child_pid}" 2>/dev/null; then
        kill -INT "${test_child_pid}" 2>/dev/null || true
    fi
}

trap handle_sigint INT TERM

while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--no-build)
            SKIP_BUILD=true
            shift
            ;;
        -n=true|--no-build=true)
            SKIP_BUILD=true
            shift
            ;;
        -n=false|--no-build=false)
            SKIP_BUILD=false
            shift
            ;;
        -t|--timeout)
            TEST_TIMEOUT="$2"
            shift 2
            ;;
        -i|--iterations)
            MAX_ITERATIONS="$2"
            shift 2
            ;;
        -f|--filter)
            GTEST_FILTER="$2"
            shift 2
            ;;
        -h|--help)
            SHOW_HELP=true
            shift
            ;;
        *)
            echo -e "${RED}错误: 未知参数: $1${NC}"
            echo "使用 --help 查看帮助信息"
            exit 1
            ;;
    esac
done

# 显示帮助信息
if [ "${SHOW_HELP}" = true ]; then
    cat << EOF
循环运行测试脚本（使用 GDB 调试模式）

用法:
    $0 [选项]

选项:
    -n, --no-build            跳过构建，直接运行测试（需要 builddir 已存在）
    -n=false                  显式关闭跳过构建（与默认相同）
    -t, --timeout SEC         设置测试超时时间（秒，默认: 300）
    -i, --iterations N        设置循环次数（默认: 100）
    -f, --filter FILTER       设置 gtest 过滤器（例如: "-ModuleManagerTest.*:-mMicroComponentTest*"）
    -h, --help                显示此帮助信息

环境变量:
    MAX_ITERATIONS            最大循环次数（默认: 100）
    TEST_TIMEOUT              测试超时时间，单位秒（默认: 300）
    GTEST_FILTER              gtest 过滤器（默认: 空，不过滤）

示例:
    # 完整构建并测试
    $0

    # 跳过构建，直接测试（需要先构建）
    $0 --no-build

    # 自定义超时和循环次数
    $0 --timeout 600 --iterations 10

    # 组合使用
    $0 --no-build --timeout 600 --iterations 5
EOF
    exit 0
fi

# 创建日志目录
mkdir -p "${LOG_DIR}"

# 统计信息
success_count=0
failure_count=0

echo "=========================================="
echo "开始循环测试：共 ${MAX_ITERATIONS} 次"
if [ "${SKIP_BUILD}" = true ]; then
    echo "模式: 跳过构建（直接运行测试）"
else
    echo "模式: 完整构建并测试"
fi
echo "超时时间: ${TEST_TIMEOUT} 秒"
echo "=========================================="
echo ""

for iteration in $(seq 1 ${MAX_ITERATIONS}); do
    if [ "${STOP_REQUESTED}" = true ]; then
        echo "检测到中断请求，停止循环。"
        break
    fi
    echo "[${iteration}/${MAX_ITERATIONS}] 开始第 ${iteration} 次测试..."

    # 根据参数决定是否构建
    if [ "${SKIP_BUILD}" = false ]; then
        # 清理并重新构建
        echo "  清理 builddir..."
        rm -rf "${BUILD_DIR}"
        
        echo "  运行 meson setup..."
        meson setup "${BUILD_DIR}" || {
            echo -e "${RED}  [失败] meson setup 失败${NC}"
            failure_count=$((failure_count + 1))
            continue
        }
        
        echo "  运行 meson compile..."
        time meson compile -C "${BUILD_DIR}" || {
            echo -e "${RED}  [失败] meson compile 失败${NC}"
            failure_count=$((failure_count + 1))
            continue
        }
    else
        # 跳过构建，检查 builddir 是否存在
        if [ ! -d "${BUILD_DIR}" ]; then
            echo -e "${RED}  [失败] builddir 不存在，无法跳过构建${NC}"
            echo -e "${YELLOW}  提示: 请先运行完整构建，或移除 --no-build 参数${NC}"
            failure_count=$((failure_count + 1))
            continue
        fi
        echo "  跳过构建，使用现有的 builddir"
    fi

    # 运行测试（使用 GDB 以便在阻塞或失败时记录 backtrace）
    echo "  运行测试（使用 GDB 调试模式）..."
    test_log="${LOG_DIR}/test_${iteration}.log"
    gdb_log="${LOG_DIR}/test_${iteration}_gdb.log"
    bt_log="${LOG_DIR}/test_${iteration}_backtrace.log"
    full_log="${LOG_DIR}/test_${iteration}_full.log"
    test_exit_code=0
    test_failed=false
    test_timeout=false
    
    # 测试可执行文件路径
    TEST_EXE="${BUILD_DIR}/tests/libmcpp_test"
    
    if [ ! -f "${TEST_EXE}" ]; then
        echo -e "${RED}  [失败] 测试可执行文件不存在: ${TEST_EXE}${NC}"
        failure_count=$((failure_count + 1))
        continue
    fi
    
    # 创建 GDB 命令脚本
    # 使用 continue 让程序运行直到退出，并捕获所有信号
    gdb_script=$(mktemp)
    cat > "${gdb_script}" << GDBEOF
set confirm off
set pagination off
set print elements 0
set print null-stop on
set print frame-arguments all
# 捕获所有可能导致崩溃的信号，并在信号处停止并打印堆栈
handle SIGINT stop print
handle SIGABRT stop print
handle SIGSEGV stop print
handle SIGTERM stop print
handle SIGBUS stop print
handle SIGFPE stop print
handle SIGILL stop print
# 为每个信号定义命令，在信号处自动打印堆栈
# 注意：在 batch 模式下，commands 可能无法正确执行，改用更直接的方法
# 使用 Python 脚本在信号处自动打印堆栈
python
import gdb

class SignalBacktrace(gdb.Command):
    def __init__(self):
        super(SignalBacktrace, self).__init__("signal_bt", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        print("\n==========================================")
        print("Backtrace on signal:")
        print("==========================================")
        try:
            gdb.execute("thread apply all bt full")
        except:
            pass
        print("==========================================")

bt_cmd = SignalBacktrace()

def stop_handler(event):
    if isinstance(event, gdb.StopEvent):
        try:
            bt_cmd.invoke("", False)
        except:
            pass

gdb.events.stop.connect(stop_handler)
end
# 运行测试
run --gtest_color=yes${GTEST_FILTER:+ --gtest_filter=${GTEST_FILTER}}
# 如果程序收到信号，GDB 会停止，hook 会打印堆栈
# 如果程序正常退出，continue 会立即返回
continue
# 如果程序还在运行（不应该发生），退出
quit
GDBEOF
    
    # 使用 timeout 命令运行 GDB，超时后记录 backtrace
    {
        echo "=========================================="
        echo "使用 GDB 运行测试"
        echo "测试可执行文件: ${TEST_EXE}"
        echo "超时时间: ${TEST_TIMEOUT} 秒"
        echo "=========================================="
        echo ""
        
        # 使用 timeout 运行 GDB，超时后会发送 SIGTERM
        test_child_pid=""
        timeout "${TEST_TIMEOUT}" gdb -batch -x "${gdb_script}" "${TEST_EXE}" > "${gdb_log}" 2>&1 &
        test_child_pid=$!
        wait "${test_child_pid}"
        test_exit_code=$?
        test_child_pid=""
        
        # 检查是否超时（timeout 命令在超时时返回 124）
        if [ ${test_exit_code} -eq 124 ]; then
            test_timeout=true
            test_failed=true
            echo ""
            echo "=========================================="
            echo "测试超时（${TEST_TIMEOUT} 秒），记录 backtrace"
            echo "=========================================="
        else
            # 首先检查 GDB 日志中是否有信号或崩溃信息
            if grep -qE "Program received signal|Thread.*received signal|Backtrace on" "${gdb_log}" 2>/dev/null; then
                # 有崩溃信号，提取 backtrace 信息
                test_failed=true
                echo ""
                echo "=========================================="
                echo "检测到程序崩溃信号，提取 backtrace"
                echo "=========================================="
                
                # 检查是否有实际的堆栈信息
                if grep -qE "^#0 |^#1 |^#2 |^Thread [0-9]+.*at " "${gdb_log}" 2>/dev/null; then
                    # 从 GDB 日志中提取 backtrace（从信号行或 Backtrace 标记开始）
                    {
                        echo "=========================================="
                        echo "程序崩溃 backtrace（从 GDB 日志提取）"
                        echo "=========================================="
                        echo ""
                        # 查找信号行或 Backtrace 标记
                        signal_line=$(grep -n "Program received signal\|Thread.*received signal\|Backtrace on" "${gdb_log}" | head -1 | cut -d: -f1)
                        if [ -n "${signal_line}" ]; then
                            # 从信号行开始提取，保留所有堆栈帧信息
                            tail -n +${signal_line} "${gdb_log}" | head -3000 | \
                                awk '
                                BEGIN { in_stack=0; found_signal=0; stack_count=0; in_backtrace_section=0 }
                                /Program received signal|Thread.*received signal|Backtrace on/ { 
                                    found_signal=1
                                    in_stack=1
                                    if (/Backtrace on/) {
                                        in_backtrace_section=1
                                    }
                                    print
                                    next
                                }
                                found_signal && in_backtrace_section && /^==========================================/ {
                                    print
                                    next
                                }
                                found_signal && /^#/ { 
                                    in_stack=1
                                    stack_count++
                                    print
                                    next
                                }
                                found_signal && in_stack && (/^  / || /^0x[0-9a-fA-F]+/ || /^in / || /^at / || /^from / || /^$/) { 
                                    print
                                    next
                                }
                                found_signal && in_stack && /^Thread [0-9]/ && (stack_count > 0 || in_backtrace_section) {
                                    # 新的线程开始，但我们已经有了堆栈信息，继续
                                    print
                                    next
                                }
                                found_signal && in_stack && /^\[/ && !/^\[Thread/ && stack_count == 0 && !in_backtrace_section { 
                                    # 遇到新的 GDB 消息但没有堆栈信息，停止提取
                                    in_stack=0
                                    exit
                                }
                                found_signal && in_stack && /^\(gdb\)/ && stack_count == 0 && !in_backtrace_section { 
                                    # 遇到 GDB 提示符但没有堆栈信息，停止提取
                                    in_stack=0
                                    exit
                                }
                                found_signal && in_stack && /Error in sourced command file/ && stack_count == 0 {
                                    # 遇到错误信息，停止提取
                                    in_stack=0
                                    exit
                                }
                                ' | \
                                grep -v "^\[New\|^\[Thread\|^Reading\|^Loaded\|^Breakpoint\|^Starting\|^Inferior\|^Detaching\|^A debugging" | \
                                grep -v "^Reading symbols\|^\(gdb\)\|^GNU gdb\|^Type\|^For help\|^This GDB" | \
                                grep -v "^Using host\|^Thread debugging" | \
                                head -1500
                        fi
                        echo ""
                        echo "=========================================="
                        echo "相关错误信息"
                        echo "=========================================="
                        grep -E "SIGABRT|SIGSEGV|SIGTERM|Fatal|assertion failed|Segmentation fault" "${gdb_log}" | tail -10
                    } > "${bt_log}" 2>&1 || true
                    
                    # 如果 gdb 日志已经包含完整堆栈，则不再保留单独的 bt 日志，避免重复
                    if grep -qE "^#0 |^#1 |^#2 " "${gdb_log}" 2>/dev/null; then
                        rm -f "${bt_log}"
                    fi
                else
                    # 没有找到堆栈信息，保存信号信息和错误信息
                    {
                        echo "=========================================="
                        echo "程序崩溃信息（未找到完整堆栈）"
                        echo "=========================================="
                        echo ""
                        echo "检测到的信号:"
                        grep -E "Program received signal|Thread.*received signal|Backtrace on|SIGSEGV|SIGABRT|SIGTERM|Segmentation fault" "${gdb_log}" | head -10
                        echo ""
                        echo "GDB 错误信息:"
                        grep -E "Error|error|failed|Failed|The program is not being run" "${gdb_log}" | head -10
                        echo ""
                        echo "GDB 日志末尾信息:"
                        tail -100 "${gdb_log}"
                    } > "${bt_log}" 2>&1 || true
                    echo "警告: 未找到完整的堆栈信息，已保存信号和错误信息"
                fi
            fi
            
            # 从 GDB 日志中提取测试输出（过滤掉 GDB 调试信息和 backtrace）
            # 只保留测试用例的输出，不包含堆栈跟踪信息
            grep -v "^\[New\|^\[Thread\|^Reading\|^Loaded\|^Breakpoint\|^Starting\|^Inferior\|^Detaching\|^A debugging\|^warning:" "${gdb_log}" | \
                grep -v "^Reading symbols\|^\(gdb\)\|^GNU gdb" | \
                grep -v "^Type\|^For help\|^This GDB" | \
                grep -v "^Program received signal\|^Thread.*received signal" | \
                grep -v "^#0 \|^#1 \|^#2 \|^#3 \|^#4 \|^#5 \|^#6 \|^#7 \|^#8 \|^#9 \|^#1[0-9] " | \
                grep -v "^in \|^at \|^from \|^Using host\|^Thread debugging" | \
                grep -v "^Inferior.*exited" > "${test_log}" || true
            
            # 检查测试是否失败（通过检查日志中是否有失败信息）
            if [ "${test_failed}" != true ]; then
                if grep -qE "FAILED|TIMEOUT|killed by signal|SIGABRT|SIGSEGV|Fatal|assertion failed" "${test_log}" "${gdb_log}" 2>/dev/null; then
                    test_failed=true
                elif [ ${test_exit_code} -ne 0 ]; then
                    # GDB 退出码非 0，可能是测试失败
                    test_failed=true
                else
                    # 检查测试日志是否完整（是否有测试总结信息）
                    # 正常的测试应该以 "[==========] ... tests from ... test suites ran." 结束
                    if ! grep -qE "\[==========\].*tests from.*test suites ran|\[.*PASSED.*\]|\[.*FAILED.*\]" "${test_log}" "${gdb_log}" 2>/dev/null; then
                        # 没有找到测试总结，可能是程序在退出阶段崩溃
                        # 检查是否有测试用例运行
                        if grep -qE "\[.*RUN.*\]|\[.*OK.*\]" "${test_log}" "${gdb_log}" 2>/dev/null; then
                            # 有测试用例运行，但没有总结，可能是退出阶段崩溃
                            test_failed=true
                            echo ""
                            echo "=========================================="
                            echo "检测到可能的退出阶段崩溃"
                            echo "测试用例已运行，但程序在退出时异常终止"
                            echo "=========================================="
                        fi
                    fi
                fi
            fi
        fi
        
        # 如果此前被标记失败，但没有 FAILED/信号/超时且退出码为 0，则视为成功（避免纯 SKIP 情况残留日志）
        if [ "${test_failed}" = true ] && [ "${test_timeout}" = false ]; then
            has_failed_marker=false
            has_failed_summary=false
            if grep -q "\[  FAILED  \]" "${test_log}" "${gdb_log}" 2>/dev/null; then
                has_failed_marker=true
            fi
            if grep -q "Fail:[[:space:]]*[1-9]" "${test_full_log:-/dev/null}" "${test_log}" "${gdb_log}" 2>/dev/null; then
                has_failed_summary=true
            fi
            if [ "${has_failed_marker}" = false ] && [ "${has_failed_summary}" = false ]; then
                test_failed=false
            fi
        fi

        # 如果测试失败且还没有提取 backtrace，尝试获取 backtrace
        if [ "${test_failed}" = true ] && [ ! -s "${bt_log}" ]; then
            echo ""
            echo "=========================================="
            if [ "${test_timeout}" = true ]; then
                echo "测试超时，尝试获取 backtrace"
            else
                echo "测试失败，尝试获取 backtrace"
            fi
            echo "=========================================="
            
            # 查找测试进程的 PID
            test_pid=$(pgrep -f "${TEST_EXE}" | head -1)
            
            if [ -n "${test_pid}" ]; then
                echo "找到测试进程 PID: ${test_pid}"
                
                # 创建 GDB 命令来获取 backtrace
                bt_script=$(mktemp)
                cat > "${bt_script}" << BTEOF
set confirm off
set pagination off
set print elements 0
set print null-stop on
set print pretty on
set print frame-arguments all
attach ${test_pid}
thread apply all bt full
info threads
info registers
detach
quit
BTEOF
                
                # 附加到进程并获取 backtrace
                echo "附加到进程并记录所有线程的 backtrace..."
                # 等待一小段时间，确保进程状态稳定
                sleep 0.5
                if gdb -batch -x "${bt_script}" "${TEST_EXE}" > "${bt_log}.tmp" 2>&1; then
                    # 检查是否有实际的 backtrace 信息（包含 #0, #1 等堆栈帧）
                    if grep -qE "^#0 |^#1 |^#2 |^Thread [0-9]|^bt |backtrace" "${bt_log}.tmp" 2>/dev/null; then
                        # 提取 backtrace 信息，保留所有堆栈帧和线程信息
                        {
                            echo "=========================================="
                            echo "程序 backtrace（所有线程）"
                            echo "=========================================="
                            echo ""
                            # 提取从 "Thread" 或 "#0" 开始的所有行，直到遇到明显的分隔符
                            # 保留所有包含堆栈帧的行（#0, #1, #2 等）和相关信息
                            awk '
                            /^Thread [0-9]/ { in_thread=1; print; next }
                            /^#/ { in_thread=1; print; next }
                            in_thread && (/^  / || /^0x/ || /^in / || /^at / || /^from / || /^$/) { print; next }
                            in_thread && /^[^#]/ && !/^Thread/ && !/^bt/ && !/^backtrace/ { in_thread=0 }
                            END { if (in_thread) print }
                            ' "${bt_log}.tmp"
                            echo ""
                            echo "=========================================="
                            echo "线程信息"
                            echo "=========================================="
                            grep "^Thread [0-9]" "${bt_log}.tmp" || true
                        } > "${bt_log}" 2>&1 || true
                        echo "Backtrace 已保存到: ${bt_log}"
                    else
                        # 如果没有找到 backtrace，保存完整的 GDB 输出用于调试
                        {
                            echo "=========================================="
                            echo "GDB 输出（未找到 backtrace，保存完整输出）"
                            echo "=========================================="
                            echo ""
                            cat "${bt_log}.tmp"
                        } > "${bt_log}" 2>&1 || true
                        echo "警告: 未找到 backtrace 信息，已保存完整 GDB 输出到: ${bt_log}"
                    fi
                    rm -f "${bt_log}.tmp"
                else
                    echo "警告: 无法附加到进程获取 backtrace"
                    # 保存错误信息
                    {
                        echo "=========================================="
                        echo "GDB 附加失败"
                        echo "=========================================="
                        echo ""
                        echo "进程 PID: ${test_pid}"
                        echo "GDB 错误输出:"
                        cat "${bt_log}.tmp" 2>&1 || echo "无法读取 GDB 输出"
                    } > "${bt_log}" 2>&1 || true
                    rm -f "${bt_log}.tmp"
                fi
                rm -f "${bt_script}"
                
                # 如果进程还在运行，终止它
                if kill -0 ${test_pid} 2>/dev/null; then
                    echo "终止测试进程..."
                    kill -TERM ${test_pid} 2>/dev/null || true
                    sleep 2
                    # 如果还在运行，强制终止
                    if kill -0 ${test_pid} 2>/dev/null; then
                        echo "强制终止测试进程..."
                        kill -KILL ${test_pid} 2>/dev/null || true
                    fi
                fi
            else
                echo "无法找到测试进程，可能已经退出"
                
                # 检查是否是退出阶段崩溃（测试用例全部通过，但程序异常退出）
                if grep -qE "\[.*OK.*\]" "${test_log}" "${gdb_log}" 2>/dev/null && \
                   ! grep -qE "\[==========\].*tests from.*test suites ran|\[.*PASSED.*\]|\[.*FAILED.*\]" "${test_log}" "${gdb_log}" 2>/dev/null; then
                    echo ""
                    echo "检测到退出阶段崩溃："
                    echo "  - 测试用例已运行并全部通过"
                    echo "  - 但程序在退出清理阶段异常终止"
                    echo ""
                    # 从 GDB 日志中提取退出阶段的信息
                    {
                        echo "=========================================="
                        echo "退出阶段崩溃分析"
                        echo "=========================================="
                        echo ""
                        echo "最后运行的测试用例:"
                        grep -E "\[.*OK.*\]" "${gdb_log}" | tail -10
                        echo ""
                        echo "GDB 日志末尾信息（可能包含崩溃线索）:"
                        tail -100 "${gdb_log}" | grep -v "^\[New\|^\[Thread\|^Reading\|^Loaded" | tail -50
                        echo ""
                        echo "可能的错误信号或断言:"
                        grep -E "SIGABRT|SIGSEGV|SIGTERM|Fatal|assertion|Program received signal|Thread.*received signal" "${gdb_log}" | tail -20 || echo "  未找到明确的错误信号"
                    } > "${bt_log}" 2>&1 || true
                fi
            fi
        fi
    }
    
    # 清理临时文件
    rm -f "${gdb_script}"
    
    if [ "${test_failed}" = false ]; then
        # 测试成功，删除所有相关日志，避免重复占用空间
        rm -f "${test_log}" "${gdb_log}" "${bt_log}" "${full_log}"
        success_count=$((success_count + 1))
        echo -e "${GREEN}  [成功] 第 ${iteration} 次测试通过${NC}"
    else
        # 测试失败，检查是否有实际的崩溃信号
        # 如果没有崩溃信号（如段错误），删除 gdb 日志以节省空间
        if ! grep -qE "Program received signal|Thread.*received signal|SIGSEGV|SIGABRT|SIGTERM|SIGBUS|SIGFPE|SIGILL" "${gdb_log}" 2>/dev/null; then
            # 没有崩溃信号，删除 gdb 日志
            rm -f "${gdb_log}"
            echo "  没有检测到崩溃信号，已删除 GDB 日志以节省空间"
        fi
        # 测试失败，保存相关日志
        failure_count=$((failure_count + 1))
        
        # 提取失败的测试用例和错误信息
        failed_tests=$(grep -E "^\s*\[.*FAILED.*\]|^\s*FAILED|^\s*TIMEOUT" "${test_log}" "${gdb_log}" 2>/dev/null | head -20 | sed 's/^[[:space:]]*//' || true)
        
        # 提取错误原因（信号、断言失败等）
        error_reason=""
        
        # 检查是否是退出阶段崩溃
        exit_phase_crash=false
        if grep -qE "\[.*OK.*\]" "${test_log}" "${gdb_log}" 2>/dev/null && \
           ! grep -qE "\[==========\].*tests from.*test suites ran|\[.*PASSED.*\]|\[.*FAILED.*\]" "${test_log}" "${gdb_log}" 2>/dev/null; then
            exit_phase_crash=true
        fi
        
        if [ "${exit_phase_crash}" = true ]; then
            error_reason="退出阶段崩溃 (测试用例全部通过，但程序在退出清理时异常终止)"
            if grep -q "SIGABRT" "${gdb_log}" 2>/dev/null; then
                error_reason="${error_reason} - SIGABRT"
                if grep -q "assertion failed" "${gdb_log}" 2>/dev/null; then
                    assertion_info=$(grep "assertion failed" "${gdb_log}" | head -1 | sed 's/^[[:space:]]*//')
                    error_reason="${error_reason}: ${assertion_info}"
                fi
            elif grep -q "SIGSEGV" "${gdb_log}" 2>/dev/null; then
                error_reason="${error_reason} - SIGSEGV"
            fi
        elif grep -q "SIGABRT" "${gdb_log}" 2>/dev/null; then
            error_reason="SIGABRT (程序异常终止)"
            if grep -q "assertion failed" "${gdb_log}" 2>/dev/null; then
                assertion_info=$(grep "assertion failed" "${gdb_log}" | head -1 | sed 's/^[[:space:]]*//')
                error_reason="${error_reason}: ${assertion_info}"
            fi
        elif grep -q "SIGSEGV" "${gdb_log}" 2>/dev/null; then
            error_reason="SIGSEGV (段错误)"
        elif grep -q "killed by signal" "${gdb_log}" 2>/dev/null; then
            error_reason=$(grep "killed by signal" "${gdb_log}" | head -1 | sed 's/^[[:space:]]*//')
        elif [ "${test_timeout}" = true ]; then
            error_reason="TIMEOUT (测试超时 ${TEST_TIMEOUT} 秒)"
        elif grep -q "FAILED" "${test_log}" "${gdb_log}" 2>/dev/null; then
            error_reason="测试用例失败"
        else
            error_reason="未知错误 (退出码: ${test_exit_code})"
        fi
        
        # 保存 Meson full log（包含测试运行的详细信息）
        full_log_src="${BUILD_DIR}/meson-logs/testlog.txt"
        if [ -f "${full_log_src}" ]; then
            cp "${full_log_src}" "${full_log}"
        fi
        
        # 输出失败信息
        echo -e "${RED}  [失败] 第 ${iteration} 次测试失败${NC}"
        echo -e "${YELLOW}  错误原因: ${error_reason}${NC}"
        if [ -n "${failed_tests}" ]; then
            echo -e "${YELLOW}  失败的测试用例:${NC}"
            echo "${failed_tests}" | head -10 | sed 's/^/    /'
            if [ $(echo "${failed_tests}" | wc -l) -gt 10 ]; then
                echo "    ... (还有更多失败的测试用例)"
            fi
        fi
        echo -e "${YELLOW}  日志文件:${NC}"
        if [ -f "${bt_log}" ] && [ -s "${bt_log}" ]; then
            echo "    - Backtrace: ${bt_log} (包含崩溃堆栈信息，优先查看)"
        fi
        if [ -f "${gdb_log}" ]; then
            echo "    - GDB 完整日志: ${gdb_log} (包含 GDB 调试信息和测试输出)"
        fi
        if [ -f "${test_log}" ] && [ -s "${test_log}" ]; then
            echo "    - 测试输出: ${test_log} (仅测试用例输出，已过滤 GDB 信息)"
        fi
        if [ -f "${full_log}" ]; then
            echo "    - Meson full log: ${full_log} (Meson 测试框架的完整日志)"
        fi
    fi
    
    echo ""
done

# 输出统计信息
echo "=========================================="
echo "测试循环完成"
echo "=========================================="
echo "总次数: ${MAX_ITERATIONS}"
echo -e "成功: ${GREEN}${success_count}${NC}"
echo -e "失败: ${RED}${failure_count}${NC}"
echo ""
if [ ${failure_count} -gt 0 ]; then
    echo -e "${YELLOW}失败日志保存在: ${LOG_DIR}${NC}"
    echo ""
    echo "失败的测试迭代:"
    # 收集所有失败迭代的编号（从所有可能的日志文件）
    failed_iterations=""
    for log_file in "${LOG_DIR}"/test_*.log; do
        if [ -f "${log_file}" ]; then
            iteration=$(basename "${log_file}" | sed -E 's/test_([0-9]+)_.*\.log/\1/' | head -1)
            if [ -n "${iteration}" ] && [ "${iteration}" != "*" ]; then
                if ! echo "${failed_iterations}" | grep -q "^${iteration}$\|^${iteration} \| ${iteration}$\| ${iteration} "; then
                    failed_iterations="${failed_iterations} ${iteration}"
                fi
            fi
        fi
    done
    
    # 对迭代号排序并去重
    for iteration in $(echo "${failed_iterations}" | tr ' ' '\n' | sort -n | uniq); do
        if [ -n "${iteration}" ]; then
            echo ""
            echo "  迭代 ${iteration}:"
            
            # 提取错误原因（从 backtrace 或 gdb 日志）
            gdb_file="${LOG_DIR}/test_${iteration}_gdb.log"
            bt_file="${LOG_DIR}/test_${iteration}_backtrace.log"
            
            if [ -f "${gdb_file}" ] && [ -s "${gdb_file}" ]; then
                if grep -q "SIGABRT" "${gdb_file}" 2>/dev/null; then
                    echo "    错误: SIGABRT (程序异常终止)"
                    if grep -q "assertion failed" "${gdb_file}" 2>/dev/null; then
                        assertion_info=$(grep "assertion failed" "${gdb_file}" | head -1 | sed 's/^/      /')
                        echo "${assertion_info}"
                    fi
                elif grep -q "SIGSEGV" "${gdb_file}" 2>/dev/null; then
                    echo "    错误: SIGSEGV (段错误)"
                elif grep -q "killed by signal" "${gdb_file}" 2>/dev/null; then
                    signal_info=$(grep "killed by signal" "${gdb_file}" | head -1 | sed 's/^/      /')
                    echo "    错误: ${signal_info}"
                elif grep -q "FAILED" "${gdb_file}" 2>/dev/null; then
                    echo "    错误: 测试用例失败"
                fi
            elif [ -f "${bt_file}" ] && [ -s "${bt_file}" ]; then
                if grep -q "SIGSEGV\|Segmentation fault" "${bt_file}" 2>/dev/null; then
                    echo "    错误: SIGSEGV (段错误)"
                elif grep -q "SIGABRT" "${bt_file}" 2>/dev/null; then
                    echo "    错误: SIGABRT (程序异常终止)"
                fi
            fi
            
            # 列出相关日志文件（只列出实际存在的文件）
            echo "    日志:"
            if [ -f "${bt_file}" ] && [ -s "${bt_file}" ]; then
                echo "      - ${bt_file} (Backtrace，优先查看)"
            fi
            if [ -f "${gdb_file}" ] && [ -s "${gdb_file}" ]; then
                echo "      - ${gdb_file} (GDB 完整日志)"
            fi
            test_file="${LOG_DIR}/test_${iteration}.log"
            if [ -f "${test_file}" ] && [ -s "${test_file}" ]; then
                echo "      - ${test_file} (测试输出)"
            fi
            full_file="${LOG_DIR}/test_${iteration}_full.log"
            if [ -f "${full_file}" ] && [ -s "${full_file}" ]; then
                echo "      - ${full_file} (Meson 测试日志)"
            fi
        fi
    done
else
    echo -e "${GREEN}所有测试都通过了！${NC}"
    # 清理日志目录（如果所有测试都成功）
    rm -rf "${LOG_DIR}"
fi

