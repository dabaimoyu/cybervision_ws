#ifndef CYBERVISION_EDGE_OSG_RENDERER_HPP_
#define CYBERVISION_EDGE_OSG_RENDERER_HPP_

#include <osg/ref_ptr>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgGA/TrackballManipulator>
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/ShapeDrawable>
#include <osg/MatrixTransform>
#include <osgText/Text>
#include <vector>
#include <string>
#include <memory>
#include "cybervision_interfaces/msg/detected_object3_d.hpp"

namespace cybervision {

/**
 * @brief OSG 渲染引擎封装
 * 
 * 功能:
 * - 创建 3D 场景图
 * - 渲染检测到的 3D 目标
 * - 支持相机控制和漫游
 * - 双线程渲染（OSG 渲染线程）
 */
class OSGRenderer {
public:
    OSGRenderer();
    ~OSGRenderer();
    
    /**
     * @brief 初始化 OSG 场景
     */
    bool init();
    
    /**
     * @brief 更新场景中的目标
     * @param objects 检测到的 3D 目标数组
     * @param frame_id 帧 ID
     */
    void updateObjects(const std::vector<cybervision_interfaces::msg::DetectedObject3D>& objects, int64_t frame_id);
    
    /**
     * @brief 启动渲染循环
     */
    void startRendering();
    
    /**
     * @brief 停止渲染
     */
    void stopRendering();
    
    /**
     * @brief 获取 OSG Viewer
     */
    osgViewer::Viewer* getViewer() { return viewer_; }
    
private:
    /**
     * @brief 创建场景图根节点
     */
    osg::Group* createSceneGraph();
    
    /**
     * @brief 创建地面网格
     */
    osg::Node* createGroundPlane();
    
    /**
     * @brief 创建 3D 边界框表示车辆
     * @param x X 坐标
     * @param y Y 坐标
     * @param z Z 坐标
     * @param length 长度
     * @param width 宽度
     * @param height 高度
     * @param color 颜色
     */
    osg::Node* createBoundingBox(float x, float y, float z, 
                                  float length, float width, float height,
                                  const osg::Vec4& color);
    
    /**
     * @brief 添加边界框线条到顶点数组（批量渲染用）
     */
    void addBoundingBoxLines(osg::Vec3Array* vertices, float cx, float cy, float cz, 
                            float lx, float ly, float lz);
    
    /**
     * @brief 创建文本标签
     * @param text 文本内容
     * @param position 位置
     */
    osg::Node* createTextLabel(const std::string& text, const osg::Vec3& position);
    
    /**
     * @brief 根据物体类型获取颜色
     */
    osg::Vec4 getColorByType(const std::string& object_type);
    
    /**
     * @brief 清除旧的目标节点
     */
    void clearOldNodes();
    
    // OSG 核心组件
    osgViewer::Viewer* viewer_;              // OSG 查看器
    osg::ref_ptr<osg::Group> root_node_;     // 根节点
    osg::ref_ptr<osg::Group> objects_node_;  // 目标节点父节点
    
    // 场景元素
    std::vector<osg::ref_ptr<osg::Node>> object_nodes_;  // 当前帧的目标节点
    osg::ref_ptr<osg::Geometry> batch_geometry_;  // 复用的批量几何体
    osg::ref_ptr<osg::Vec3Array> vertex_array_;   // 复用的顶点数组
    osg::ref_ptr<osg::Vec4Array> color_array_;    // 复用的颜色数组
    
    // 状态
    bool initialized_;
    int64_t current_frame_id_;
};

} // namespace cybervision

#endif // CYBERVISION_EDGE_OSG_RENDERER_HPP_
