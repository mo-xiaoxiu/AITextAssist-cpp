class AIAssistantUI {
    constructor() {
        this.apiBase = '';
        this.isLoading = false;
        this.currentConversationId = null;
        
        this.initializeElements();
        this.bindEvents();
        this.checkStatus();
        this.loadConversations();
        
        // Auto-resize textarea
        this.setupTextareaAutoResize();
    }
    
    initializeElements() {
        this.elements = {
            statusDot: document.getElementById('status-dot'),
            statusText: document.getElementById('status-text'),
            chatMessages: document.getElementById('chat-messages'),
            messageInput: document.getElementById('message-input'),
            sendButton: document.getElementById('send-button'),
            charCount: document.getElementById('char-count'),
            conversationList: document.getElementById('conversation-list'),
            systemStatus: document.getElementById('system-status'),
            totalConversations: document.getElementById('total-conversations'),
            totalMessages: document.getElementById('total-messages'),
            newConversationBtn: document.getElementById('new-conversation'),
            clearChatBtn: document.getElementById('clear-chat'),
            loadingOverlay: document.getElementById('loading-overlay'),
            errorToast: document.getElementById('error-toast'),
            errorMessage: document.getElementById('error-message'),
            closeError: document.getElementById('close-error')
        };
    }
    
    bindEvents() {
        // Send message events
        this.elements.sendButton.addEventListener('click', () => this.sendMessage());
        this.elements.messageInput.addEventListener('keydown', (e) => {
            if (e.ctrlKey && e.key === 'Enter') {
                e.preventDefault();
                this.sendMessage();
            }
        });
        
        // Input events
        this.elements.messageInput.addEventListener('input', () => this.updateCharCount());
        this.elements.messageInput.addEventListener('input', () => this.updateSendButton());
        
        // Button events
        this.elements.newConversationBtn.addEventListener('click', () => this.newConversation());
        this.elements.clearChatBtn.addEventListener('click', () => this.clearChat());
        this.elements.closeError.addEventListener('click', () => this.hideError());
        
        // Auto-hide error toast
        setTimeout(() => this.hideError(), 5000);
    }
    
    setupTextareaAutoResize() {
        const textarea = this.elements.messageInput;
        textarea.addEventListener('input', () => {
            textarea.style.height = 'auto';
            textarea.style.height = Math.min(textarea.scrollHeight, 120) + 'px';
        });
    }
    
    updateCharCount() {
        const length = this.elements.messageInput.value.length;
        this.elements.charCount.textContent = `${length}/8000`;

        if (length > 7500) {
            this.elements.charCount.style.color = '#e74c3c';
        } else if (length > 6000) {
            this.elements.charCount.style.color = '#f39c12';
        } else {
            this.elements.charCount.style.color = '#7f8c8d';
        }
    }
    
    updateSendButton() {
        const hasText = this.elements.messageInput.value.trim().length > 0;
        this.elements.sendButton.disabled = !hasText || this.isLoading;
    }
    
    async sendMessage() {
        const message = this.elements.messageInput.value.trim();
        if (!message || this.isLoading) return;

        // Check message length
        if (message.length > 8000) {
            this.showError(`消息太长！当前 ${message.length} 字符，最大支持 8000 字符。请缩短消息后重试。`);
            return;
        }

        // Add user message to chat
        this.addMessage(message, 'user');

        // Clear input
        this.elements.messageInput.value = '';
        this.elements.messageInput.style.height = 'auto';
        this.updateCharCount();
        this.updateSendButton();

        // Show thinking message in chat instead of overlay
        const thinkingMessageId = this.addThinkingMessage();

        try {
            const response = await fetch('/api/chat', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ message })
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            const data = await response.json();

            if (data.error) {
                throw new Error(data.error);
            }

            // Remove thinking message
            this.removeThinkingMessage(thinkingMessageId);

            // Handle response (single or split)
            if (data.is_split) {
                // Add multiple messages for split response
                data.response_parts.forEach((part, index) => {
                    const isLast = index === data.response_parts.length - 1;
                    const prefix = index === 0 ? '' : `(续 ${index + 1}/${data.total_parts}) `;
                    this.addMessage(prefix + part, 'assistant');
                });
            } else {
                // Add single response
                this.addMessage(data.response, 'assistant');
            }

        } catch (error) {
            console.error('Error sending message:', error);
            this.removeThinkingMessage(thinkingMessageId);
            this.showError('发送消息失败: ' + error.message);
            this.addMessage('抱歉，我遇到了一些问题，请稍后再试。', 'assistant', true);
        }
    }
    
    addMessage(text, sender, isError = false) {
        const messageDiv = document.createElement('div');
        messageDiv.className = `message ${sender}-message`;

        const avatar = sender === 'user' ? '👤' : '🤖';
        const errorClass = isError ? ' error' : '';

        // Use markdown rendering for assistant messages, plain text for user messages
        const content = sender === 'assistant' && !isError ?
            this.renderMarkdown(text) :
            `<p>${this.escapeHtml(text)}</p>`;

        messageDiv.innerHTML = `
            <div class="message-content">
                <div class="message-avatar">${avatar}</div>
                <div class="message-text${errorClass}">
                    ${content}
                </div>
            </div>
        `;

        this.elements.chatMessages.appendChild(messageDiv);
        this.scrollToBottom();
        return messageDiv;
    }

    addThinkingMessage() {
        const messageDiv = document.createElement('div');
        messageDiv.className = 'message assistant-message thinking-message';
        messageDiv.id = 'thinking-' + Date.now();

        messageDiv.innerHTML = `
            <div class="message-content">
                <div class="message-avatar">🤖</div>
                <div class="message-text thinking">
                    <div class="thinking-dots">
                        <span></span>
                        <span></span>
                        <span></span>
                    </div>
                    <p>AI正在思考中...</p>
                </div>
            </div>
        `;

        this.elements.chatMessages.appendChild(messageDiv);
        this.scrollToBottom();
        return messageDiv.id;
    }

    removeThinkingMessage(messageId) {
        const messageElement = document.getElementById(messageId);
        if (messageElement) {
            messageElement.remove();
        }
    }
    
    scrollToBottom() {
        this.elements.chatMessages.scrollTop = this.elements.chatMessages.scrollHeight;
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    // Simple Markdown renderer
    renderMarkdown(text) {
        let html = this.escapeHtml(text);

        // Headers
        html = html.replace(/^### (.*$)/gm, '<h3>$1</h3>');
        html = html.replace(/^## (.*$)/gm, '<h2>$1</h2>');
        html = html.replace(/^# (.*$)/gm, '<h1>$1</h1>');

        // Bold and italic
        html = html.replace(/\*\*\*(.*?)\*\*\*/g, '<strong><em>$1</em></strong>');
        html = html.replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>');
        html = html.replace(/\*(.*?)\*/g, '<em>$1</em>');

        // Code blocks
        html = html.replace(/```([\s\S]*?)```/g, '<pre><code>$1</code></pre>');
        html = html.replace(/`([^`]+)`/g, '<code>$1</code>');

        // Lists
        html = html.replace(/^\* (.*$)/gm, '<li>$1</li>');
        html = html.replace(/^- (.*$)/gm, '<li>$1</li>');
        html = html.replace(/^\d+\. (.*$)/gm, '<li>$1</li>');

        // Wrap consecutive list items in ul/ol
        html = html.replace(/(<li>.*<\/li>)/gs, (match) => {
            return '<ul>' + match + '</ul>';
        });

        // Blockquotes
        html = html.replace(/^> (.*$)/gm, '<blockquote>$1</blockquote>');

        // Line breaks
        html = html.replace(/\n\n/g, '</p><p>');
        html = html.replace(/\n/g, '<br>');

        // Wrap in paragraphs if not already wrapped
        if (!html.includes('<h') && !html.includes('<ul>') && !html.includes('<pre>') && !html.includes('<blockquote>')) {
            html = '<p>' + html + '</p>';
        }

        return html;
    }
    
    showLoading() {
        this.isLoading = true;
        this.elements.loadingOverlay.classList.remove('hidden');
        this.updateSendButton();
    }
    
    hideLoading() {
        this.isLoading = false;
        this.elements.loadingOverlay.classList.add('hidden');
        this.updateSendButton();
    }
    
    showError(message) {
        this.elements.errorMessage.textContent = message;
        this.elements.errorToast.classList.remove('hidden');
        this.elements.errorToast.className = 'error-toast';

        // Auto-hide after 5 seconds
        setTimeout(() => this.hideError(), 5000);
    }

    showSuccess(message) {
        this.elements.errorMessage.textContent = message;
        this.elements.errorToast.classList.remove('hidden');
        this.elements.errorToast.className = 'error-toast success-toast';

        // Auto-hide after 3 seconds
        setTimeout(() => this.hideError(), 3000);
    }

    hideError() {
        this.elements.errorToast.classList.add('hidden');
    }
    
    async checkStatus() {
        try {
            const response = await fetch('/api/status');
            const data = await response.json();
            
            if (data.status === 'running') {
                this.updateStatus('connected', '已连接');
                this.elements.systemStatus.textContent = '运行中';
                this.elements.totalConversations.textContent = data.total_conversations || '0';
                this.elements.totalMessages.textContent = data.total_messages || '0';
            } else {
                this.updateStatus('error', '连接错误');
            }
        } catch (error) {
            console.error('Status check failed:', error);
            this.updateStatus('error', '连接失败');
        }
    }
    
    updateStatus(status, text) {
        this.elements.statusDot.className = `status-dot ${status}`;
        this.elements.statusText.textContent = text;
    }
    
    async loadConversations() {
        try {
            const response = await fetch('/api/conversations');
            const data = await response.json();
            
            if (data.conversations) {
                this.renderConversations(data.conversations);
            }
        } catch (error) {
            console.error('Failed to load conversations:', error);
        }
    }
    
    renderConversations(conversations) {
        const listElement = this.elements.conversationList;
        listElement.innerHTML = '';

        if (conversations.length === 0) {
            listElement.innerHTML = '<div class="conversation-item">暂无对话历史</div>';
            return;
        }

        conversations.forEach(conv => {
            const item = document.createElement('div');
            item.className = 'conversation-item';
            item.innerHTML = `
                <div class="conversation-content" onclick="window.aiAssistant.loadConversation('${conv.id}')">
                    <div class="conversation-title">${this.escapeHtml(conv.title || '未命名对话')}</div>
                    <div class="conversation-time">${this.formatTime(conv.created_at)}</div>
                </div>
                <button class="delete-conversation-btn" onclick="window.aiAssistant.deleteConversation('${conv.id}')" title="删除对话">
                    🗑️
                </button>
            `;

            listElement.appendChild(item);
        });
    }

    async deleteConversation(conversationId) {
        if (!confirm('确定要删除这个对话吗？此操作不可撤销。')) {
            return;
        }

        try {
            const response = await fetch('/api/conversations', {
                method: 'DELETE',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ conversation_id: conversationId })
            });

            const data = await response.json();

            if (!response.ok) {
                throw new Error(data.error || '删除失败');
            }

            // Show success message first
            this.showSuccess('对话已删除');

            // Immediately refresh conversations list and status
            await this.loadConversations();
            await this.checkStatus();

            // If the deleted conversation was the current one, clear the chat
            if (this.currentConversationId === conversationId) {
                this.clearChat();
                this.currentConversationId = null;
                this.addMessage('对话已删除，请开始新的对话。', 'assistant');
            }

        } catch (error) {
            console.error('Error deleting conversation:', error);
            this.showError('删除对话失败: ' + error.message);
        }
    }
    
    formatTime(timestamp) {
        const date = new Date(timestamp * 1000);
        const now = new Date();
        const diff = now - date;
        
        if (diff < 60000) return '刚刚';
        if (diff < 3600000) return Math.floor(diff / 60000) + '分钟前';
        if (diff < 86400000) return Math.floor(diff / 3600000) + '小时前';
        return Math.floor(diff / 86400000) + '天前';
    }
    
    async newConversation() {
        // Clear current chat
        this.clearChat();
        this.currentConversationId = null;
        
        // Add welcome message
        this.addMessage('你好！我是AI文本助手，有什么可以帮助你的吗？', 'assistant');
        
        // Refresh conversations list
        this.loadConversations();
    }
    
    clearChat() {
        this.elements.chatMessages.innerHTML = '';
    }
    
    async loadConversation(conversationId) {
        // This would require additional API endpoint to load conversation history
        console.log('Loading conversation:', conversationId);
        this.currentConversationId = conversationId;
    }

    // 计算字符串字节数
    getByteSize(str) {
        return new TextEncoder().encode(str).length;
    }

    // 根据字节数确定气泡大小类别
    getBubbleSizeClass(byteSize) {
        if (byteSize <= 100) return 'small';
        if (byteSize <= 500) return 'medium';
        if (byteSize <= 1500) return 'large';
        return 'xlarge';
    }

    // 应用气泡大小样式
    applyBubbleSize(messageElement, content) {
        const byteSize = this.getByteSize(content);
        const sizeClass = this.getBubbleSizeClass(byteSize);
        const messageContent = messageElement.querySelector('.message-content');
        messageContent.setAttribute('data-byte-size', sizeClass);
    }
}

// Initialize the UI when the page loads
document.addEventListener('DOMContentLoaded', () => {
    window.aiAssistant = new AIAssistantUI();
});
