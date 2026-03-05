#include "qt_gui.hpp"
#include <QScrollArea>
#include <iostream>

namespace cybervision {

QtGUIWindow::QtGUIWindow(QWidget* parent) 
    : QMainWindow(parent) {
    
    setWindowTitle("CyberVision Edge - 实时感知与三维数字孪生系统");
    resize(400, 600);
    
    createUI();
    
    // 初始化显示默认数据
    updatePerformanceData(0, 0.0, 0.0, 0, 0);
}

QtGUIWindow::~QtGUIWindow() {
}

void QtGUIWindow::createUI() {
    QWidget* central_widget = new QWidget(this);
    setCentralWidget(central_widget);
    
    QVBoxLayout* layout = new QVBoxLayout(central_widget);
    
    // 性能监控面板
    layout->addWidget(createPerformancePanel());
    
    // 状态栏
    layout->addWidget(createStatusBar());
    
    // 控制按钮
    layout->addWidget(createControlPanel());
    
    layout->addStretch();
}

QWidget* QtGUIWindow::createPerformancePanel() {
    QGroupBox* group = new QGroupBox("📊 性能监控");
    QVBoxLayout* layout = new QVBoxLayout();
    
    // FPS
    QHBoxLayout* fps_layout = new QHBoxLayout();
    fps_label_ = new QLabel("FPS: 0");
    fps_label_->setMinimumWidth(200);
    fps_progress_ = new QProgressBar();
    fps_progress_->setRange(0, 100);
    fps_progress_->setValue(0);
    fps_layout->addWidget(fps_label_);
    fps_layout->addWidget(fps_progress_);
    layout->addLayout(fps_layout);
    
    // 延迟
    QHBoxLayout* latency_layout = new QHBoxLayout();
    latency_label_ = new QLabel("延迟：0.00 ms");
    latency_label_->setMinimumWidth(200);
    latency_progress_ = new QProgressBar();
    latency_progress_->setRange(0, 100);
    latency_progress_->setValue(0);
    latency_layout->addWidget(latency_label_);
    latency_layout->addWidget(latency_progress_);
    layout->addLayout(latency_layout);
    
    // 推理耗时
    QHBoxLayout* inference_layout = new QHBoxLayout();
    inference_label_ = new QLabel("推理：0.00 ms");
    inference_layout->addWidget(inference_label_);
    layout->addLayout(inference_layout);
    
    // 目标数
    QHBoxLayout* object_layout = new QHBoxLayout();
    object_count_label_ = new QLabel("检测目标：0");
    object_layout->addWidget(object_count_label_);
    layout->addLayout(object_layout);
    
    // 帧 ID
    QHBoxLayout* frame_layout = new QHBoxLayout();
    frame_id_label_ = new QLabel("帧 ID: 0");
    frame_layout->addWidget(frame_id_label_);
    layout->addLayout(frame_layout);
    
    group->setLayout(layout);
    return group;
}

QWidget* QtGUIWindow::createStatusBar() {
    QGroupBox* group = new QGroupBox("ℹ️ 系统状态");
    QVBoxLayout* layout = new QVBoxLayout();
    
    QLabel* status_label = new QLabel("✅ 系统运行正常");
    layout->addWidget(status_label);
    
    QLabel* connection_label = new QLabel("🔗 DDS 连接：已建立");
    layout->addWidget(connection_label);
    
    group->setLayout(layout);
    return group;
}

QWidget* QtGUIWindow::createControlPanel() {
    QGroupBox* group = new QGroupBox("🎮 控制面板");
    QHBoxLayout* layout = new QHBoxLayout();
    
    exit_button_ = new QPushButton("退出系统");
    connect(exit_button_, &QPushButton::clicked, this, &QtGUIWindow::close);
    layout->addWidget(exit_button_);
    
    group->setLayout(layout);
    return group;
}

void QtGUIWindow::updatePerformanceData(int fps, double latency_ms, 
                                         double inference_time_ms,
                                         int object_count, int64_t frame_id) {
    // 调试日志
    static int update_count = 0;
    if (++update_count % 100 == 0) {
        std::cout << "[Qt GUI] 更新性能数据：FPS=" << fps << ", 目标数=" << object_count << std::endl;
    }
    
    // 更新标签
    fps_label_->setText(QString("FPS: %1").arg(fps));
    latency_label_->setText(QString("延迟：%1 ms").arg(latency_ms, 0, 'f', 2));
    inference_label_->setText(QString("推理：%1 ms").arg(inference_time_ms, 0, 'f', 2));
    object_count_label_->setText(QString("检测目标：%1").arg(object_count));
    frame_id_label_->setText(QString("帧 ID: %1").arg(frame_id));
    
    // 更新进度条
    fps_progress_->setValue(std::min(100, static_cast<int>(fps * 100 / 60)));
    latency_progress_->setValue(std::min(100, static_cast<int>(latency_ms * 100 / 60)));
    
    // 根据性能改变颜色
    if (fps >= 60) {
        fps_progress_->setStyleSheet("QProgressBar::chunk { background-color: #4CAF50; }");
    } else if (fps >= 30) {
        fps_progress_->setStyleSheet("QProgressBar::chunk { background-color: #FFC107; }");
    } else {
        fps_progress_->setStyleSheet("QProgressBar::chunk { background-color: #F44336; }");
    }
    
    if (latency_ms < 60) {
        latency_progress_->setStyleSheet("QProgressBar::chunk { background-color: #4CAF50; }");
    } else if (latency_ms < 100) {
        latency_progress_->setStyleSheet("QProgressBar::chunk { background-color: #FFC107; }");
    } else {
        latency_progress_->setStyleSheet("QProgressBar::chunk { background-color: #F44336; }");
    }
}

} // namespace cybervision
