#ifndef CYBERVISION_EDGE_QT_GUI_HPP_
#define CYBERVISION_EDGE_QT_GUI_HPP_

#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QGroupBox>
#include <QTimer>
#include <string>

namespace cybervision {

/**
 * @brief Qt GUI 主窗口
 * 
 * 功能:
 * - 显示性能监控面板
 * - OSG 渲染窗口嵌入
 * - 控制按钮和状态显示
 * - 双线程渲染（Qt GUI 线程）
 */
class QtGUIWindow : public QMainWindow {
public:
    QtGUIWindow(QWidget* parent = nullptr);
    ~QtGUIWindow();
    
    /**
     * @brief 更新性能数据显示
     */
    void updatePerformanceData(int fps, double latency_ms, double inference_time_ms, 
                               int object_count, int64_t frame_id);
    
private:
    /**
     * @brief 创建 UI 组件
     */
    void createUI();
    
    /**
     * @brief 创建性能监控面板
     */
    QWidget* createPerformancePanel();
    
    /**
     * @brief 创建状态栏
     */
    QWidget* createStatusBar();
    
    /**
     * @brief 创建控制按钮区域
     */
    QWidget* createControlPanel();
    
    // UI 组件
    QLabel* fps_label_;           // FPS 显示
    QLabel* latency_label_;       // 延迟显示
    QLabel* inference_label_;     // 推理耗时显示
    QLabel* object_count_label_;  // 目标数显示
    QLabel* frame_id_label_;      // 帧 ID 显示
    
    QProgressBar* fps_progress_;      // FPS 进度条
    QProgressBar* latency_progress_;  // 延迟进度条
    
    QPushButton* exit_button_;        // 退出按钮
};

} // namespace cybervision

#endif // CYBERVISION_EDGE_QT_GUI_HPP_
