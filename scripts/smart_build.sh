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

# 主函数
main() {
    print_info "开始智能编译..."

    # 检查是否需要清理构建目录
    if [[ "$1" == "--clean" || "$1" == "clean" ]]; then
        print_warning "检测到 clean 参数，正在删除构建目录 builddir..."
        rm -rf builddir
        print_info "构建目录 builddir 已删除"
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
    print_info "系统信息: CPU核数=${cpu_cores}, 总内存=${total_mem_gb}GB, 可用内存=${available_mem_gb}GB, Swap=${swap_total_gb}GB"
    
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
    
    # 如果builddir不存在，先配置
    if [ ! -d "$build_dir" ]; then
        print_info "配置构建环境..."
        meson setup $build_dir
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
        meson test -C $build_dir
    else
        print_error "编译失败"
        exit 1
    fi
}

# 如果脚本被直接执行
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi 