#!/bin/bash

# 智能编译脚本 - 根据系统资源动态调整编译并发数
# 版本：2.0
#
# 功能:
#   1. 根据系统资源（CPU/内存/Swap）动态计算最优编译并发数
#   2. 配置 ccache（绕过编译机 CCACHE_DIR 只读问题）
#   3. 每行输出自动加相对时间戳 [HH:MM:SS.mmm]（依赖 moreutils 的 ts；
#      SMART_BUILD_NO_TIMESTAMP=1 可关闭）
#   4. 所有输出同时落盘到 tests/smart-build-YYYYMMDD-HHMMSS.log
#      （格式参照 temp/bingo-test-*.log；文件已加入 .gitignore）
#      SMART_BUILD_NO_LOGFILE=1   可关闭落盘
#      SMART_BUILD_LOG_KEEP=<n>   保留最近 N 份日志（默认 10，0 表示不清理）
#   5. 一键完成 setup → compile → test 全流程
#
# 用法:
#   ./scripts/smart_build.sh                   # 常规构建（自动检测/复用缓存）
#   ./scripts/smart_build.sh --clean|--clean-build|-c   # 一级清理：仅清理 builddir 后重建
#   ./scripts/smart_build.sh --clean-prebuilt|-C        # 二级清理：clean-build + 清理 libmcpp conan 缓存
#   ./scripts/smart_build.sh --clean-all|-X             # 三级清理：clean-prebuilt + 清空 ccache
#   ./scripts/smart_build.sh --coverage        # 启用代码覆盖率
#   ./scripts/smart_build.sh --fast-coverage   # 使用快速覆盖率模式

set -e

# ────────────────────────────────────────────────────────────
# 颜色输出
# ────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'
DIM='\033[2m'

