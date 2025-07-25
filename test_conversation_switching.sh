#!/bin/bash

# 测试对话切换和长久记忆功能

API_BASE="http://localhost:8080"

echo "🧪 测试对话切换和长久记忆功能"
echo "=================================="

# 测试1: 创建第一个对话
echo ""
echo "📝 测试1: 创建第一个对话"
echo "发送消息: 你好，我是李四"
RESPONSE1=$(curl -s -X POST "$API_BASE/api/chat" \
    -H "Content-Type: application/json" \
    -d '{"message": "你好，我是李四"}')

echo "响应: $RESPONSE1"
CONV_ID1=$(echo $RESPONSE1 | grep -o '"conversation_id":"[^"]*"' | cut -d'"' -f4)
echo "对话ID1: $CONV_ID1"

# 测试2: 在第一个对话中继续
echo ""
echo "📝 测试2: 在第一个对话中继续"
echo "发送消息: 请记住我喜欢编程"
RESPONSE2=$(curl -s -X POST "$API_BASE/api/chat" \
    -H "Content-Type: application/json" \
    -d "{\"message\": \"请记住我喜欢编程\", \"conversation_id\": \"$CONV_ID1\"}")

echo "响应: $RESPONSE2"

# 测试3: 创建第二个对话
echo ""
echo "📝 测试3: 创建第二个对话"
echo "发送消息: 你好，我是王五"
RESPONSE3=$(curl -s -X POST "$API_BASE/api/chat" \
    -H "Content-Type: application/json" \
    -d '{"message": "你好，我是王五"}')

echo "响应: $RESPONSE3"
CONV_ID2=$(echo $RESPONSE3 | grep -o '"conversation_id":"[^"]*"' | cut -d'"' -f4)
echo "对话ID2: $CONV_ID2"

# 测试4: 在第二个对话中继续
echo ""
echo "📝 测试4: 在第二个对话中继续"
echo "发送消息: 请记住我喜欢音乐"
RESPONSE4=$(curl -s -X POST "$API_BASE/api/chat" \
    -H "Content-Type: application/json" \
    -d "{\"message\": \"请记住我喜欢音乐\", \"conversation_id\": \"$CONV_ID2\"}")

echo "响应: $RESPONSE4"

# 测试5: 切换回第一个对话，测试记忆
echo ""
echo "📝 测试5: 切换回第一个对话，测试记忆"
echo "发送消息: 我的名字是什么？我喜欢什么？"
RESPONSE5=$(curl -s -X POST "$API_BASE/api/chat" \
    -H "Content-Type: application/json" \
    -d "{\"message\": \"我的名字是什么？我喜欢什么？\", \"conversation_id\": \"$CONV_ID1\"}")

echo "响应: $RESPONSE5"

# 测试6: 切换到第二个对话，测试记忆
echo ""
echo "📝 测试6: 切换到第二个对话，测试记忆"
echo "发送消息: 我的名字是什么？我喜欢什么？"
RESPONSE6=$(curl -s -X POST "$API_BASE/api/chat" \
    -H "Content-Type: application/json" \
    -d "{\"message\": \"我的名字是什么？我喜欢什么？\", \"conversation_id\": \"$CONV_ID2\"}")

echo "响应: $RESPONSE6"

# 测试7: 获取对话列表
echo ""
echo "📝 测试7: 获取对话列表"
CONVERSATIONS=$(curl -s "$API_BASE/api/conversations")
echo "对话列表: $CONVERSATIONS"

# 测试8: 获取第一个对话的消息历史
echo ""
echo "📝 测试8: 获取第一个对话的消息历史"
MESSAGES1=$(curl -s "$API_BASE/api/conversations/messages?conversation_id=$CONV_ID1")
echo "对话1消息历史: $MESSAGES1"

# 测试9: 获取第二个对话的消息历史
echo ""
echo "📝 测试9: 获取第二个对话的消息历史"
MESSAGES2=$(curl -s "$API_BASE/api/conversations/messages?conversation_id=$CONV_ID2")
echo "对话2消息历史: $MESSAGES2"

echo ""
echo "✅ 测试完成！"
echo ""
echo "🔍 验证要点："
echo "1. 每个对话应该有独立的记忆"
echo "2. 对话1应该记住'李四'和'编程'"
echo "3. 对话2应该记住'王五'和'音乐'"
echo "4. 切换对话后应该能继续之前的上下文"
echo "5. 消息历史应该正确保存和加载"
