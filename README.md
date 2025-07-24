# AI Text Assistant

一个基于C++的AI文本助手，支持历史会话管理和长久记忆功能。

## ✨ 功能特性

- 🤖 **智能对话**: 基于大语言模型的自然语言交互
- 💾 **历史会话**: 支持多个独立对话，可随时切换
- 🧠 **长久记忆**: AI在每个对话中保持完整的上下文记忆
- 🗑️ **会话管理**: 支持删除不需要的历史对话
- 🌐 **Web界面**: 现代化的响应式Web界面
- 📱 **跨平台**: 支持桌面和移动端访问

## 🚀 快速开始

### 1. 编译项目

```bash
mkdir build && cd build
cmake ..
make
```

### 2. 启动服务

```bash
# 启动完整服务（推荐）
./start.sh
```

### 3. 访问界面

- **AI聊天界面**: http://localhost:3001/enhanced-chat.html

## 📋 项目结构

```
ai-test/
├── src/                    # C++源代码
│   ├── core/              # 核心AI助手逻辑
│   ├── web/               # HTTP服务器
│   ├── database/          # 数据库管理
│   └── utils/             # 工具类
├── include/               # 头文件
├── web-nextchat/          # Web前端界面
├── config/                # 配置文件
├── build/                 # 编译输出目录
└── tests/                 # 测试代码
```

## 🔧 配置说明

主要配置文件：`config/default_config.json`

```json
{
  "llm": {
    "provider": "openai",
    "api_endpoint": "https://www.dmxapi.cn/v1/chat/completions",  //代理平台
    "api_key": "",												     //创建的令牌key
    "model_name": "o4-mini-2025-04-16"
  },
  "prompt": {
    "system_prompt": "You are a helpful AI text assistant...",
    "max_history_messages": 10
  }
}
```

## 📚 API文档

### 聊天接口

```http
POST /api/chat
Content-Type: application/json

{
    "message": "用户消息",
    "conversation_id": "可选的对话ID"
}
```

### 获取对话列表

```http
GET /api/conversations
```

### 获取对话消息

```http
GET /api/conversations/messages?conversation_id=<ID>
```

### 删除对话

```http
DELETE /api/conversations
Content-Type: application/json

{
    "conversation_id": "要删除的对话ID"
}
```

## 🛠️ 开发指南

### 依赖要求

- **C++17** 或更高版本
- **CMake** 3.10+
- **SQLite3**
- **libcurl**
- **nlohmann/json**
- **Python 3** (用于前端服务)

### 编译选项

```bash
# Debug模式
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release模式
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### 添加新功能

1. 在`src/`目录下添加源文件
2. 在`include/`目录下添加头文件
3. 更新`CMakeLists.txt`
4. 编写相应的测试

## 🔍 故障排除

### 常见问题

**Q: 编译失败**
- 检查依赖是否安装完整
- 确认C++编译器版本

**Q: 服务启动失败**
- 检查端口是否被占用
- 确认配置文件格式正确

**Q: 前端无法连接后端**
- 确认后端服务正在运行
- 检查防火墙设置

**Q: 删除对话失败**
- 检查浏览器控制台错误
- 确认CORS设置正确

## 📄 许可证

本项目采用MIT许可证。详见LICENSE文件。

## 🤝 贡献

欢迎提交Issue和Pull Request！

1. Fork本项目
2. 创建功能分支
3. 提交更改
4. 推送到分支
5. 创建Pull Request
