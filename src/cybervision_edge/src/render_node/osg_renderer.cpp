#include "osg_renderer.hpp"
#include <iostream>
#include <algorithm>

namespace cybervision {

OSGRenderer::OSGRenderer() 
    : viewer_(nullptr)
    , initialized_(false)
    , current_frame_id_(0) {
}

OSGRenderer::~OSGRenderer() {
    stopRendering();
    
    // 释放复用的资源
    batch_geometry_ = nullptr;
    vertex_array_ = nullptr;
    color_array_ = nullptr;
}

bool OSGRenderer::init() {
    if (initialized_) {
        return true;
    }
    
    std::cout << "[OSG] 初始化 OpenSceneGraph 渲染引擎..." << std::endl;
    
    // 创建 Viewer
    viewer_ = new osgViewer::Viewer();
    
    // 设置场景图
    root_node_ = createSceneGraph();
    viewer_->setSceneData(root_node_);
    
    // 设置相机控制器（Trackball 轨迹球）
    viewer_->setCameraManipulator(new osgGA::TrackballManipulator());
    
    // 添加坐标系处理器（注释掉，因为可能不存在）
    // viewer_->addEventHandler(new osgViewer::CoordinateSystemNode());
    
    // 添加统计信息处理器（显示 FPS 等）
    viewer_->addEventHandler(new osgViewer::StatsHandler());
    
    // 设置视图端口
    viewer_->getCamera()->setViewport(0, 0, 1280, 720);
    
    // 设置窗口大小和位置（非全屏）
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits();
    traits->x = 100;                    // 窗口 x 位置
    traits->y = 100;                    // 窗口 y 位置
    traits->width = 1280;               // 窗口宽度
    traits->height = 720;               // 窗口高度
    traits->red = 8;                    // 红色通道位数
    traits->green = 8;                  // 绿色通道位数
    traits->blue = 8;                   // 蓝色通道位数
    traits->alpha = 8;                  // Alpha 通道位数
    traits->depth = 24;                 // 深度缓冲位数
    traits->windowDecoration = true;    // 显示窗口装饰（标题栏、边框等）
    traits->doubleBuffer = true;        // 双缓冲
    traits->sampleBuffers = 1;          // 多重采样缓冲区
    traits->samples = 4;                // 4x 抗锯齿
    
    osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits);
    viewer_->getCamera()->setGraphicsContext(gc);
    
    // 设置清除颜色（天空蓝）
    viewer_->getCamera()->setClearColor(osg::Vec4(0.5f, 0.7f, 0.9f, 1.0f));
    
    // 启用深度测试
    viewer_->getCamera()->setComputeNearFarMode(osg::Camera::DO_NOT_COMPUTE_NEAR_FAR);
    
    std::cout << "[OSG] ✅ 渲染引擎初始化完成!" << std::endl;
    std::cout << "[OSG] 场景元素:" << std::endl;
    std::cout << "  - 地面网格 (绿色)" << std::endl;
    std::cout << "  - 3D 边界框 (按类型着色)" << std::endl;
    std::cout << "  - 文本标签 (物体类型 + 置信度)" << std::endl;
    std::cout << "[OSG] 控制方式:" << std::endl;
    std::cout << "  - 左键旋转 | 右键缩放 | 中键平移" << std::endl;
    std::cout << "  - 's' 键显示统计信息" << std::endl;
    
    initialized_ = true;
    return true;
}

osg::Group* OSGRenderer::createSceneGraph() {
    osg::Group* root = new osg::Group();
    
    // 创建地面
    osg::Node* ground = createGroundPlane();
    if (ground) {
        root->addChild(ground);
    }
    
    // 创建目标节点容器
    objects_node_ = new osg::Group();
    root->addChild(objects_node_);
    
    return root;
}