print_info()    { echo -e "${GREEN}[INFO]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_error()   { echo -e "${RED}[ERROR]${NC} $1"; }
print_step()    { echo -e "${CYAN}${BOLD}[STEP]${NC} $1"; }

# ────────────────────────────────────────────────────────────
# 相对时间戳 + 日志落盘：
#   - 每行输出前加 [HH:MM:SS.mmm]（自脚本启动计时）
#     参考 README.md: `ts -s '[%H:%M:%.S]'`（来自 moreutils）
#     默认启用；SMART_BUILD_NO_TIMESTAMP=1 关闭
#     未安装 ts 时静默回退（不阻塞构建，但仍会落盘日志）
#   - 同步 tee 到 tests/smart-build-YYYYMMDD-HHMMSS.log
#     格式与 temp/bingo-test-*.log 保持一致（每行 [HH:MM:SS.ffffff] 前缀）
#     默认启用；SMART_BUILD_NO_LOGFILE=1 关闭
#   - 通过 exec 重定向，覆盖脚本自身 + 所有子进程（meson/ninja/ccache…）
# ────────────────────────────────────────────────────────────
setup_relative_timestamp() {
    [ -n "${_SMART_BUILD_TS_ACTIVE:-}" ] && return 0

    local ts_enabled=1
    [ "${SMART_BUILD_NO_TIMESTAMP:-0}" = "1" ] && ts_enabled=0
    local _warn_no_ts=0
    if [ "$ts_enabled" = "1" ] && ! command -v ts >/dev/null 2>&1; then
        ts_enabled=0
        _warn_no_ts=1
    fi

    local logfile_enabled=1
    [ "${SMART_BUILD_NO_LOGFILE:-0}" = "1" ] && logfile_enabled=0

    local log_file=""
    local _warn_pruned_count=0
    if [ "$logfile_enabled" = "1" ]; then
        local log_dir="tests"
        if mkdir -p "$log_dir" 2>/dev/null; then
            local _keep_logs="${SMART_BUILD_LOG_KEEP:-10}"
            if ! [[ "$_keep_logs" =~ ^[0-9]+$ ]]; then
                _keep_logs=10
            fi
            if [ "$_keep_logs" -gt 0 ]; then
                local _keep_history=$(( _keep_logs - 1 ))
                local _pruned
                _pruned=$(ls -1t "$log_dir"/smart-build-*.log 2>/dev/null \
                    | tail -n +$(( _keep_history + 1 )) \
                    | tee >(xargs -r rm -f --) \
                    | wc -l)
                _warn_pruned_count="${_pruned:-0}"
            fi
            log_file="$log_dir/smart-build-$(date +%Y%m%d-%H%M%S).log"
            export _SMART_BUILD_LOG_FILE="$log_file"
        else
            logfile_enabled=0
            _warn_no_logdir=1
        fi
    fi

    if [ "$ts_enabled" = "0" ] && [ "$logfile_enabled" = "0" ]; then
        export _SMART_BUILD_TS_ACTIVE=1
        return 0
    fi

    export _SMART_BUILD_TS_ACTIVE=1
    if [ "$ts_enabled" = "1" ] && [ "$logfile_enabled" = "1" ]; then
        exec > >(stdbuf -oL -eL ts -s '[%H:%M:%.S]' | tee "$log_file") 2>&1
    elif [ "$ts_enabled" = "1" ]; then
        exec > >(stdbuf -oL -eL ts -s '[%H:%M:%.S]') 2>&1
    else
        exec > >(tee "$log_file") 2>&1
    fi

    [ "$_warn_no_ts" = "1" ] && \
        print_warning "未安装 ts（moreutils），日志未加时间戳（apt-get install -y moreutils）"
    [ "${_warn_no_logdir:-0}" = "1" ] && \
        print_warning "无法创建 tests/ 目录，跳过日志落盘"
    if [ "$logfile_enabled" = "1" ]; then
        if [ "${_warn_pruned_count:-0}" -gt 0 ]; then
            print_info "构建日志同步保存: $log_file（已滚动清理旧日志 ${_warn_pruned_count} 份，保留最近 ${SMART_BUILD_LOG_KEEP:-10} 份）"
        else
            print_info "构建日志同步保存: $log_file（保留最近 ${SMART_BUILD_LOG_KEEP:-10} 份）"
        fi
    fi
}

# ── 计时工具 ──
_phase_times=()
_phase_names=()
_phase_start=0
_total_start=0

timer_start() { _phase_start=$(date +%s); }

timer_record() {
    local name="$1"
    local elapsed=$(( $(date +%s) - _phase_start ))
    _phase_names+=("$name")
    _phase_times+=("$elapsed")
    echo -e "  ${GREEN}done${NC} ${DIM}${name} 用时 $(format_duration "$elapsed")${NC}"
}

format_duration() {
    local secs=$1
    if [ "$secs" -ge 60 ]; then
        printf "%dm%02ds" $((secs / 60)) $((secs % 60))
    else
        printf "%ds" "$secs"
    fi
}

print_phase_header() {
    local step="$1" total="$2" title="$3"
    echo ""
    echo -e "${CYAN}${BOLD}  [$step/$total] $title${NC}"
    echo -e "${DIM}  $(printf '%.0s─' {1..50})${NC}"
}

print_summary() {
    local total_elapsed=$(( $(date +%s) - _total_start ))
    echo ""
    echo -e "${BOLD}  ┌──────────────────────────────────────────────┐${NC}"
    echo -e "${BOLD}  │            构建耗时统计                      │${NC}"
    echo -e "${BOLD}  ├─────────────────────────┬────────────────────┤${NC}"
    for i in "${!_phase_names[@]}"; do
        local pct=0
        [ "$total_elapsed" -gt 0 ] && pct=$(( _phase_times[$i] * 100 / total_elapsed ))
        printf "  ${BOLD}│${NC} %-23s ${BOLD}│${NC} %6s  ${DIM}(%2d%%)${NC}      ${BOLD}│${NC}\n" \
            "${_phase_names[$i]}" "$(format_duration "${_phase_times[$i]}")" "$pct"
    done
    echo -e "${BOLD}  ├─────────────────────────┼────────────────────┤${NC}"
    printf "  ${BOLD}│${NC} ${GREEN}%-23s${NC} ${BOLD}│${NC} ${GREEN}%6s${NC}  ${DIM}     ${NC}      ${BOLD}│${NC}\n" \
        "总计" "$(format_duration "$total_elapsed")"
    echo -e "${BOLD}  └─────────────────────────┴────────────────────┘${NC}"
}

# ────────────────────────────────────────────────────────────
# 系统资源检测与最优并发数计算
# ────────────────────────────────────────────────────────────

get_system_info() {
    local cpu_cores=$(nproc)
    # cgroup v2 CPU 配额检测：容器/受限环境下 nproc 仍返回宿主核数，
    # 但实际可用 CPU 受 cpu.max 限制；读取真实配额作为有效核数。
    if [ -r /sys/fs/cgroup/cpu.max ]; then
        local _cpu_max
        _cpu_max=$(cat /sys/fs/cgroup/cpu.max 2>/dev/null)
        local _quota _period
        _quota=$(echo "$_cpu_max" | awk '{print $1}')
        _period=$(echo "$_cpu_max" | awk '{print $2}')
        if [ "$_quota" != "max" ] && [ -n "$_quota" ] && [ -n "$_period" ] && [ "$_period" -gt 0 ]; then
            local _effective=$(( (_quota + _period - 1) / _period ))
            if [ "$_effective" -gt 0 ] && [ "$_effective" -lt "$cpu_cores" ]; then
                cpu_cores=$_effective
            fi
        fi
    fi
    local total_mem_kb=$(grep MemTotal /proc/meminfo | awk '{print $2}')
    local available_mem_kb=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
    local swap_total_kb=$(grep SwapTotal /proc/meminfo | awk '{print $2}')
    local total_mem_gb=$((total_mem_kb / 1024 / 1024))
    local available_mem_gb=$((available_mem_kb / 1024 / 1024))
    local swap_total_gb=$((swap_total_kb / 1024 / 1024))
    echo "$cpu_cores $total_mem_gb $available_mem_gb $swap_total_gb"
}

calculate_optimal_jobs() {
    local cpu_cores=$1
    local available_mem_gb=$2
    local swap_total_gb=$3

    # 共享机策略（可通过环境变量覆盖）：
    #   SMART_BUILD_MEM_BUDGET_GB  本次构建可用内存硬上限（cgroup/配额）
    #                              默认 = min(系统 available, 40)，避免与其他用户争抢
    #   SMART_BUILD_MEM_PER_JOB_MB 单 g++ 进程预估峰值内存
    #                              默认 1200MB：fast_debug_info 启用后的实测峰值
    #                              关闭 fast_debug_info 时建议设 1700
    local mem_budget_gb="${SMART_BUILD_MEM_BUDGET_GB:-}"
    if [ -z "$mem_budget_gb" ]; then
        mem_budget_gb=$(( available_mem_gb < 40 ? available_mem_gb : 40 ))
    fi
    local mem_per_job_mb="${SMART_BUILD_MEM_PER_JOB_MB:-1200}"

    local mem_based_jobs=$(( mem_budget_gb * 1024 / mem_per_job_mb ))
    # CPU 预算：受 cgroup 严格限制（<=16 核）时应铺满配额以逼近 wall 下限；
    # 未受限的共享机才让出 1/4 给其他用户。
    local cpu_based_jobs
    if [ "$cpu_cores" -le 16 ]; then
        cpu_based_jobs=$cpu_cores
    else
        cpu_based_jobs=$(( cpu_cores * 3 / 4 ))
    fi

    # 无 swap 再留 10% 余量，避免 OOM
    if [ "$swap_total_gb" -eq 0 ]; then
        mem_based_jobs=$(( mem_based_jobs * 9 / 10 ))
    fi

    local jobs=$(( mem_based_jobs < cpu_based_jobs ? mem_based_jobs : cpu_based_jobs ))
    jobs=$(( jobs < 1 ? 1 : jobs ))
    jobs=$(( jobs > 64 ? 64 : jobs ))

    # cgroup CPU 配额严格受限时（如 cpu.max=8），过多的 make/ninja 并行会导致线程被
    # 大量调度/上下文切换而非真正同时执行，反而拖慢总耗时。此场景下将并发数限制为
    # CPU 配额的 1.25 倍（留出少量等待 I/O 的冗余），避免无效争抢。
    if [ "$cpu_cores" -le 16 ]; then
        local _cpu_throttle_ceiling=$(( cpu_cores * 5 / 4 + 1 ))
        if [ "$jobs" -gt "$_cpu_throttle_ceiling" ]; then
            jobs=$_cpu_throttle_ceiling
        fi
    fi

    # 用普通全局变量返回多值（避免 $() 子 shell 吞掉 export）
    _SMART_OPTIMAL_JOBS=$jobs
    _SMART_MEM_BUDGET_GB=$mem_budget_gb
    _SMART_MEM_PER_JOB_MB=$mem_per_job_mb
    _SMART_MEM_BASED_JOBS=$mem_based_jobs
    _SMART_CPU_BASED_JOBS=$cpu_based_jobs
}

# ────────────────────────────────────────────────────────────
# ccache 配置（绕过编译机 CCACHE_DIR 指向只读分区的问题）
# ────────────────────────────────────────────────────────────

setup_ccache() {
    if ! command -v ccache &>/dev/null; then
        print_warning "ccache 未安装，跳过缓存加速（建议: apt install ccache）"
        return 0
    fi

    # 先寻找可写目录，避免 CCACHE_DIR 落在只读分区
    local _uid
    _uid="$(id -u)"
    local _candidate_dirs=()
    if [ -n "${CCACHE_DIR:-}" ]; then
        _candidate_dirs+=("$CCACHE_DIR")
    fi
    _candidate_dirs+=("${HOME}/.ccache")
    if [ -n "${XDG_CACHE_HOME:-}" ]; then
        _candidate_dirs+=("${XDG_CACHE_HOME}/ccache")
    fi
    _candidate_dirs+=("/tmp/ccache-${_uid}")

    local ccache_dir=""
    local d
    for d in "${_candidate_dirs[@]}"; do
        [ -z "$d" ] && continue
        if mkdir -p "$d" 2>/dev/null && touch "$d/.ccache_write_test" 2>/dev/null; then
            rm -f "$d/.ccache_write_test"
            ccache_dir="$d"
            break
        fi
    done

    if [ -z "$ccache_dir" ]; then
        print_warning "无法为 ccache 配置可写目录，已设置 CCACHE_DISABLE=1"
        export CCACHE_DISABLE=1
        return 0
    fi

    local real_ccache
    real_ccache="$(command -v ccache)"

    # 调优 ccache：
    #   max_size=30G        默认仅 5G，多构型容易被 evict
    #   compression=1 + level=1
    #                       压缩率换 IO，level=1 写入开销最低，CPU 占用 <5%
    #   sloppiness          time_macros / include_file_(m|c)time / pch_defines / locale
    #                       覆盖项目的 __DATE__、PCH、locale-sensitive 头，提升命中率
    #   pch_extsum=true     PCH 命中校验外部 sum，避免 .gch 时间戳抖动导致全部失效
    #   base_dir=.          把绝对路径改成相对，跨目录构建（例 worktree）也能命中
    export CCACHE_DIR="$ccache_dir"
    export CCACHE_COMPRESS=1
    export CCACHE_COMPRESSLEVEL=1
    export CCACHE_SLOPPINESS="time_macros,include_file_mtime,include_file_ctime,pch_defines,locale,system_headers"
    export CCACHE_PCH_EXTSUM=true
    export CCACHE_BASEDIR="$(pwd)"
    export CCACHE_NOHASHDIR=1
    ccache -M 30G >/dev/null 2>&1 || true

    print_info "ccache → $ccache_dir ($(ccache --version | head -1)) max=30G compress=L1 sloppiness=PCH+time"
}

# ────────────────────────────────────────────────────────────
# 生成覆盖率报告
# ────────────────────────────────────────────────────────────

generate_coverage_report() {
    local build_dir=$1
    local output_dir=$2

    print_info "开始生成覆盖率报告..."

    for tool in lcov genhtml; do
        if ! command -v $tool >/dev/null 2>&1; then
            print_error "$tool 未安装，无法生成覆盖率报告"
            return 1
        fi
    done

    mkdir -p "$output_dir"

    print_info "收集覆盖率数据..."
    local _jobs="${BUILD_JOBS:-$(nproc)}"
    lcov --capture --directory "$build_dir" --output-file "$output_dir/coverage.info" \
        --rc branch_coverage=1 \
        --rc geninfo_unexecuted_blocks=1 \
        -j "$_jobs" \
        --ignore-errors mismatch,gcov,negative

    print_info "过滤覆盖率数据..."
    lcov --remove "$output_dir/coverage.info" \
        '/usr/*' '*/tests/*' '*/test/*' '*_test.cpp' '*_test.h' \
        '*/gtest/*' '*/boost/*' '*/builddir/*' '*/subprojects/*' '*/gen/*' \
        --output-file "$output_dir/coverage_filtered.info" \
        --rc branch_coverage=1 \
        -j "$_jobs" \
        --ignore-errors mismatch,negative,unused

    print_info "生成 HTML 覆盖率报告..."
    genhtml "$output_dir/coverage_filtered.info" --output-directory "$output_dir/html" \
        --title "libmcpp 代码覆盖率报告" \
        --show-details --legend --branch-coverage --function-coverage \
        -j "$_jobs" \
        --ignore-errors mismatch,negative

    print_info "生成覆盖率汇总..."
    lcov --summary "$output_dir/coverage_filtered.info" \
        --rc branch_coverage=1 \
        --ignore-errors mismatch,negative > "$output_dir/coverage_summary.txt"

    print_info "覆盖率统计结果:"
    cat "$output_dir/coverage_summary.txt"
    print_info "HTML 报告: $output_dir/html/index.html"
}

# ────────────────────────────────────────────────────────────
# 主函数
# ────────────────────────────────────────────────────────────

main() {
    setup_relative_timestamp
    echo ""
    print_info "==========================================="
    print_info " smart_build.sh v2.0 — 智能编译"
    print_info "==========================================="

    # 解析参数
    local enable_coverage=false
    local clean_build=false
    local clean_cache=false
    local purge_cache=false
    local coverage_output_dir="coverage"
    local fast_coverage=false
    local meson_extra_args=()

    while [[ $# -gt 0 ]]; do
        case $1 in
            --clean|--clean-build|-c)
                clean_build=true
                shift
                ;;
            --clean-prebuilt|-C)
                clean_build=true
                clean_cache=true
                shift
                ;;
            --clean-all|-X|--purge-cache)
                clean_build=true
                clean_cache=true
                purge_cache=true
                shift
                ;;
            --coverage)
                enable_coverage=true
                shift
                ;;
            --coverage-output)
                coverage_output_dir="$2"
                shift 2
                ;;
            --fast-coverage)
                enable_coverage=true
                fast_coverage=true
                shift
                ;;
            -D*)
                meson_extra_args+=("$1")
                shift
                ;;
            -h|--help)
                echo "用法: $0 [选项] [-D<meson参数>...]"
                echo ""
                echo "选项:"
                echo "  --clean|--clean-build|-c   一级清理：仅清理 builddir 后重建"
                echo "  --clean-prebuilt|-C        二级清理：clean-build + 清理 libmcpp conan 缓存"
                echo "  --clean-all|-X             三级清理：clean-prebuilt + 清空 ccache"
                echo "  --purge-cache              兼容别名，等同 --clean-all"
                echo "  --coverage                 启用代码覆盖率统计"
                echo "  --fast-coverage            使用快速覆盖率模式"
                echo "  --coverage-output <dir>    覆盖率输出目录（默认: coverage）"
                echo "  -D<key>=<val>              传递额外 meson 参数（如 -Dtests=false）"
                echo "  -h, --help                 显示帮助信息"
                echo ""
                echo "环境变量 (进一步调优):"
                echo "  SMART_BUILD_MEM_BUDGET_GB=<n>   覆盖内存预算（GB）"
                echo "  SMART_BUILD_MEM_PER_JOB_MB=<n>  覆盖单任务内存预估（MB）"
                echo "  SMART_BUILD_NO_TIMESTAMP=1      关闭每行相对时间戳前缀（默认启用，需要 moreutils 的 ts）"
                echo "  SMART_BUILD_NO_LOGFILE=1        关闭日志落盘（默认写入 tests/smart-build-YYYYMMDD-HHMMSS.log）"
                echo "  SMART_BUILD_LOG_KEEP=<n>        保留最近 N 份构建日志（默认 10；0 表示不清理）"
                exit 0
                ;;
            *)
                print_error "未知参数: $1（使用 --help 查看帮助）"
                exit 1
                ;;
        esac
    done

    # 检查是否在项目根目录
    if [ ! -f "meson.build" ]; then
        print_error "未找到 meson.build，请确认在项目根目录执行"
        exit 1
    fi

    # 自动检测并安装依赖工具
    if command -v apt-get >/dev/null 2>&1; then
        local basic_packages="libboost-all-dev libgtest-dev ninja-build gcc g++ ccache"
        local coverage_packages=""

        if [ "$enable_coverage" = true ]; then
            coverage_packages="lcov"
        fi

        for pkg in $basic_packages $coverage_packages; do
            if ! dpkg -s "$pkg" >/dev/null 2>&1; then
                print_warning "$pkg 未安装，正在自动安装..."
                if apt-get update -qq && apt-get install -y -qq "$pkg"; then
                    print_info "$pkg 安装完成"
                else
                    print_error "$pkg 安装失败，请手动安装后重试"
                    exit 1
                fi
            fi
        done

        if [ "$enable_coverage" = true ] && ! command -v gcov >/dev/null 2>&1; then
            print_error "gcov 不可用，请确保已安装 gcc"
            exit 1
        fi
    fi

    # 清理构建目录
    if [ "$clean_build" = true ]; then
        print_warning "清理 builddir..."
        rm -rf builddir
        if [ "$enable_coverage" = true ]; then
            rm -rf "$coverage_output_dir"
            find . -name "*.gcda" -delete 2>/dev/null || true
            find . -name "*.gcno" -delete 2>/dev/null || true
        fi
    fi

    # 二级清理：清理 libmcpp conan 缓存
    if [ "$clean_cache" = true ]; then
        if command -v conan &>/dev/null; then
            local comp_name=""
            if [ -f "mds/service.json" ]; then
                comp_name=$(python3 -c "import json; print(json.load(open('mds/service.json')).get('name',''))" 2>/dev/null || true)
            fi
            if [ -z "$comp_name" ]; then
                comp_name=$(basename "$(pwd)")
            fi
            print_warning "清理 $comp_name 的 conan 包缓存..."
            conan remove -c "$comp_name/*" 2>/dev/null || true
        fi
    fi

    # 三级清理：清空 ccache
    if [ "$purge_cache" = true ]; then
        if command -v ccache &>/dev/null; then
            print_warning "深度清理：清除 ccache 编译缓存..."
            local _ccache_dir="$HOME/.ccache"
            CCACHE_DIR="$_ccache_dir" ccache -C 2>/dev/null || true
        fi
    fi

    # ── Step 1: 配置 ccache ──
    print_phase_header 1 3 "配置 ccache"
    timer_start
    setup_ccache
    timer_record "ccache 配置"

    # ── Step 2: 系统资源检测 ──
    print_phase_header 2 3 "系统资源检测"
    timer_start
    local system_info=($(get_system_info))
    local cpu_cores=${system_info[0]}
    local total_mem_gb=${system_info[1]}
    local available_mem_gb=${system_info[2]}
    local swap_total_gb=${system_info[3]}

    print_info "系统: ${cpu_cores} 核, ${total_mem_gb}GB 总内存, ${available_mem_gb}GB 可用, ${swap_total_gb}GB Swap"

    if [ "$available_mem_gb" -lt 4 ]; then
        print_error "可用内存不足 4GB，建议释放内存后再编译"
        exit 1
    fi
    if [ "$swap_total_gb" -eq 0 ]; then
        print_warning "无 Swap 分区，将使用保守并发策略"
    fi

    calculate_optimal_jobs "$cpu_cores" "$available_mem_gb" "$swap_total_gb"
    local optimal_jobs="$_SMART_OPTIMAL_JOBS"
    export BUILD_JOBS="$optimal_jobs"
    print_info "内存预算: ${_SMART_MEM_BUDGET_GB}GB，单任务预估: ${_SMART_MEM_PER_JOB_MB}MB"
    print_info "最优并发数: ${optimal_jobs}（基于内存=${_SMART_MEM_BASED_JOBS}, 基于CPU=${_SMART_CPU_BASED_JOBS}）"
    timer_record "系统资源检测"

    # ── Step 3: 编译 + 测试 ──
    print_phase_header 3 3 "编译 libmcpp（并发=$optimal_jobs）"
    _total_start=$(date +%s)
    timer_start

    local build_dir="builddir"
    local meson_options=()
    if [ "$enable_coverage" = true ]; then
        meson_options+=("-Denable_coverage=true")
        print_info "启用代码覆盖率统计"
    fi
    meson_options+=("${meson_extra_args[@]}")

    if [ ! -d "$build_dir" ]; then
        print_info "配置 meson 构建环境..."
        meson setup "$build_dir" "${meson_options[@]}"
    elif [ "$enable_coverage" = true ]; then
        print_info "重新配置构建环境以启用覆盖率..."
        meson configure "$build_dir" "${meson_options[@]}"
    else
        print_info "使用已有构建目录（跳过 meson setup）"
    fi

    print_info "开始编译..."
    if meson compile -C "$build_dir" -j "$optimal_jobs"; then
        print_info "编译成功"
    else
        print_error "编译失败"
        exit 1
    fi
    timer_record "libmcpp 编译"

    # 运行测试
    timer_start
    print_info "运行测试..."
    local _test_jobs="${BUILD_JOBS:-1}"
    if meson test -C "$build_dir" --num-processes "$_test_jobs"; then
        print_info "测试通过"
    else
        local testlog="$build_dir/meson-logs/testlog.txt"
        if [ -f "$testlog" ] && \
           grep -q "result:[[:space:]]*killed by signal 15 SIGTERM" "$testlog"; then
            print_warning "检测到测试超时，自动重试（timeout-multiplier=3）..."
            if meson test -C "$build_dir" --num-processes "$_test_jobs" --timeout-multiplier 3; then
                print_info "超时重试后测试通过"
            else
                print_error "超时重试后测试仍失败"
                exit 1
            fi
        else
            print_error "测试失败"
            exit 1
        fi
    fi
    timer_record "测试执行"

    # 覆盖率报告
    if [ "$enable_coverage" = true ]; then
        timer_start
        if [ "$fast_coverage" = true ]; then
            print_info "使用快速覆盖率模式..."
            ./scripts/coverage.sh --fast --output "$coverage_output_dir" --build-dir "$build_dir"
        else
            generate_coverage_report "$build_dir" "$coverage_output_dir"
        fi
        timer_record "覆盖率报告"
    fi

    print_summary
    [ -n "${_SMART_BUILD_LOG_FILE:-}" ] && \
        print_info "完整构建日志: ${_SMART_BUILD_LOG_FILE}"
}

# 如果脚本被直接执行
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
