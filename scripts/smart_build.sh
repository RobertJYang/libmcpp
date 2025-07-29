#!/bin/bash

# 智能编译脚本 - 根据系统资源动态调整编译并发数
# 日期：2025-07-15
# 版本：1.0
# 说明：本脚本用于根据系统资源动态调整编译并发数，以避免因过度并发导致的编译卡死问题
# 使用方法：在项目根目录下执行 ./scripts/smart_build.sh
# 注意事项：本脚本仅适用于Linux系统，且需要安装meson和g++

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 获取系统信息
get_system_info() {
    local cpu_cores=$(nproc)
    local total_mem_kb=$(grep MemTotal /proc/meminfo | awk '{print $2}')
    local available_mem_kb=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
    local swap_total_kb=$(grep SwapTotal /proc/meminfo | awk '{print $2}')
    
    # 转换为GB
    local total_mem_gb=$((total_mem_kb / 1024 / 1024))
    local available_mem_gb=$((available_mem_kb / 1024 / 1024))
    local swap_total_gb=$((swap_total_kb / 1024 / 1024))
    
    echo "$cpu_cores $total_mem_gb $available_mem_gb $swap_total_gb"
}

# 计算最优编译并发数
calculate_optimal_jobs() {
    local cpu_cores=$1
    local total_mem_gb=$2
    local available_mem_gb=$3
    local swap_total_gb=$4
    
    # 基于内存的并发数计算
    # C++编译平均每个进程消耗约1-2GB内存，保守估计用1.5GB
    local mem_based_jobs=$((available_mem_gb * 2 / 3 / 2))  # 使用2/3可用内存，每个任务2GB
    
    # 基于CPU的并发数计算
    local cpu_based_jobs=$((cpu_cores / 2))  # 使用一半CPU核数
    
    # 如果没有swap，更保守一些
    if [ $swap_total_gb -eq 0 ]; then
        mem_based_jobs=$((mem_based_jobs * 2 / 3))
    fi
    
    # 取较小值，但至少为1
    local optimal_jobs=$(( mem_based_jobs < cpu_based_jobs ? mem_based_jobs : cpu_based_jobs ))
    optimal_jobs=$(( optimal_jobs < 1 ? 1 : optimal_jobs ))
    
    # 最大并发数限制（避免过度并发）
    local max_jobs=64
    optimal_jobs=$(( optimal_jobs > max_jobs ? max_jobs : optimal_jobs ))
    
    echo $optimal_jobs
}

# 生成覆盖率报告
generate_coverage_report() {
    local build_dir=$1
    local output_dir=$2
    
    print_info "开始生成覆盖率报告..."
    
    # 检查必需工具
    if ! command -v lcov >/dev/null 2>&1; then
        print_error "lcov 未安装，无法生成覆盖率报告"
        return 1
    fi
    
    if ! command -v genhtml >/dev/null 2>&1; then
        print_error "genhtml 未安装，无法生成 HTML 覆盖率报告"
        return 1
    fi
    
    # 创建输出目录
    mkdir -p "$output_dir"
    
    # 收集覆盖率数据
    print_info "收集覆盖率数据..."
    lcov --capture --directory "$build_dir" --output-file "$output_dir/coverage.info" \
        --rc lcov_branch_coverage=1 \
        --rc geninfo_unexecuted_blocks=1 \
        --ignore-errors mismatch,gcov,negative
    
    # 过滤掉系统头文件和测试文件
    print_info "过滤覆盖率数据..."
    lcov --remove "$output_dir/coverage.info" \
        '/usr/*' \
        '*/tests/*' \
        '*/test/*' \
        '*_test.cpp' \
        '*_test.h' \
        '*/gtest/*' \
        '*/boost/*' \
        '*/builddir/*' \
        --output-file "$output_dir/coverage_filtered.info" \
        --rc lcov_branch_coverage=1 \
        --ignore-errors mismatch,negative,unused
    
    # 生成 HTML 报告
    print_info "生成 HTML 覆盖率报告..."
    genhtml "$output_dir/coverage_filtered.info" --output-directory "$output_dir/html" \
        --title "libmcpp 代码覆盖率报告" \
        --show-details --legend \
        --branch-coverage \
        --function-coverage \
        --ignore-errors mismatch,negative
    
    # 生成覆盖率汇总
    print_info "生成覆盖率汇总..."
    lcov --summary "$output_dir/coverage_filtered.info" \
        --rc lcov_branch_coverage=1 \
        --ignore-errors mismatch,negative > "$output_dir/coverage_summary.txt"
    
    # 显示覆盖率汇总
    print_info "覆盖率统计结果:"
    cat "$output_dir/coverage_summary.txt"
    
    print_info "覆盖率报告已生成到: $output_dir"
    print_info "HTML 报告位置: $output_dir/html/index.html"
    
    # 如果有浏览器，提示用户打开报告
    if command -v xdg-open >/dev/null 2>&1; then
        print_info "可以使用以下命令打开 HTML 报告:"
        echo "  xdg-open $output_dir/html/index.html"
    fi
}