osg::Node* OSGRenderer::createGroundPlane() {
    osg::Geometry* geometry = new osg::Geometry();
    
    // 创建地面顶点（100m x 100m）
    osg::Vec3Array* vertices = new osg::Vec3Array();
    float size = 50.0f;
    int divisions = 50;
    
    // 预分配内存，提升性能
    vertices->reserve((divisions + 1) * 4);
    
    for (int i = 0; i <= divisions; ++i) {
        float t = static_cast<float>(i) / divisions;
        float x = -size + 2 * size * t;
        
        // 横线
        vertices->push_back(osg::Vec3(-size, -size + 2 * size * t, 0.0f));
        vertices->push_back(osg::Vec3(size, -size + 2 * size * t, 0.0f));
        
        // 竖线
        vertices->push_back(osg::Vec3(-size + 2 * size * t, -size, 0.0f));
        vertices->push_back(osg::Vec3(-size + 2 * size * t, size, 0.0f));
    }
    
    geometry->setVertexArray(vertices);
    
    // 设置颜色（灰色网格）
    osg::Vec4Array* colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(0.3f, 0.3f, 0.3f, 1.0f));
    geometry->setColorArray(colors);
    geometry->setColorBinding(osg::Geometry::BIND_OVERALL);
    
    // 使用线条绘制
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, vertices->size()));
    
    // 创建优化的 StateSet
    osg::StateSet* stateset = geometry->getOrCreateStateSet();
    stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    stateset->setRenderingHint(osg::StateSet::OPAQUE_BIN);
    
    // 启用 VBO（OSG 会自动处理）
    // OSG 默认会对 Geometry 使用 VBO，无需显式设置
    
    osg::Geode* geode = new osg::Geode();
    geode->addDrawable(geometry);
    
    return geode;
}

void OSGRenderer::updateObjects(const std::vector<cybervision_interfaces::msg::DetectedObject3D>& objects, int64_t frame_id) {
    if (!initialized_) {
        return;
    }
    
    current_frame_id_ = frame_id;
    
    // === 批量渲染优化：复用 Geometry，只更新顶点数据 ===
    if (!batch_geometry_) {
        // 首次创建
        batch_geometry_ = new osg::Geometry();
        vertex_array_ = new osg::Vec3Array();
        color_array_ = new osg::Vec4Array();
        
        // 预分配最大内存（支持最多 200 个物体）
        vertex_array_->reserve(200 * 12 * 2);
        color_array_->reserve(200 * 12);
        
        batch_geometry_->setVertexArray(vertex_array_);
        batch_geometry_->setColorArray(color_array_);
        batch_geometry_->setColorBinding(osg::Geometry::BIND_OVERALL);
        batch_geometry_->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, 0));
        
        // 优化 StateSet
        osg::StateSet* stateset = batch_geometry_->getOrCreateStateSet();
        stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        stateset->setRenderingHint(osg::StateSet::OPAQUE_BIN);
        
        osg::Geode* batch_geode = new osg::Geode();
        batch_geode->addDrawable(batch_geometry_);
        objects_node_->addChild(batch_geode);
    }
    
    // 清空并重新填充顶点
    vertex_array_->clear();
    color_array_->clear();
    
    if (objects.empty()) {
        // 如果没有目标，设置绘制数量为 0
        osg::DrawArrays* draw = dynamic_cast<osg::DrawArrays*>(batch_geometry_->getPrimitiveSet(0));
        if (draw) {
            draw->setCount(0);
        }
        return;
    }
    
    // 预分配本帧所需内存
    size_t total_vertices = objects.size() * 12 * 2;
    vertex_array_->reserve(total_vertices);
    
    for (const auto& obj : objects) {
        osg::Vec4 color = getColorByType(obj.object_type);
        
        // 计算边界框顶点
        float lx = obj.length / 2.0f;
        float ly = obj.width / 2.0f;
        float lz = obj.height / 2.0f;
        
        // 添加 12 条边的顶点
        addBoundingBoxLines(vertex_array_, obj.x, obj.y, obj.z, lx, ly, lz);
        
        // 添加颜色（每个顶点一个颜色）
        for (int i = 0; i < 24; ++i) {
            color_array_->push_back(color);
        }
        
        // 跳过文本标签渲染（性能优化）
        // osgText 非常慢，每帧创建 70+ 个文本回答导致 FPS 暴跌
        // 暂时禁用文本，只显示边界框
    }
    
    // 更新绘制数量
    osg::DrawArrays* draw = dynamic_cast<osg::DrawArrays*>(batch_geometry_->getPrimitiveSet(0));
    if (draw) {
        draw->setCount(vertex_array_->size());
    }
    
    // 关键修复：必须每帧都 dirty，否则 GPU 不会更新
    vertex_array_->dirty();
    color_array_->dirty();
    
    std::cout << "\r[OSG 渲染] 帧：" << frame_id 
              << " | 目标数：" << objects.size() 
              << " | 顶点数：" << vertex_array_->size()
              << " | DrawCalls: 1"
              << std::flush;
}

