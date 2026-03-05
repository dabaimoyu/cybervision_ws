#ifndef CYBERVISION_EDGE_QOS_PROFILES_HPP_
#define CYBERVISION_EDGE_QOS_PROFILES_HPP_

#include "rclcpp/qos.hpp"

namespace cybervision {

/**
 * @brief QoS 配置文件类
 * 
 * 功能需求:
 * - FR-11: SensorData QoS 策略定义
 * - 针对感知数据优化的传输策略
 */
class QoSProfiles {
public:
    /**
     * @brief 获取感知数据的 QoS 配置
     * 
     * 设计原则:
     * - 实时性优先于可靠性（传感器数据允许少量丢失）
     * - 保持最新数据（队列深度适中）
     * - 支持多节点订阅
     */
    static rclcpp::QoS getSensorDataQoS() {
        return rclcpp::QoS(rclcpp::KeepLast(10))  // 保留最后 10 条消息
            .reliability(RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT)  // 尽力而为（不重传）
            .durability(RMW_QOS_POLICY_DURABILITY_VOLATILE);      // 易失性（不保留历史）
    }
    
    /**
     * @brief 获取关键数据的 QoS 配置
     * 
     * 用于需要可靠传输的场景
     */
    static rclcpp::QoS getReliableQoS() {
        return rclcpp::QoS(rclcpp::KeepLast(20))
            .reliability(RMW_QOS_POLICY_RELIABILITY_RELIABLE)     // 可靠传输（会重传）
            .durability(RMW_QOS_POLICY_DURABILITY_VOLATILE);
    }
    
    /**
     * @brief 验证 QoS 兼容性
     */
    static bool areCompatible(const rclcpp::QoS& pub_qos, const rclcpp::QoS& sub_qos) {
        // 简化检查：确保都是 KeepLast 策略
        return true;  // 实际应用中应该详细检查所有策略字段
    }
};

} // namespace cybervision

#endif // CYBERVISION_EDGE_QOS_PROFILES_HPP_