# 主函数
main() {
    print_info "开始智能编译..."

    # 解析参数
    local enable_coverage=true
    local clean_build=false
    local coverage_output_dir="coverage"
    local fast_coverage=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --clean|clean)
                clean_build=true
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
            *)
                print_error "未知参数: $1"
                print_info "使用方法: $0 [--clean] [--coverage] [--coverage-output <目录>] [--fast-coverage]"
                exit 1
                ;;
        esac
    done

    # 自动检测并安装依赖工具
    if command -v apt-get >/dev/null 2>&1; then
        local basic_packages="libboost-all-dev libgtest-dev ninja-build gcc g++"
        local coverage_packages=""
        
        if [ "$enable_coverage" = true ]; then
            coverage_packages="lcov"
        fi
        
        for pkg in $basic_packages $coverage_packages; do
            print_info "检查 $pkg 是否安装..."
            dpkg -s $pkg >/dev/null 2>&1
            if [ $? -ne 0 ]; then
                print_warning "$pkg 未安装，正在自动安装..."
                apt-get update && apt-get install -y $pkg
                if [ $? -eq 0 ]; then
                    print_info "$pkg 安装完成"
                else
                    print_error "$pkg 安装失败，请手动安装后重试"
                    exit 1
                fi
            else
                print_info "$pkg 已安装"
            fi
        done
        
        # 如果启用覆盖率，检查 gcov 是否可用
        if [ "$enable_coverage" = true ]; then
            if ! command -v gcov >/dev/null 2>&1; then
                print_error "gcov 工具不可用，覆盖率统计将无法正常工作"
                print_info "gcov 通常包含在 gcc 中，请确保已安装 gcc"
                exit 1
            else
                print_info "gcov 工具可用"
            fi
        fi
        
        print_info "依赖安装完成"
    fi

    # 检查是否需要清理构建目录
    if [ "$clean_build" = true ]; then
        print_warning "检测到 clean 参数，正在删除构建目录 builddir..."
        rm -rf builddir
        print_info "构建目录 builddir 已删除"
        
        # 如果启用覆盖率，也清理覆盖率数据
        if [ "$enable_coverage" = true ]; then
            print_warning "清理覆盖率数据..."
            rm -rf "$coverage_output_dir"
            find . -name "*.gcda" -delete
            find . -name "*.gcno" -delete
        fi
    fi
    
    # 检查是否在项目根目录
    if [ ! -f "meson.build" ]; then
        print_error "请在项目根目录执行此脚本"
        exit 1
    fi
    
    # 获取系统信息
    local system_info=($(get_system_info))
    local cpu_cores=${system_info[0]}
    local total_mem_gb=${system_info[1]}
    local available_mem_gb=${system_info[2]}
    local swap_total_gb=${system_info[3]}
    
    # 显示系统信息
    print_info "系统信息: CPU核数=${cpu_cores}, 总内存=${total_mem_gb}GB, 可用内存=${available_mem_gb}GB, Swap=${swap_total_gb}GB, 覆盖率=${enable_coverage}"
    
    # 检查内存情况
    if [ $available_mem_gb -lt 4 ]; then
        print_error "可用内存不足4GB，建议释放内存后再编译"
        exit 1
    fi
    
    # 如果没有swap，给出警告
    if [ $swap_total_gb -eq 0 ]; then
        print_warning "系统没有配置swap分区，将使用更保守的并发策略"
    fi
    
    # 计算最优并发数
    local optimal_jobs=$(calculate_optimal_jobs $cpu_cores $total_mem_gb $available_mem_gb $swap_total_gb)
    
    # 显示计算过程（手动计算用于显示）
    local mem_based_jobs=$((available_mem_gb * 2 / 3 / 2))
    local cpu_based_jobs=$((cpu_cores / 2))
    if [ $swap_total_gb -eq 0 ]; then
        mem_based_jobs=$((mem_based_jobs * 2 / 3))
    fi
    print_info "计算结果: 基于内存=${mem_based_jobs}, 基于CPU=${cpu_based_jobs}, 最终选择=${optimal_jobs}"
    
    print_info "将使用 ${optimal_jobs} 个并发任务进行编译"
    
    # 设置构建目录
    local build_dir="builddir"
    
    # 配置构建选项
    local meson_options=""
    if [ "$enable_coverage" = true ]; then
        meson_options="-Denable_coverage=true"
        print_info "启用代码覆盖率统计"
    fi
    
    # 如果builddir不存在，先配置
    if [ ! -d "$build_dir" ]; then
        print_info "配置构建环境..."
        meson setup $build_dir $meson_options
    elif [ "$enable_coverage" = true ]; then
        # 如果启用覆盖率，重新配置构建
        print_info "重新配置构建环境以启用覆盖率..."
        meson configure $build_dir $meson_options
    fi
    
    # 开始编译
    print_info "开始编译..."
    local start_time=$(date +%s)
    
    # 使用计算出的最优并发数进行编译
    if meson compile -C $build_dir -j $optimal_jobs; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        print_info "编译成功完成! 用时: ${duration}秒"
        
        # 运行测试
        print_info "运行测试..."
        if meson test -C $build_dir; then
            print_info "测试运行完成"
            
            # 如果启用了覆盖率，生成覆盖率报告
            if [ "$enable_coverage" = true ]; then
                if [ "$fast_coverage" = true ]; then
                    print_info "使用快速覆盖率模式..."
                    # 调用 coverage.sh 的快速模式
                    ./scripts/coverage.sh --fast --output "$coverage_output_dir" --build-dir "$build_dir"
                else
                    generate_coverage_report "$build_dir" "$coverage_output_dir"
                fi
            fi
        else
            print_error "测试运行失败"
            exit 1
        fi
    else
        print_error "编译失败"
        exit 1
    fi
}

# 如果脚本被直接执行
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi 