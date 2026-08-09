/* ==========================================================
 * AI聊天助手 前端主逻辑 script.js
 * 功能:
 *   - 会话列表加载/切换/删除
 *   - 模型选择并创建新会话
 *   - 发送消息 + SSE 流式解析
 *   - Markdown 渲染 + 代码高亮 + 代码一键复制
 * ========================================================== */

(function () {
    'use strict';

    // ----------- 配置 -----------
    // 前后端同域部署，使用相对路径即可
    const API_BASE_URL = '';
    const MAX_INPUT_LENGTH = 2000;

    // ----------- 全局状态 -----------
    const state = {
        currentSessionId: null,
        currentModelName: null,
        sessions: [],
        models: [],
        selectedModelName: null,
        isStreaming: false,
        proxyAvailable: null,   // null=未检测, true=可用, false=不可用
        proxyChecking: false,
    };

    // ----------- DOM 缓存 -----------
    const $ = (id) => document.getElementById(id);
    const dom = {
        headerNewChatBtn: $('headerNewChatBtn'),
        welcomeNewChatBtn: $('welcomeNewChatBtn'),
        sessionList: $('sessionList'),
        sessionCount: $('sessionCount'),
        statusDot: $('statusDot'),
        statusText: $('statusText'),
        welcomeScreen: $('welcomeScreen'),
        chatInterface: $('chatInterface'),
        chatHeader: $('chatHeader'),
        currentModelName: $('currentModelName'),
        messagesContainer: $('messagesContainer'),
        messageInput: $('messageInput'),
        sendBtn: $('sendBtn'),
        charCount: $('charCount'),
        modelModal: $('modelModal'),
        modelGrid: $('modelGrid'),
        closeModalBtn: $('closeModalBtn'),
        cancelBtn: $('cancelBtn'),
        confirmBtn: $('confirmBtn'),
        toastContainer: $('toastContainer'),
    };

    // ----------- 工具函数 -----------

    /** HTML 转义，用于非 markdown 的纯文本显示 */
    function escapeHtml(str) {
        if (str === null || str === undefined) return '';
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    }

    /**
     * 格式化时间戳(秒级Unix时间)成友好字符串
     */
    function formatTimestamp(ts) {
        if (!ts || isNaN(Number(ts)) || Number(ts) <= 0) return '未知时间';
        const sec = Number(ts);
        // 后端返回秒级，Date 使用毫秒
        const date = new Date(sec * 1000);
        if (isNaN(date.getTime())) return '未知时间';

        const now = new Date();
        const diffMs = now - date;
        const min  = Math.floor(diffMs / 60000);
        const hr   = Math.floor(diffMs / 3600000);
        const day  = Math.floor(diffMs / 86400000);

        if (diffMs < 60000)       return '刚刚';
        if (min < 60)             return `${min}分钟前`;
        if (hr  < 24)             return `${hr}小时前`;
        if (day < 7)              return `${day}天前`;

        const pad = (n) => String(n).padStart(2, '0');
        return `${date.getFullYear()}-${pad(date.getMonth()+1)}-${pad(date.getDate())} ${pad(date.getHours())}:${pad(date.getMinutes())}`;
    }

    /** Toast 提示 */
    function toast(message, type = 'info') {
        const validTypes = ['success', 'error', 'warn', 'info'];
        const t = validTypes.includes(type) ? type : 'info';
        const icons = { success: 'fa-check-circle', error: 'fa-exclamation-circle',
                        warn: 'fa-exclamation-triangle', info: 'fa-info-circle' };

        const el = document.createElement('div');
        el.className = `toast toast-${t}`;
        el.innerHTML = `<i class="fas ${icons[t]}"></i><span>${escapeHtml(message)}</span>`;
        dom.toastContainer.appendChild(el);
        setTimeout(() => el.remove(), 2900);
    }

    /** 设置状态栏 */
    function setStatus(text, type = 'ok') {
        dom.statusText.textContent = text;
        const colors = { ok: '#10b981', warn: '#f59e0b', error: '#ef4444', loading: '#4f46e5' };
        dom.statusDot.style.background = colors[type] || colors.ok;
    }

    /** 自动缩放 textarea 高度 */
    function autoResizeTextarea() {
        const ta = dom.messageInput;
        ta.style.height = 'auto';
        ta.style.height = Math.min(ta.scrollHeight, 180) + 'px';
    }

    // ----------- Marked 配置 (Markdown 渲染) -----------
    function initMarked() {
        if (typeof marked === 'undefined') return;

        const renderer = new marked.Renderer();

        // 代码块：包装 wrapper，之后再插入复制按钮
        const origCode = renderer.code.bind(renderer);
        renderer.code = function (code, infostring, escaped) {
            const lang = (infostring || '').trim();
            // 让 marked 先按原逻辑输出代码块
            const html = origCode(code, infostring, escaped);
            // 外层包裹 .code-block-wrapper，便于插入复制按钮和定位
            return `<div class="code-block-wrapper" data-lang="${escapeHtml(lang)}">${html}</div>`;
        };

        marked.setOptions({
            renderer,
            gfm: true,
            tables: true,
            breaks: true,
            pedantic: false,
            sanitize: false,
            smartLists: true,
            smartypants: false,
            highlight: function (code, lang) {
                if (typeof hljs === 'undefined') return code;
                try {
                    if (lang && hljs.getLanguage(lang)) {
                        return hljs.highlight(code, { language: lang, ignoreIllegals: true }).value;
                    }
                    return hljs.highlightAuto(code).value;
                } catch (e) {
                    return code;
                }
            },
        });
    }

    /** 对容器里的 KaTeX 公式进行重新渲染（流式更新时需要） */
    function renderMathInContainer(container) {
        if (!container || typeof katex === 'undefined') return;

        // 排除代码块和预格式化文本
        const excludeSelector = 'pre, code, .code-block-wrapper, .katex, script, style';
        const elements = container.querySelectorAll(':scope *');
        
        elements.forEach(el => {
            // 跳过被排除的元素
            if (el.matches(excludeSelector)) return;
            if (el.closest(excludeSelector)) return;

            // 处理直接子文本节点
            const childNodes = Array.from(el.childNodes);
            childNodes.forEach(node => {
                if (node.nodeType !== Node.TEXT_NODE) return;
                const text = node.nodeValue;
                if (!text || !/\$[^$]+\$/.test(text)) return;

                // 替换块级公式 $$...$$
                let newText = text.replace(/\$\$([\s\S]+?)\$\$/g, (match, formula) => {
                    try {
                        const html = katex.renderToString(formula.trim(), { throwOnError: false, displayMode: true });
                        return html;
                    } catch (e) {
                        return match;
                    }
                });

                // 替换内联公式 $...$
                newText = newText.replace(/\$([^\$\n]+?)\$/g, (match, formula) => {
                    if (formula.trim().length === 0) return match;
                    try {
                        const html = katex.renderToString(formula.trim(), { throwOnError: false, displayMode: false });
                        return html;
                    } catch (e) {
                        return match;
                    }
                });

                if (newText !== text) {
                    const tempDiv = document.createElement('div');
                    tempDiv.innerHTML = newText;
                    const parent = node.parentNode;
                    if (parent) {
                        while (tempDiv.firstChild) {
                            parent.insertBefore(tempDiv.firstChild, node);
                        }
                        parent.removeChild(node);
                    }
                }
            });
        });
    }

    /** 对容器里所有 .code-block-wrapper 加复制按钮 + (可选)重新高亮 + 公式渲染 */
    function enhanceCodeBlocks(container) {
        if (!container) return;
        const wrappers = container.querySelectorAll('.code-block-wrapper');
        wrappers.forEach((wrapper) => {
            // 已经加过按钮就跳过
            if (wrapper.querySelector('.copy-code-btn')) return;

            const pre = wrapper.querySelector('pre');
            const code = wrapper.querySelector('code');
            if (!pre) return;

            // 再做一次高亮（流式增量渲染时可能之前没高亮）
            if (typeof hljs !== 'undefined' && code && !code.classList.contains('hljs')) {
                try {
                    const langAttr = wrapper.getAttribute('data-lang');
                    if (langAttr && hljs.getLanguage(langAttr)) {
                        hljs.highlightElement(code);
                    } else {
                        hljs.highlightElement(code);
                    }
                } catch (e) {}
            }

            const btn = document.createElement('button');
            btn.type = 'button';
            btn.className = 'copy-code-btn';
            btn.innerHTML = '<i class="fas fa-copy"></i> 复制';
            btn.addEventListener('click', (ev) => {
                ev.preventDefault();
                ev.stopPropagation();
                const text = code ? code.innerText : pre.innerText;
                copyTextToClipboard(text).then((ok) => {
                    if (ok) {
                        btn.classList.add('copied');
                        btn.innerHTML = '<i class="fas fa-check"></i> 已复制';
                        setTimeout(() => {
                            btn.classList.remove('copied');
                            btn.innerHTML = '<i class="fas fa-copy"></i> 复制';
                        }, 2000);
                    } else {
                        toast('复制失败，请手动选择代码复制', 'warn');
                    }
                });
            });
            wrapper.appendChild(btn);
        });

        // 同时渲染容器内的 LaTeX 数学公式
        renderMathInContainer(container);
    }

    /** 安全复制文本 */
    async function copyTextToClipboard(text) {
        if (navigator.clipboard && window.isSecureContext) {
            try { await navigator.clipboard.writeText(text); return true; } catch (_) {}
        }
        // 兼容回退：textarea + execCommand
        try {
            const ta = document.createElement('textarea');
            ta.value = text;
            ta.style.position = 'fixed';
            ta.style.top = '-1000px';
            ta.style.left = '-1000px';
            ta.setAttribute('readonly', '');
            document.body.appendChild(ta);
            ta.select();
            ta.setSelectionRange(0, text.length);
            const ok = document.execCommand('copy');
            document.body.removeChild(ta);
            return ok;
        } catch (_) {
            return false;
        }
    }

    // ----------- API 请求 -----------

    /** 统一 JSON 请求封装 */
    async function apiRequest(path, options = {}) {
        const url = API_BASE_URL + path;
        const opts = Object.assign({ credentials: 'same-origin' }, options);
        if (opts.body && typeof opts.body === 'object' && !(opts.body instanceof FormData)) {
            opts.headers = Object.assign({ 'Content-Type': 'application/json' }, opts.headers || {});
            opts.body = JSON.stringify(opts.body);
        }
        let resp;
        try {
            resp = await fetch(url, opts);
        } catch (e) {
            toast('网络错误，无法连接到服务器', 'error');
            setStatus('离线', 'error');
            throw e;
        }
        if (!resp.ok) {
            let msg = `HTTP ${resp.status}`;
            try {
                const j = await resp.json();
                if (j && typeof j.message === 'string') msg = j.message;
            } catch (_) {}
            throw new Error(msg);
        }
        return resp;
    }

    /** 获取会话列表 */
    async function loadSessions() {
        try {
            const resp = await apiRequest('/api/sessions');
            const json = await resp.json();
            if (!json.success) throw new Error(json.message || '获取会话列表失败');
            state.sessions = Array.isArray(json.data) ? json.data : [];
            // 按更新时间倒序
            state.sessions.sort((a, b) => (b.updated_at || 0) - (a.updated_at || 0));
            renderSessionList();
        } catch (e) {
            console.error('[loadSessions]', e);
            toast(e.message || '加载会话列表失败', 'error');
        }
    }

    /** 获取可用模型 */
    async function loadModels() {
        try {
            const resp = await apiRequest('/api/models');
            const json = await resp.json();
            if (!json.success) throw new Error(json.message || '获取模型列表失败');
            state.models = Array.isArray(json.data) ? json.data : [];
            // 如果有需要代理的模型，同时检测代理状态
            const needsProxyModels = state.models.filter(m => m.needs_proxy);
            if (needsProxyModels.length > 0 && state.proxyAvailable === null) {
                checkProxyAvailability();
            }
            renderModelGrid();
            return state.models;
        } catch (e) {
            console.error('[loadModels]', e);
            toast(e.message || '加载模型列表失败', 'error');
            state.models = [];
            renderModelGrid();
            return [];
        }
    }

    /** 检测代理可用性（缓存 30 秒） */
    async function checkProxyAvailability() {
        if (state.proxyChecking) return state.proxyAvailable;
        state.proxyChecking = true;
        try {
            const resp = await fetch(API_BASE_URL + '/api/proxy/check');
            if (resp.ok) {
                const json = await resp.json();
                state.proxyAvailable = !!json.proxy_available;
            } else {
                state.proxyAvailable = false;
            }
        } catch (e) {
            state.proxyAvailable = false;
        } finally {
            state.proxyChecking = false;
            // 重新渲染模型网格以更新代理状态标签
            if (state.models.length > 0) renderModelGrid();
        }
        return state.proxyAvailable;
    }

    /** 创建新会话 */
    async function createSession(modelName) {
        const resp = await apiRequest('/api/session', {
            method: 'POST',
            body: { model: modelName },
        });
        const json = await resp.json();
        if (!json.success) throw new Error(json.message || '创建会话失败');
        return json.data; // { session_id, model }
    }

    /** 删除会话 */
    async function deleteSessionRequest(sessionId) {
        const resp = await apiRequest(`/api/session/${encodeURIComponent(sessionId)}`, {
            method: 'DELETE',
        });
        const json = await resp.json();
        if (!json.success) throw new Error(json.message || '删除会话失败');
        return true;
    }

    /** 获取会话历史 */
    async function loadSessionHistory(sessionId) {
        const resp = await apiRequest(`/api/session/${encodeURIComponent(sessionId)}/history`);
        const json = await resp.json();
        if (!json.success) throw new Error(json.message || '获取会话历史失败');
        return Array.isArray(json.data) ? json.data : [];
    }

    /**
     * 发送消息并以 SSE 流式接收
     * @param {string} sessionId
     * @param {string} message
     * @param {(chunk:string, done:boolean)=>void} onChunk 增量回调
     */
    async function sendMessageStream(sessionId, message, onChunk) {
        const resp = await apiRequest('/api/message/async', {
            method: 'POST',
            body: { session_id: sessionId, message: message },
        });
        if (!resp.body) throw new Error('浏览器不支持流式读取');

        const reader = resp.body.getReader();
        const decoder = new TextDecoder('utf-8');
        let buffer = '';

        // SSE 解析：按事件流规则，空行分隔事件，每个 data: 行拼接后作为单次数据
        // 这里服务器返回的是：
        //   data: "文本"\n\n     (JSON 字符串，经 Json::valueToQuotedString 转义)
        //   data: [DONE]\n\n
        try {
            while (true) {
                const { value, done } = await reader.read();
                if (done) break;
                buffer += decoder.decode(value, { stream: true });

                // 逐个处理完整的 SSE 事件（空行分隔）
                let idx;
                while ((idx = buffer.indexOf('\n\n')) !== -1) {
                    const event = buffer.slice(0, idx);
                    buffer = buffer.slice(idx + 2);
                    processSseEvent(event, onChunk);
                }
            }
            // 缓冲区里可能还有最后的内容（没有以 \n\n 结尾）
            if (buffer.trim().length > 0) {
                processSseEvent(buffer, onChunk);
            }
        } finally {
            try { reader.releaseLock(); } catch (_) {}
        }
    }

    /** 处理单条 SSE 事件块 (可能包含多个 data: 行) */
    function processSseEvent(eventText, onChunk) {
        // 一个事件可能包含多个 data: 前缀行，它们要拼成一个字符串(换行分隔)
        const lines = eventText.split('\n');
        let dataLines = [];
        for (const rawLine of lines) {
            let line = rawLine;
            // 去掉可选的 BOM/前置空格
            if (line.charCodeAt(0) === 0xFEFF) line = line.slice(1);
            // SSE 规范注释行以 : 开头，忽略
            if (line.length === 0 || line.startsWith(':')) continue;
            // 按工程约定：比较前 5 个字符与 "data:"
            if (line.length < 5) continue;
            if (line.substring(0, 5) !== 'data:') continue;

            // data: 后面可能有一个可选空格
            let value = line.substring(5);
            if (value.startsWith(' ')) value = value.substring(1);
            dataLines.push(value);
        }
        if (dataLines.length === 0) return;

        const payload = dataLines.join('\n');

        if (payload === '[DONE]') {
            onChunk('', true);
            return;
        }

        // 去掉两端可能的空白
        const trimmed = payload.trim();
        if (trimmed.length === 0) {
            onChunk('', false);
            return;
        }

        // 数据是 JSON 字符串（服务端用 Json::valueToQuotedString 包裹了），做 JSON 解码来得到正文
        // 兼容两种情况：带引号字符串 或 纯文本
        let text = payload;
        if ((trimmed.startsWith('"') && trimmed.endsWith('"')) ||
            (trimmed.startsWith('{') || trimmed.startsWith('['))) {
            try {
                const parsed = JSON.parse(trimmed);
                if (typeof parsed === 'string') {
                    text = parsed;
                } else if (parsed && typeof parsed === 'object') {
                    // 某些实现返回对象，尝试取 content/text/message
                    text = parsed.content || parsed.text || parsed.message ||
                           (parsed.data && (parsed.data.content || parsed.data.text || parsed.data.message)) ||
                           trimmed;
                }
            } catch (_) {
                // 解码失败就使用原始文本
                text = payload;
            }
        }

        if (text) onChunk(text, false);
    }

    // ----------- 渲染 -----------

    /** 渲染会话列表 */
    function renderSessionList() {
        const list = dom.sessionList;
        dom.sessionCount.textContent = String(state.sessions.length);

        if (state.sessions.length === 0) {
            list.innerHTML = `
                <div class="empty-sessions">
                    <i class="fas fa-inbox"></i>
                    <p>暂无会话记录</p>
                    <p class="empty-hint">点击右上角"新建对话"开始</p>
                </div>`;
            return;
        }

        list.innerHTML = '';
        for (const s of state.sessions) {
            const item = document.createElement('div');
            item.className = 'session-item' + (state.currentSessionId === s.id ? ' active' : '');
            item.dataset.id = s.id;

            const firstMsg = s.first_user_message ? escapeHtml(s.first_user_message) : '<em style="opacity:.6">新对话</em>';
            const modelTag = escapeHtml(s.model || '');
            const timeStr = formatTimestamp(s.updated_at);

            item.innerHTML = `
                <div class="session-header">
                    <span class="session-time">${timeStr}</span>
                    <button type="button" class="session-delete-btn" data-act="delete" data-id="${escapeHtml(s.id)}" title="删除会话">
                        <i class="fas fa-ellipsis-h"></i>
                    </button>
                </div>
                <div class="session-first-message">${firstMsg}</div>
                <span class="session-model">${modelTag}</span>
            `;

            // 点击卡片切换会话
            item.addEventListener('click', (ev) => {
                if (ev.target.closest('[data-act="delete"]')) return;
                selectSession(s.id);
            });

            // 删除按钮
            const delBtn = item.querySelector('[data-act="delete"]');
            delBtn.addEventListener('click', (ev) => {
                ev.stopPropagation();
                ev.preventDefault();
                handleDeleteSession(s.id);
            });

            list.appendChild(item);
        }
    }

    /** 渲染模型选择网格（单选 + 描述） */
    function renderModelGrid() {
        const grid = dom.modelGrid;
        state.selectedModelName = null;
        dom.confirmBtn.disabled = true;

        if (state.models.length === 0) {
            grid.innerHTML = `
                <div class="no-models-hint">
                    <i class="fas fa-cubes"></i>
                    <p>暂无可使用的模型</p>
                    <p style="font-size:12px;opacity:.7;margin-top:4px;">请检查服务端配置或稍后重试</p>
                </div>`;
            return;
        }

        grid.innerHTML = '';
        state.models.forEach((m) => {
            const card = document.createElement('label');
            card.className = 'model-card';
            card.dataset.name = m.name;

            const nameHtml = escapeHtml(m.name || '');
            const descHtml = escapeHtml(m.desc || '（暂无描述）');

            // 代理状态标签
            let proxyBadge = '';
            if (m.needs_proxy) {
                if (state.proxyAvailable === null) {
                    proxyBadge = `<span class="proxy-badge proxy-checking"><i class="fas fa-circle-notch fa-spin"></i> 检测代理中...</span>`;
                } else if (state.proxyAvailable) {
                    proxyBadge = `<span class="proxy-badge proxy-ok"><i class="fas fa-shield-alt"></i> 代理可用</span>`;
                } else {
                    proxyBadge = `<span class="proxy-badge proxy-warn"><i class="fas fa-exclamation-triangle"></i> 需代理 · 未检测到</span>`;
                }
            }

            card.innerHTML = `
                <div class="model-radio-row">
                    <span class="model-radio" aria-hidden="true"></span>
                    <div class="model-info">
                        <div class="model-name">
                            ${nameHtml}
                            ${proxyBadge}
                        </div>
                        <div class="model-desc">${descHtml}</div>
                    </div>
                </div>
            `;

            card.addEventListener('click', (e) => {
                e.preventDefault();
                state.selectedModelName = m.name;
                grid.querySelectorAll('.model-card').forEach(c => c.classList.remove('selected'));
                card.classList.add('selected');
                dom.confirmBtn.disabled = false;
                // 如果选择了需要代理但代理不可用的模型，给出提示
                if (m.needs_proxy && state.proxyAvailable === false) {
                    toast('当前模型需要代理访问，请开启代理后使用', 'warn');
                }
            });

            grid.appendChild(card);
        });
    }

    /** 渲染单条消息并返回消息 id，返回 { elId, el } */
    function appendMessage(role, content, isStreaming = false, timestampSec = null, opts = {}) {
        const elId = 'msg-' + Date.now().toString(36) + '-' + Math.random().toString(36).slice(2, 8);
        const wrap = document.createElement('div');
        wrap.className = `message ${role}`;
        wrap.id = elId;

        const avatarIcon = role === 'user' ? 'fa-user' : 'fa-robot';
        const timeText = formatTimestamp(timestampSec || (Date.now() / 1000));
        let bodyHtml = '';
        if (isStreaming) {
            bodyHtml = `<div class="streaming-indicator"><span class="typing-dots"><span></span><span></span><span></span></span> 正在思考...</div>`;
        } else {
            if (typeof marked !== 'undefined') {
                try {
                    bodyHtml = marked.parse(content || '');
                } catch (_) {
                    bodyHtml = `<pre>${escapeHtml(content || '')}</pre>`;
                }
            } else {
                bodyHtml = `<pre style="white-space:pre-wrap">${escapeHtml(content || '')}</pre>`;
            }
        }

        wrap.innerHTML = `
            <div class="message-avatar"><i class="fas ${avatarIcon}"></i></div>
            <div class="message-main">
                <div class="message-bubble"><div class="message-content">${bodyHtml}</div></div>
                <div class="message-time">${timeText}</div>
            </div>
        `;

        (opts.mountTarget || dom.messagesContainer).appendChild(wrap);

        // 对非流式消息：增强代码块（复制按钮 + 高亮）
        if (!isStreaming && !opts.skipEnhance) {
            enhanceCodeBlocks(wrap.querySelector('.message-content'));
        }

        if (!opts.skipScroll) {
            scrollToBottom();
        }
        return { elId, el: wrap };
    }

    /** 更新流式消息内容 */
    function updateMessageContent(elId, accumulatedContent, isDone) {
        const wrap = document.getElementById(elId);
        if (!wrap) return;
        const contentEl = wrap.querySelector('.message-content');
        if (!contentEl) return;

        let html = '';
        if (!isDone && !accumulatedContent) {
            html = `<div class="streaming-indicator"><span class="typing-dots"><span></span><span></span><span></span></span> 正在思考...</div>`;
        } else {
            if (typeof marked !== 'undefined') {
                try {
                    html = marked.parse(accumulatedContent || '');
                } catch (_) {
                    html = `<pre style="white-space:pre-wrap">${escapeHtml(accumulatedContent || '')}</pre>`;
                }
            } else {
                html = `<pre style="white-space:pre-wrap">${escapeHtml(accumulatedContent || '')}</pre>`;
            }
        }
        contentEl.innerHTML = html;
        enhanceCodeBlocks(contentEl);
        scrollToBottom();
    }

    function scrollToBottom() {
        requestAnimationFrame(() => {
            dom.messagesContainer.scrollTop = dom.messagesContainer.scrollHeight;
        });
    }

    /** 渲染聊天历史 */
    function renderHistory(history) {
        dom.messagesContainer.innerHTML = '';
        if (!Array.isArray(history) || history.length === 0) return;

        // 性能优化：用 DocumentFragment 批量插入，避免逐条 appendChild 触发多次 reflow
        const fragment = document.createDocumentFragment();
        const msgEls = [];
        for (const m of history) {
            const role = String(m.role || 'assistant') === 'user' ? 'user' : 'assistant';
            const { el } = appendMessage(role, m.content || '', false, m.timestamp || null, {
                skipScroll: true,      // 批量渲染时不逐条滚动，最后统一滚一次
                skipEnhance: true,     // 批量渲染时跳过代码块增强，改为异步分批执行
                mountTarget: fragment, // 先挂到 fragment，最后一次性插入 DOM
            });
            msgEls.push(el);
        }
        // 一次性插入，只触发 1 次 reflow（而不是 N 次）
        dom.messagesContainer.appendChild(fragment);

        // 只滚动一次到底部
        scrollToBottom();

        // 异步分批增强代码块（复制按钮 + 语法高亮），避免阻塞首屏渲染
        scheduleEnhanceBatch(msgEls);
    }

    /** 分批异步增强代码块，每帧处理少量消息，避免长时间阻塞主线程 */
    function scheduleEnhanceBatch(msgEls) {
        if (!msgEls || msgEls.length === 0) return;
        let i = 0;
        const batchSize = 3;
        function step() {
            const end = Math.min(i + batchSize, msgEls.length);
            for (; i < end; i++) {
                const el = msgEls[i];
                if (el && el.isConnected) {
                    enhanceCodeBlocks(el.querySelector('.message-content'));
                }
            }
            if (i < msgEls.length) {
                requestAnimationFrame(step);
            }
        }
        requestAnimationFrame(step);
    }

    // ----------- 交互逻辑 -----------

    /** 显示模型选择弹窗 */
    async function openModelModal() {
        if (state.isStreaming) {
            toast('当前有消息正在生成中，请稍候', 'warn');
            return;
        }
        state.selectedModelName = null;
        dom.confirmBtn.disabled = true;
        dom.modelModal.style.display = 'flex';
        setStatus('加载模型列表...', 'loading');
        await loadModels();
        setStatus('就绪', 'ok');
    }

    function closeModelModal() {
        dom.modelModal.style.display = 'none';
    }

    /** 新建对话：弹窗选模型 -> 创建会话 -> 切聊天页 */
    async function handleConfirmCreate() {
        if (!state.selectedModelName) {
            toast('请选择一个模型', 'warn');
            return;
        }
        dom.confirmBtn.disabled = true;
        setStatus('正在创建会话...', 'loading');
        try {
            const data = await createSession(state.selectedModelName);
            state.currentSessionId = data.session_id;
            state.currentModelName = data.model || state.selectedModelName;
            dom.currentModelName.textContent = state.currentModelName;

            // 刷新会话列表
            await loadSessions();
            // 切换到聊天界面
            switchToChat();
            // 清空消息区
            dom.messagesContainer.innerHTML = '';
            toast('新会话创建成功', 'success');
            closeModelModal();
        } catch (e) {
            console.error('[createSession]', e);
            toast(e.message || '创建会话失败', 'error');
            dom.confirmBtn.disabled = false;
        } finally {
            setStatus('就绪', 'ok');
        }
    }

    /** 删除会话 */
    async function handleDeleteSession(sessionId) {
        if (state.isStreaming && state.currentSessionId === sessionId) {
            toast('当前会话正在生成中，请稍后再删除', 'warn');
            return;
        }
        const ok = window.confirm('确定要删除该会话吗？删除后不可恢复。');
        if (!ok) return;
        try {
            await deleteSessionRequest(sessionId);
            toast('会话已删除', 'success');
            // 如果删除的是当前会话，回欢迎页
            if (state.currentSessionId === sessionId) {
                state.currentSessionId = null;
                state.currentModelName = null;
                switchToWelcome();
            }
            await loadSessions();
        } catch (e) {
            toast(e.message || '删除会话失败', 'error');
        }
    }

    /** 切换到某个会话 */
    async function selectSession(sessionId) {
        if (state.isStreaming) {
            toast('当前有消息生成中，请稍后再切换会话', 'warn');
            return;
        }
        if (state.currentSessionId === sessionId) return;

        const info = state.sessions.find(s => s.id === sessionId);
        setStatus('加载会话历史...', 'loading');
        try {
            // 先切换界面并显示加载占位，让 UI 先响应，避免"卡顿感"
            state.currentSessionId = sessionId;
            state.currentModelName = info ? info.model : state.currentModelName;
            dom.currentModelName.textContent = state.currentModelName || '-';
            switchToChat();
            dom.messagesContainer.innerHTML = `
                <div class="history-loading">
                    <i class="fas fa-spinner fa-spin"></i>
                    <span>正在加载历史消息...</span>
                </div>`;
            renderSessionList();

            const history = await loadSessionHistory(sessionId);
            // 让"加载中"先渲染一帧，再渲染历史消息（避免主线程被同步渲染长时间阻塞）
            await new Promise(r => requestAnimationFrame(() => r()));
            renderHistory(history);
        } catch (e) {
            toast(e.message || '加载会话历史失败', 'error');
            dom.messagesContainer.innerHTML = '';
        } finally {
            setStatus('就绪', 'ok');
        }
    }

    function switchToChat() {
        dom.welcomeScreen.style.display = 'none';
        dom.chatInterface.style.display = 'flex';
        setTimeout(() => dom.messageInput.focus(), 30);
    }

    function switchToWelcome() {
        dom.welcomeScreen.style.display = 'flex';
        dom.chatInterface.style.display = 'none';
    }

    /** 发送当前输入 */
    async function handleSendMessage() {
        if (state.isStreaming) return;
        const raw = dom.messageInput.value;
        const text = (raw || '').replace(/\s+$/g, ''); // 去除末尾空白
        if (!text) {
            toast('请输入消息内容', 'warn');
            return;
        }
        if (!state.currentSessionId) {
            toast('请先创建或选择一个会话', 'warn');
            return;
        }

        // 代理检测：如果当前模型需要代理，检查代理是否可用
        const currentModel = state.models.find(m => m.name === state.currentModelName);
        if (currentModel && currentModel.needs_proxy && state.proxyAvailable === false) {
            toast('当前模型需要代理访问，请开启代理后重试', 'error');
            return;
        }
        // 如果尚未检测过代理状态，先快速检测一次
        if (currentModel && currentModel.needs_proxy && state.proxyAvailable === null && !state.proxyChecking) {
            const available = await checkProxyAvailability();
            if (!available) {
                toast('当前模型需要代理访问，请开启代理后重试', 'error');
                return;
            }
        }

        // 清空输入并重置
        dom.messageInput.value = '';
        updateCharCount();
        autoResizeTextarea();

        state.isStreaming = true;
        dom.sendBtn.disabled = true;
        setStatus('AI 正在响应...', 'loading');

        // 1) 先展示用户消息
        appendMessage('user', text, false, Math.floor(Date.now() / 1000));

        // 2) 创建 AI 消息占位（流式状态）
        const { elId } = appendMessage('assistant', '', true, null);
        let accumulated = '';

        try {
            await sendMessageStream(state.currentSessionId, text, (chunk, done) => {
                if (chunk) accumulated += chunk;
                updateMessageContent(elId, accumulated, done);
                if (done) {
                    // 完成后更新一下时间
                    const wrap = document.getElementById(elId);
                    if (wrap) {
                        const t = wrap.querySelector('.message-time');
                        if (t) t.textContent = formatTimestamp(Math.floor(Date.now() / 1000));
                    }
                }
            });
        } catch (e) {
            console.error('[sendMessage]', e);
            // 在流式消息位置显示错误
            accumulated = accumulated || '';
            const errMsg = `\n\n> ⚠️ 生成出错：${escapeHtml(e.message || '网络异常，请重试')}`;
            updateMessageContent(elId, accumulated + errMsg, true);
            toast(e.message || '消息发送失败', 'error');
        } finally {
            state.isStreaming = false;
            dom.sendBtn.disabled = false;
            // 发送完后刷新会话列表（更新时间/摘要/计数）
            loadSessions().catch(() => {});
            setStatus('就绪', 'ok');
            setTimeout(() => dom.messageInput.focus(), 30);
        }
    }

    /** 更新字数显示 */
    function updateCharCount() {
        const len = (dom.messageInput.value || '').length;
        dom.charCount.textContent = `${len}/${MAX_INPUT_LENGTH}`;
        dom.charCount.classList.remove('warning', 'danger');
        if (len > MAX_INPUT_LENGTH * 0.9)      dom.charCount.classList.add('danger');
        else if (len > MAX_INPUT_LENGTH * 0.75) dom.charCount.classList.add('warning');

        // 发送按钮可用性
        const canSend = len > 0 && !state.isStreaming && !!state.currentSessionId;
        dom.sendBtn.disabled = !canSend;
    }

    // ----------- 事件绑定 -----------
    function bindEvents() {
        // 新建对话按钮
        dom.headerNewChatBtn.addEventListener('click', openModelModal);
        dom.welcomeNewChatBtn.addEventListener('click', openModelModal);

        // 弹窗控制
        dom.closeModalBtn.addEventListener('click', closeModelModal);
        dom.cancelBtn.addEventListener('click', closeModelModal);
        dom.confirmBtn.addEventListener('click', handleConfirmCreate);

        // 点击遮罩关闭
        dom.modelModal.addEventListener('click', (e) => {
            if (e.target === dom.modelModal) closeModelModal();
        });
        // Esc 关闭
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && dom.modelModal.style.display === 'flex') {
                closeModelModal();
            }
        });

        // 输入区
        dom.messageInput.addEventListener('input', () => {
            updateCharCount();
            autoResizeTextarea();
        });
        // Enter 发送 / Shift+Enter 换行
        dom.messageInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey && !e.isComposing) {
                // 中文输入法未确认时不发送
                e.preventDefault();
                handleSendMessage();
            }
        });
        dom.sendBtn.addEventListener('click', handleSendMessage);
    }

    // ----------- 入口 -----------
    async function bootstrap() {
        initMarked();
        bindEvents();

        // 自动调整输入框
        autoResizeTextarea();
        updateCharCount();

        setStatus('连接服务器...', 'loading');
        // 初始加载会话列表 + 模型列表
        await Promise.all([loadSessions(), loadModels().catch(() => {})]);
        setStatus('就绪', 'ok');
    }

    // DOMContentLoaded
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', bootstrap);
    } else {
        bootstrap();
    }
})();
