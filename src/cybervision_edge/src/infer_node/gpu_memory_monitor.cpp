#include "gpu_memory_monitor.hpp"
#include <cuda_runtime.h>
#include <iomanip>

namespace cybervision {

GPUMemoryMonitor::GPUMemoryMonitor() 
    : baseline_used_(0) {
    resetBaseline();
}

size_t GPUMemoryMonitor::getUsedMemory() const {
    cudaMemGetInfo(nullptr, nullptr);
    
    size_t free_mem, total_mem;
    cudaError_t err = cudaMemGetInfo(&free_mem, &total_mem);
    
    if (err != cudaSuccess) {
        std::cerr << "[ERROR] CUDA 显存查询失败：" << cudaGetErrorString(err) << std::endl;
        return 0;
    }
    
    return (total_mem - free_mem) / (1024 * 1024);  // 转换为 MB
}

size_t GPUMemoryMonitor::getFreeMemory() const {
    size_t free_mem, total_mem;
    cudaError_t err = cudaMemGetInfo(&free_mem, &total_mem);
    
    if (err != cudaSuccess) {
        std::cerr << "[ERROR] CUDA 显存查询失败：" << cudaGetErrorString(err) << std::endl;
        return 0;
    }
    
    return free_mem / (1024 * 1024);  // 转换为 MB
}

size_t GPUMemoryMonitor::getTotalMemory() const {
    size_t free_mem, total_mem;
    cudaError_t err = cudaMemGetInfo(&free_mem, &total_mem);
    
    if (err != cudaSuccess) {
        std::cerr << "[ERROR] CUDA 显存查询失败：" << cudaGetErrorString(err) << std::endl;
        return 0;
    }
    
    return total_mem / (1024 * 1024);  // 转换为 MB
}

void GPUMemoryMonitor::printMemoryReport() const {
    size_t used = getUsedMemory();
    size_t free = getFreeMemory();
    size_t total = getTotalMemory();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "       GPU 显存监控报告                 " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总显存：" << total << " MB" << std::endl;
    std::cout << "已使用：" << used << " MB (" 
              << std::fixed << std::setprecision(1) 
              << (used * 100.0 / total) << "%)" << std::endl;
    std::cout << "可用：  " << free << " MB (" 
              << (free * 100.0 / total) << "%)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 预算检查
    std::cout << "\n显存预算分配:" << std::endl;
    std::cout << "  Ubuntu 桌面：     " << UBUNTU_DESKTOP_MB << " MB" << std::endl;
    std::cout << "  AI 推理节点：     " << INFER_NODE_BUDGET_MB << " MB" << std::endl;
    std::cout << "  OSG 渲染节点：    " << RENDER_NODE_BUDGET_MB << " MB" << std::endl;
    std::cout << "  安全冗余：        " << SAFETY_MARGIN_MB << " MB" << std::endl;
    std::cout << "  ────────────────────────" << std::endl;
    std::cout << "  总计：            " << TOTAL_VRAM_MB << " MB" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 当前进程预算检查
    bool budget_ok = checkBudget(INFER_NODE_BUDGET_MB);
    
    if (budget_ok) {
        std::cout << "✅ 显存使用正常 (" << used << " MB <= " 
                  << INFER_NODE_BUDGET_MB << " MB)" << std::endl;
    } else {
        std::cout << "⚠️  警告：显存超出预算！(" << used << " MB > " 
                  << INFER_NODE_BUDGET_MB << " MB)" << std::endl;
    }
    
    // 泄漏检测
    size_t leak = detectLeak();
    if (leak > 0) {
        std::cout << "⚠️  疑似显存泄漏：" << leak << " MB" << std::endl;
    } else {
        std::cout << "✅ 未检测到显存泄漏" << std::endl;
    }
    
    std::cout << "========================================\n" << std::endl;
}

bool GPUMemoryMonitor::checkBudget(size_t budget_mb) const {
    return getUsedMemory() <= budget_mb;
}

void GPUMemoryMonitor::logAllocation(const std::string& name, size_t size_mb) {
    allocations_.push_back({name, size_mb});
    
    std::cout << "[MEM] 分配：" << name << " - " << size_mb << " MB" << std::endl;
}

void GPUMemoryMonitor::resetBaseline() {
    baseline_used_ = getUsedMemory();
    allocations_.clear();
    
    std::cout << "[MEM] 显存基线重置：" << baseline_used_ << " MB" << std::endl;
}

size_t GPUMemoryMonitor::detectLeak() const {
    size_t current_used = getUsedMemory();
    
    // 计算所有记录的分配总量
    size_t total_allocated = 0;
    for (const auto& alloc : allocations_) {
        total_allocated += alloc.second;
    }
    
    // 如果当前使用量远大于基线 + 分配总量，可能存在泄漏
    size_t expected_max = baseline_used_ + total_allocated;
    
    if (current_used > expected_max + 50) {  // 50MB 容差
        return current_used - expected_max;
    }
    
    return 0;
}

} // namespace cybervision
