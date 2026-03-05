#!/bin/bash

# YOLOv8 ONNX 转 TensorRT Engine 脚本
# 使用 trtexec 工具进行模型转换

# 参数配置
ONNX_MODEL="${1:-yolov8s.onnx}"  # 默认使用 yolov8s.onnx
ENGINE_FILE="${2:-yolov8s.engine}"
FP16_MODE="${3:-true}"  # 默认启用 FP16

echo "======================================"
echo "YOLOv8 ONNX -> TensorRT Engine 转换"
echo "======================================"
echo "输入 ONNX 模型：$ONNX_MODEL"
echo "输出 Engine 文件：$ENGINE_FILE"
echo "FP16 模式：$FP16_MODE"
echo ""

# 检查文件是否存在
if [ ! -f "$ONNX_MODEL" ]; then
    echo "[错误] 找不到 ONNX 模型文件：$ONNX_MODEL"
    exit 1
fi

# 构建 trtexec 命令
TRTEXEC_CMD="trtexec --onnx=$ONNX_MODEL \
    --saveEngine=$ENGINE_FILE \
    --minShapes=images:1x3x640x640 \
    --optShapes=images:4x3x640x640 \
    --maxShapes=images:16x3x640x640 \
    --workspace=4096"

# 如果启用 FP16
if [ "$FP16_MODE" = "true" ]; then
    TRTEXEC_CMD="$TRTEXEC_CMD --fp16"
    echo "[信息] 启用 FP16 量化模式"
else
    echo "[信息] 使用 FP32 模式"
fi

echo ""
echo "执行命令:"
echo "$TRTEXEC_CMD"
echo ""
echo "开始转换..."
echo ""

# 执行转换
eval $TRTEXEC_CMD

if [ $? -eq 0 ]; then
    echo ""
    echo "======================================"
    echo "✓ 转换成功!"
    echo "Engine 文件已保存到：$ENGINE_FILE"
    echo "======================================"
    
    # 显示文件大小
    ls -lh "$ENGINE_FILE"
else
    echo ""
    echo "======================================"
    echo "✗ 转换失败!"
    echo "======================================"
    exit 1
fi
