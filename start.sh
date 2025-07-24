#!/bin/bash

# AI Text Assistant 测试启动脚本

echo "🧪 启动 AI Text Assistant 环境"
echo "=================================="

# 检查是否在正确的目录
if [ ! -d "web-nextchat" ] || [ ! -d "build" ]; then
    echo "❌ 错误: 请在 ai-test 目录下运行此脚本"
    exit 1
fi

# 检查后端程序
if [ ! -f "build/AITextAssistant" ]; then
    echo "❌ 错误: 后端程序不存在，请先编译"
    echo "   cd build && make"
    exit 1
fi

# 启动后端服务
echo "🔧 启动后端服务..."
cd build
./AITextAssistant --web &
BACKEND_PID=$!
cd ..

echo "   后端服务已启动 (PID: $BACKEND_PID)"

# 等待后端启动
echo "   等待后端服务启动..."
for i in {1..10}; do
    if curl -s http://localhost:8080/api/status > /dev/null 2>&1; then
        echo "   ✅ 后端服务启动成功"
        break
    fi
    if [ $i -eq 10 ]; then
        echo "   ❌ 后端服务启动超时"
        kill $BACKEND_PID 2>/dev/null
        exit 1
    fi
    sleep 1
done

# 启动前端服务
echo "🌐 启动前端服务..."
cd web-nextchat
python3 -m http.server 3001 &
FRONTEND_PID=$!
cd ..

echo "   前端服务已启动 (PID: $FRONTEND_PID)"

echo ""
echo "🎉 服务环境启动完成！"
echo ""
echo "📱 聊天页面:"
echo "   🎯 AI聊天界面: http://localhost:3001/enhanced-chat.html"
echo "   🧪 测试版界面: http://localhost:3001/test-chat.html"
echo ""
echo "🧪 测试指南:"
echo "   📖 查看测试步骤: cat test_web_interface.md"
echo "   🔧 运行API测试:  ./test_conversation_switching.sh"
echo ""
echo "💡 说明:"
echo "   1. 打开测试页面进行手动测试"
echo "   2. 测试对话切换和记忆功能"
echo "   3. 按 Ctrl+C 停止所有服务"
echo ""

# 清理函数
cleanup() {
    echo ""
    echo "🛑 正在停止服务..."
    
    if [ ! -z "$FRONTEND_PID" ]; then
        echo "   停止前端服务 (PID: $FRONTEND_PID)..."
        kill $FRONTEND_PID 2>/dev/null
    fi
    
    if [ ! -z "$BACKEND_PID" ]; then
        echo "   停止后端服务 (PID: $BACKEND_PID)..."
        kill -SIGINT $BACKEND_PID 2>/dev/null
        sleep 2
        if kill -0 $BACKEND_PID 2>/dev/null; then
            kill -SIGKILL $BACKEND_PID 2>/dev/null
        fi
    fi
    
    echo "✅ 所有服务已停止"
    exit 0
}

# 设置信号处理
trap cleanup SIGINT SIGTERM

# 等待用户中断
echo "⏳ 服务环境运行中... (按 Ctrl+C 停止)"
while true; do
    if ! kill -0 $FRONTEND_PID 2>/dev/null; then
        echo "❌ 前端服务意外停止"
        cleanup
        exit 1
    fi
    
    if ! kill -0 $BACKEND_PID 2>/dev/null; then
        echo "❌ 后端服务意外停止"
        cleanup
        exit 1
    fi
    
    sleep 2
done