osg::Node* OSGRenderer::createBoundingBox(float x, float y, float z,
                                           float length, float width, float height,
                                           const osg::Vec4& color) {
    // 使用 VBO 优化边界框渲染
    osg::Geometry* geometry = new osg::Geometry();
    
    // 创建立方体顶点（12 条边，每条边 2 个顶点）
    osg::Vec3Array* vertices = new osg::Vec3Array();
    float lx = length / 2.0f;
    float ly = width / 2.0f;
    float lz = height / 2.0f;
    
    // 底面 4 条边
    vertices->push_back(osg::Vec3(x - lx, y - ly, z));        vertices->push_back(osg::Vec3(x + lx, y - ly, z));
    vertices->push_back(osg::Vec3(x + lx, y - ly, z));        vertices->push_back(osg::Vec3(x + lx, y + ly, z));
    vertices->push_back(osg::Vec3(x + lx, y + ly, z));        vertices->push_back(osg::Vec3(x - lx, y + ly, z));
    vertices->push_back(osg::Vec3(x - lx, y + ly, z));        vertices->push_back(osg::Vec3(x - lx, y - ly, z));
    
    // 顶面 4 条边
    vertices->push_back(osg::Vec3(x - lx, y - ly, z + height)); vertices->push_back(osg::Vec3(x + lx, y - ly, z + height));
    vertices->push_back(osg::Vec3(x + lx, y - ly, z + height)); vertices->push_back(osg::Vec3(x + lx, y + ly, z + height));
    vertices->push_back(osg::Vec3(x + lx, y + ly, z + height)); vertices->push_back(osg::Vec3(x - lx, y + ly, z + height));
    vertices->push_back(osg::Vec3(x - lx, y + ly, z + height)); vertices->push_back(osg::Vec3(x - lx, y - ly, z + height));
    
    // 连接顶面和底面的 4 条边
    vertices->push_back(osg::Vec3(x - lx, y - ly, z));          vertices->push_back(osg::Vec3(x - lx, y - ly, z + height));
    vertices->push_back(osg::Vec3(x + lx, y - ly, z));          vertices->push_back(osg::Vec3(x + lx, y - ly, z + height));
    vertices->push_back(osg::Vec3(x + lx, y + ly, z));          vertices->push_back(osg::Vec3(x + lx, y + ly, z + height));
    vertices->push_back(osg::Vec3(x - lx, y + ly, z));          vertices->push_back(osg::Vec3(x - lx, y + ly, z + height));
    
    // 设置顶点数组
    geometry->setVertexArray(vertices);
    
    // 设置颜色（所有线条使用相同颜色）
    osg::Vec4Array* colors = new osg::Vec4Array();
    colors->push_back(color);
    geometry->setColorArray(colors);
    geometry->setColorBinding(osg::Geometry::BIND_OVERALL);
    
    // 使用 GL_LINES 绘制
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, vertices->size()));
    
    // 创建 StateSet 并启用 VBO
    osg::StateSet* stateset = geometry->getOrCreateStateSet();
    stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateset->setRenderingHint(osg::StateSet::OPAQUE_BIN);
    
    // 优化：禁用深度写入（可选，提升性能）
    // osg::Depth* depth = new osg::Depth();
    // depth->setWriteMask(false);
    // stateset->setAttributeAndModes(depth, osg::StateAttribute::ON);
    
    osg::Geode* geode = new osg::Geode();
    geode->addDrawable(geometry);
    
    return geode;
}

