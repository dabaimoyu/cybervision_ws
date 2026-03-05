#ifndef CYBERVISION_EDGE_GPU_MEMORY_MONITOR_HPP_
#define CYBERVISION_EDGE_GPU_MEMORY_MONITOR_HPP_

#include <cuda_runtime_api.h>
#include <iostream>
#include <string>
#include <vector>

namespace cybervision {

/**
 * @brief GPU 显存监控器
 * 
 * 功能需求:
 * - 显存预算管理 (8GB 刚性约束)
 * - 实时显存使用监控
 * - 显存泄漏检测
 */
class GPUMemoryMonitor {
public:
    GPUMemoryMonitor();
    
    /**
     * @brief 获取当前显存使用量 (MB)
     */
    size_t getUsedMemory() const;
    
    /**
     @brief 获取可用显存量 (MB)
     */
    size_t getFreeMemory() const;
    
    /**
     * @brief 获取总显存量 (MB)
     */
    size_t getTotalMemory() const;
    
    /**
     * @brief 打印显存状态报告
     */
    void printMemoryReport() const;
    
    /**
     * @brief 检查是否超出显存预算
     * @param budget_mb 显存预算上限 (MB)
     * @return true 如果未超出预算
     */
    bool checkBudget(size_t budget_mb) const;
    
    /**
     * @brief 记录显存分配用于泄漏检测
     */
    void logAllocation(const std::string& name, size_t size_mb);
    
    /**
     * @brief 重置显存基线（用于泄漏检测）
     */
    void resetBaseline();
    
    /**
     * @brief 检测显存泄漏
     * @return 疑似泄漏的显存量 (MB)
     */
    size_t detectLeak() const;

private:
    size_t baseline_used_;
    std::vector<std::pair<std::string, size_t>> allocations_;
    
    // 显存预算配置 (根据 RTX 4060 8GB)
    static constexpr size_t TOTAL_VRAM_MB = 8192;
    static constexpr size_t UBUNTU_DESKTOP_MB = 1536;
    static constexpr size_t INFER_NODE_BUDGET_MB = 3072;
    static constexpr size_t RENDER_NODE_BUDGET_MB = 2560;
    static constexpr size_t SAFETY_MARGIN_MB = 1024;
};

} // namespace cybervision

#endif // CYBERVISION_EDGE_GPU_MEMORY_MONITOR_HPP_