void OSGRenderer::addBoundingBoxLines(osg::Vec3Array* vertices, float cx, float cy, float cz, 
                                       float lx, float ly, float lz) {
    // 底面 4 条边
    vertices->push_back(osg::Vec3(cx - lx, cy - ly, cz));
    vertices->push_back(osg::Vec3(cx + lx, cy - ly, cz));
    
    vertices->push_back(osg::Vec3(cx + lx, cy - ly, cz));
    vertices->push_back(osg::Vec3(cx + lx, cy + ly, cz));
    
    vertices->push_back(osg::Vec3(cx + lx, cy + ly, cz));
    vertices->push_back(osg::Vec3(cx - lx, cy + ly, cz));
    
    vertices->push_back(osg::Vec3(cx - lx, cy + ly, cz));
    vertices->push_back(osg::Vec3(cx - lx, cy - ly, cz));
    
    // 顶面 4 条边
    vertices->push_back(osg::Vec3(cx - lx, cy - ly, cz + lz * 2));
    vertices->push_back(osg::Vec3(cx + lx, cy - ly, cz + lz * 2));
    
    vertices->push_back(osg::Vec3(cx + lx, cy - ly, cz + lz * 2));
    vertices->push_back(osg::Vec3(cx + lx, cy + ly, cz + lz * 2));
    
    vertices->push_back(osg::Vec3(cx + lx, cy + ly, cz + lz * 2));
    vertices->push_back(osg::Vec3(cx - lx, cy + ly, cz + lz * 2));
    
    vertices->push_back(osg::Vec3(cx - lx, cy + ly, cz + lz * 2));
    vertices->push_back(osg::Vec3(cx - lx, cy - ly, cz + lz * 2));
    
    // 连接顶面和底面的 4 条边
    vertices->push_back(osg::Vec3(cx - lx, cy - ly, cz));
    vertices->push_back(osg::Vec3(cx - lx, cy - ly, cz + lz * 2));
    
    vertices->push_back(osg::Vec3(cx + lx, cy - ly, cz));
    vertices->push_back(osg::Vec3(cx + lx, cy - ly, cz + lz * 2));
    
    vertices->push_back(osg::Vec3(cx + lx, cy + ly, cz));
    vertices->push_back(osg::Vec3(cx + lx, cy + ly, cz + lz * 2));
    
    vertices->push_back(osg::Vec3(cx - lx, cy + ly, cz));
    vertices->push_back(osg::Vec3(cx - lx, cy + ly, cz + lz * 2));
}

osg::Node* OSGRenderer::createTextLabel(const std::string& text, const osg::Vec3& position) {
    osgText::Text* text_node = new osgText::Text();
    text_node->setFont("fonts/arial.ttf");
    text_node->setText(text);
    text_node->setPosition(position);
    text_node->setCharacterSize(0.3f);
    text_node->setColor(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    text_node->setAlignment(osgText::Text::CENTER_BOTTOM);
    text_node->setAutoRotateToScreen(true);
    
    osg::Geode* geode = new osg::Geode();
    geode->addDrawable(text_node);
    
    return geode;
}

osg::Vec4 OSGRenderer::getColorByType(const std::string& object_type) {
    // 根据物体类型返回不同颜色
    if (object_type == "car" || object_type == "vehicle") {
        return osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f);  // 红色
    } else if (object_type == "pedestrian" || object_type == "person") {
        return osg::Vec4(0.0f, 1.0f, 0.0f, 1.0f);  // 绿色
    } else if (object_type == "truck" || object_type == "bus") {
        return osg::Vec4(0.0f, 0.0f, 1.0f, 1.0f);  // 蓝色
    } else if (object_type == "bicycle") {
        return osg::Vec4(1.0f, 1.0f, 0.0f, 1.0f);  // 黄色
    } else {
        return osg::Vec4(1.0f, 0.5f, 0.0f, 1.0f);  // 橙色（其他）
    }
}

void OSGRenderer::clearOldNodes() {
    // 简单移除所有子节点（OSG 会自动释放内存）
    objects_node_->removeChildren(0, objects_node_->getNumChildren());
    object_nodes_.clear();
}

void OSGRenderer::startRendering() {
    if (!initialized_ || !viewer_) {
        return;
    }
    
    std::cout << "[OSG] 启动渲染循环..." << std::endl;
    
    // 设置运行参数
    viewer_->setThreadingModel(osgViewer::Viewer::SingleThreaded);
    
    // 运行渲染循环（会阻塞直到窗口关闭）
    viewer_->run();
}

void OSGRenderer::stopRendering() {
    if (viewer_) {
        viewer_->done();
        delete viewer_;
        viewer_ = nullptr;
    }
}

} // namespace cybervision
