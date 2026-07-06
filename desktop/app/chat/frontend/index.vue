<template>
  <div class="chat-page">
    <header class="chat-header">
      <h1>聊天</h1>
      <span class="chat-type-badge" :class="chatType">{{ chatTypeLabel }}</span>
      <span v-if="USER_NAME !== '用户'" class="chat-user">{{ USER_AVATAR }} {{ USER_NAME }}</span>
      <span class="chat-status" :class="statusClass">{{ statusText }}</span>
      <button class="agent-toggle" @click="showAgents = !showAgents" title="Agent列表">
        🤖 {{ agents.length }}
      </button>
    </header>
    <main class="chat-main" ref="msgBox">
      <transition name="fade">
        <div v-if="showAgents" class="agent-panel">
          <div class="agent-panel-header">房间Agent ({{ agents.length }})</div>
          <div v-for="(a, i) in agents" :key="i" class="agent-item">
            <span class="agent-item-avatar">{{ fixAvatar(a.avatar) }}</span>
            <span class="agent-item-name">{{ a.name }}</span>
            <button class="agent-item-rm" @click="removeAgent(a.name)">×</button>
          </div>
          <div class="agent-panel-add">
            <input v-model="newAgentName" class="agent-add-input" placeholder="Agent名称..."
              @keydown.enter="addAgent" />
            <button class="agent-add-btn" @click="addAgent">+</button>
          </div>
        </div>
      </transition>
      <div
        v-for="(m, i) in messages"
        :key="i"
        :class="['msg', m.isSelf ? 'msg-self' : 'msg-other']"
      >
        <div class="msg-wrapper" :class="m.isSelf ? 'wrapper-self' : 'wrapper-other'">
        <div v-if="m.senderAvatar" class="msg-avatar" :class="m.isSelf ? 'avatar-self' : 'avatar-other'">
          <img v-if="isImageUrl(m.senderAvatar)" :src="m.senderAvatar" class="avatar-img" />
          <span v-else>{{ m.senderAvatar }}</span>
        </div>
        <div class="msg-content-col">
        <div v-if="m.sender" class="msg-name">{{ m.sender }}</div>
        <div v-if="m.type === 'embed'" class="bubble bubble-embed" :class="'bubble-'+m.kind">
          <div v-if="m.kind === 'image'" class="embed-body">
            <img :src="m.url" :alt="m.title" class="embed-img" @click="previewImg(m.url)" />
            <div v-if="m.title" class="embed-title">{{ m.title }}</div>
          </div>
          <div v-else-if="m.kind === 'video'" class="embed-body">
            <video :src="m.url" controls class="embed-video"></video>
            <div v-if="m.title" class="embed-title">{{ m.title }}</div>
          </div>
          <div v-else-if="m.kind === 'audio'" class="embed-body">
            <div class="audio-label">{{ m.title || '音频' }}</div>
            <audio :src="m.url" controls class="embed-audio"></audio>
          </div>
          <div v-else-if="m.kind === 'game'" class="embed-body embed-game">
            <div class="game-icon">🎮</div>
            <div class="game-info">
              <div class="game-name">{{ m.name }}</div>
              <a @click.prevent="$router.push(m.url)" class="game-link" href>开始游戏</a>
            </div>
          </div>
          <div v-else class="embed-body">
            {{ m.text }}
          </div>
        </div>
        <div v-else class="bubble">
          <details v-if="m.reasoning" class="reasoning-block" :open="false">
            <summary class="reasoning-summary">💭 思考过程</summary>
            <div class="reasoning-content">{{ m.reasoning }}</div>
          </details>
          <div v-html="renderMarkdown(m.text)"></div>
        </div>
        </div>
        </div>
      </div>
    </main>
    <footer class="chat-footer">
      <div class="chat-input-wrap">
        <div v-if="showMentions && filteredAgents.length" class="mention-dropdown">
          <div v-for="a in filteredAgents" :key="a.name" class="mention-item"
            @mousedown.prevent @click="selectMention(a.name)">
            <span class="mention-avatar">{{ a.avatar }}</span>
            <span class="mention-name">{{ a.name }}</span>
          </div>
        </div>
        <textarea
          v-model="input"
          class="chat-input"
          placeholder="输入消息... @agent名可定向发送"
          @keydown="onInputKeydown"
          :disabled="!connected"
        ></textarea>
      </div>
      <button class="chat-send" @click="send" :disabled="!connected || !input.trim()">发送</button>
      <button v-if="isStreaming" class="chat-stop" @click="stopStream" title="停止生成">
        <svg viewBox="0 0 24 24" width="14" height="14"><rect x="4" y="4" width="16" height="16" rx="2" fill="currentColor"/></svg>
      </button>
    </footer>
  </div>
</template>

<script setup>
import { ref, computed, nextTick, onBeforeUnmount, onMounted } from 'vue'
import { marked } from 'marked'
import mermaid from 'mermaid'
import { createChannel } from '../../../src/services/channel.js'
import { getWsUrl } from '../../../src/services/config.js'

mermaid.initialize({
  startOnLoad: false,
  theme: 'default',
  fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif',
  securityLevel: 'loose',
})

const WS_URL = getWsUrl('/chat')
const WID = new URLSearchParams(location.search).get('wid') || ''
const DNAME = decodeURIComponent(new URLSearchParams(location.search).get('name') || '')
const DAVATAR = decodeURIComponent(new URLSearchParams(location.search).get('avatar') || '')
const USER_NAME = DNAME || '用户'
const USER_AVATAR = DAVATAR || '👤'

const input = ref('')
const messages = ref([])
const agents = ref([])
const showAgents = ref(false)
const newAgentName = ref('')
const showMentions = ref(false)
const mentionFilter = ref('')
const connected = ref(false)
const streamingIdx = ref(-1)
let pollTimer = null
let deltaBuffer = ''
let reasoningBuffer = ''
let rafPending = false

const statusText = computed(() => connected.value ? '已连接' : '未连接')
const statusClass = computed(() => connected.value ? 'status-ok' : 'status-err')
const isStreaming = computed(() => streamingIdx.value >= 0)
const chatType = computed(() => isGroupChat.value ? 'group' : 'private')
const chatTypeLabel = computed(() => isGroupChat.value ? '群聊' : '私聊')
const isGroupChat = ref(true)

const msgBox = ref(null)

const ch = createChannel(WS_URL, { maxRetries: -1, retryDelay: 3000, retryBackoff: 1 })

ch.onOpen(() => {
    connected.value = true
    const p = { action: 'init' }
    if (WID) p.window_id = WID
    if (DNAME) p.display_name = DNAME
    ch.send(p)
    setTimeout(refreshAgents, 500)
  })
ch.onError(() => { connected.value = false })
ch.onClose(() => { connected.value = false; clearStream(); stopPoll() })

function stopPoll() {
  if (pollTimer) { clearInterval(pollTimer); pollTimer = null }
}

function clearStream() {
  deltaBuffer = ''
  reasoningBuffer = ''
  rafPending = false
}

function flushStream() {
  rafPending = false
  if (streamingIdx.value < 0) return
  let changed = false
  if (deltaBuffer) {
    messages.value[streamingIdx.value].text += deltaBuffer
    deltaBuffer = ''
    changed = true
  }
  if (reasoningBuffer) {
    messages.value[streamingIdx.value].reasoning += reasoningBuffer
    reasoningBuffer = ''
    changed = true
  }
  if (changed) scrollBottom()
}

function scheduleFlush() {
  if (!rafPending) {
    rafPending = true
    requestAnimationFrame(flushStream)
  }
}

ch.onMessage((data) => {
  const sender = data.sender_name || ''
  const senderAvatar = data.sender_avatar || ''
  if (data.type === 'embed') {
    if (data.kind === 'text' && data.text && data.text.startsWith('房间AI助手列表')) {
      const lines = data.text.split('\n')
      agents.value = []
      for (let i = 1; i < lines.length; i++) {
        const m = lines[i].match(/(\S+)\s+(\S+)/)
        if (m) agents.value.push({ avatar: m[1], name: m[2] })
      }
    }
    messages.value.push({ isSelf: false, type: 'embed', kind: data.kind, url: data.url, title: data.title, name: data.name, text: data.text || '', sender, senderAvatar })
    scrollBottom()
  } else if (data.type === 'stream_start') {
    messages.value.push({ text: '', reasoning: '', isSelf: false, sender, senderAvatar })
    streamingIdx.value = messages.value.length - 1
    scrollBottom()
    startPoll()
  } else if (data.type === 'reasoning') {
    if (streamingIdx.value >= 0) {
      reasoningBuffer += data.text
      scheduleFlush()
    }
  } else if (data.type === 'delta') {
    if (streamingIdx.value >= 0) {
      deltaBuffer += data.text
      scheduleFlush()
    }
  } else if (data.type === 'stream_end') {
    if (deltaBuffer || reasoningBuffer) flushStream()
    clearStream()
    stopPoll()
    if (data.msg && streamingIdx.value >= 0) {
      messages.value[streamingIdx.value].text = '⚠️ ' + data.msg
    }
    streamingIdx.value = -1
    scrollBottom()
  } else if (data.type === 'agent_msg') {
    messages.value.push({
      text: data.content || '',
      isSelf: false,
      sender: data.sender_name || 'Agent',
      senderAvatar: data.sender_avatar || '🤖',
    })
    scrollBottom()
  } else if (data.type === 'agent') {
    if (data.action === 'open_app') {
      messages.value.push({ text: '🔧 Agent 正在打开 ' + data.app + '...', isSelf: false })
      window.parent.postMessage({
        type: 'agent_open_app',
        app: data.app,
        params: data.params || data,
      }, '*')
    } else if (data.action === 'control_app') {
      window.parent.postMessage({
        type: 'agent_control_app',
        app: data.app,
        command: data.command || { value: data.value },
      }, '*')
    } else if (data.action === 'close_app') {
      window.parent.postMessage({
        type: 'agent_close_app',
        app: data.app,
      }, '*')
    }
    scrollBottom()
  } else if (data.text !== undefined) {
    if (data.type === 'text' && typeof data.text === 'string' && data.text.startsWith('房间AI助手列表')) {
      const parsed = data.text.match(/(\S+)\s+(\S+)/g)
      if (parsed) agents.value = parsed.slice(1).map(s => {
        const parts = s.split(' ')
        return { avatar: parts[0], name: parts[1] }
      })
      else agents.value = []
    }
    messages.value.push({ text: data.text, isSelf: false, sender, senderAvatar })
    scrollBottom()
  }
})

function startPoll() {
  stopPoll()
  pollTimer = setInterval(() => ch.send({ action: 'poll' }), 50)
}

function wrapMermaid(el) {
  const source = el.getAttribute('data-source') || ''
  const wrapper = document.createElement('div')
  wrapper.className = 'mermaid-wrapper'
  el.before(wrapper)
  wrapper.appendChild(el)
  const src = document.createElement('pre')
  src.className = 'mermaid-source'
  src.textContent = source
  wrapper.appendChild(src)
  const btn = document.createElement('button')
  btn.className = 'mermaid-toggle'
  btn.textContent = '◇'
  btn.addEventListener('click', (e) => {
    e.stopPropagation()
    wrapper.classList.toggle('show-source')
  })
  wrapper.appendChild(btn)
  const svg = el.querySelector('svg')
  if (svg) {
    svg.style.cursor = 'pointer'
    svg.addEventListener('click', (e) => {
      e.stopPropagation()
      const blob = new Blob([svg.outerHTML], { type: 'image/svg+xml' })
      const url = URL.createObjectURL(blob)
      window.open(url, '_blank')
      setTimeout(() => URL.revokeObjectURL(url), 10000)
    })
  }
}

function renderMermaidInChat() {
  const blocks = document.querySelectorAll('.bubble pre > code.language-mermaid')
  if (!blocks.length) return
  blocks.forEach((el) => {
    const pre = el.parentElement
    if (!pre) return
    pre.classList.add('mermaid')
    pre.setAttribute('data-source', el.textContent || '')
    pre.innerHTML = el.textContent || ''
    el.remove()
  })
  const nodes = Array.from(document.querySelectorAll('.bubble .mermaid'))
  if (nodes.length) {
    mermaid.run({ nodes }).then(() => {
      nodes.forEach(wrapMermaid)
    }).catch((e) => console.error('mermaid chat error:', e))
  }
}

function scrollBottom() {
  nextTick(() => {
    const el = msgBox.value
    if (el) el.scrollTop = el.scrollHeight
    renderMermaidInChat()
  })
}

function renderMarkdown(text) {
  if (!text) return ''
  return marked.parse(text)
}

function previewImg(url) {
  window.open(url, '_blank')
}

function send() {
  const text = input.value.trim()
  if (!text || !connected.value) return
  const p = { text }
  if (WID) p.window_id = WID
  if (DNAME) p.display_name = DNAME
  ch.send(p)
  messages.value.push({ text, isSelf: true, sender: USER_NAME, senderAvatar: USER_AVATAR })
  input.value = ''
  showMentions.value = false
  scrollBottom()
}

function stopStream() {
  const p = { action: 'stop' }
  if (WID) p.window_id = WID
  ch.send(p)
  clearStream()
  streamingIdx.value = -1
}

function addAgent() {
  const name = newAgentName.value.trim()
  if (!name) return
  ch.send({ text: `/agent add ${name}` })
  newAgentName.value = ''
  showAgents.value = false
  setTimeout(refreshAgents, 800)
}

function removeAgent(name) {
  ch.send({ text: `/agent remove ${name}` })
  setTimeout(refreshAgents, 800)
}

function fixAvatar(av) {
  return /^(https?:|\/)/i.test(av) ? '🤖' : av
}

function refreshAgents() {
  ch.send({ text: '/agent list' })
}

function onInputKeydown(e) {
  if (e.key === 'Enter' && !e.shiftKey && !showMentions.value) { e.preventDefault(); send(); return }
  if (e.key === '@') { showMentions.value = true; mentionFilter.value = ''; return }
  if (showMentions.value) {
    if (e.key === 'Escape') { showMentions.value = false; return }
    if (e.key === 'Enter') { e.preventDefault(); return }
    if (e.key === ' ') { showMentions.value = false; return }
    if (e.key === 'Backspace') {
      mentionFilter.value = mentionFilter.value.slice(0, -1)
      if (!mentionFilter.value) showMentions.value = false
      return
    }
    if (e.key.length === 1) { mentionFilter.value += e.key; return }
  }
}

function selectMention(agentName) {
  input.value += agentName + ' '
  showMentions.value = false
  mentionFilter.value = ''
}

const filteredAgents = computed(() => {
  if (!showMentions.value) return []
  const f = mentionFilter.value.toLowerCase()
  return agents.value.filter(a => a.name.toLowerCase().includes(f))
})

function isImageUrl(val) {
  return /^(https?:|\/)/i.test(val)
}

window.addEventListener('message', (e) => {
  if (e.data?.type === 'window_closing') {
    ch.send({ action: 'close_window', window_id: WID })
    ch.close()
  }
})

onMounted(() => { renderMermaidInChat() })
onBeforeUnmount(() => ch.close())
</script>

<style scoped>
.chat-page {
  display: flex;
  flex-direction: column;
  height: 100vh;
  background: #f5f7fb;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, 'Noto Sans SC', sans-serif;
}

.chat-header {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 16px 20px;
  background: #fff;
  border-bottom: 1px solid #e5e7eb;
  flex-shrink: 0;
}

.chat-header h1 {
  font-size: 18px;
  font-weight: 600;
  margin: 0;
}

.chat-user {
  font-size: 13px;
  color: #6b7280;
  flex-shrink: 0;
}

.chat-type-badge {
  font-size: 11px;
  padding: 2px 8px;
  border-radius: 10px;
  flex-shrink: 0;
}

.chat-type-badge.group {
  background: #dbeafe;
  color: #1e40af;
}

.chat-type-badge.private {
  background: #fce7f3;
  color: #9d174d;
}

.agent-toggle {
  width: 36px;
  height: 30px;
  border: 1px solid #d1d5db;
  border-radius: 6px;
  background: #fff;
  cursor: pointer;
  font-size: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  line-height: 1;
}

.agent-toggle:hover {
  background: #f3f4f6;
}

.agent-panel {
  position: absolute;
  right: 8px;
  top: 8px;
  width: 200px;
  background: #fff;
  border: 1px solid #e5e7eb;
  border-radius: 8px;
  box-shadow: 0 4px 16px rgba(0,0,0,0.1);
  z-index: 10;
  padding: 8px;
}

.agent-panel-header {
  font-size: 12px;
  font-weight: 600;
  color: #374151;
  padding: 4px 8px;
  border-bottom: 1px solid #f3f4f6;
  margin-bottom: 4px;
}

.agent-item {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 4px 8px;
  border-radius: 4px;
  font-size: 12px;
}

.agent-item:hover {
  background: #f9fafb;
}

.agent-item-avatar { font-size: 14px; }
.agent-item-name { flex: 1; color: #374151; }

.agent-item-rm {
  width: 18px; height: 18px;
  border: none; background: none;
  color: #9ca3af; cursor: pointer;
  font-size: 14px; line-height: 1;
}

.agent-item-rm:hover { color: #ef4444; }

.agent-panel-add {
  display: flex;
  gap: 4px;
  margin-top: 4px;
}

.agent-add-input {
  flex: 1;
  padding: 4px 8px;
  border: 1px solid #e5e7eb;
  border-radius: 4px;
  font-size: 12px;
  outline: none;
}

.agent-add-btn {
  width: 24px; height: 24px;
  border: none;
  background: #2563eb;
  color: #fff;
  border-radius: 4px;
  cursor: pointer;
  font-size: 14px;
}

.chat-input-wrap {
  flex: 1;
  position: relative;
}

.mention-dropdown {
  position: absolute;
  bottom: 100%;
  left: 0;
  right: 0;
  background: #fff;
  border: 1px solid #e5e7eb;
  border-radius: 8px 8px 0 0;
  box-shadow: 0 -2px 8px rgba(0,0,0,0.06);
  max-height: 160px;
  overflow-y: auto;
  z-index: 20;
}

.mention-item {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 6px 12px;
  cursor: pointer;
  font-size: 13px;
}

.mention-item:hover {
  background: #eff6ff;
}

.mention-avatar { font-size: 16px; }
.mention-name { color: #1f2937; }

.fade-enter-active, .fade-leave-active {
  transition: opacity 0.15s ease;
}
.fade-enter-from, .fade-leave-to {
  opacity: 0;
}

.chat-status {
  font-size: 12px;
  padding: 2px 10px;
  border-radius: 10px;
  flex-shrink: 0;
}

.status-ok {
  background: #d1fae5;
  color: #065f46;
}

.status-err {
  background: #fee2e2;
  color: #991b1b;
}

.chat-main {
  position: relative;
  flex: 1;
  overflow-y: auto;
  padding: 16px 20px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.msg {
  display: flex;
  align-items: flex-start;
}

.msg-self {
  justify-content: flex-end;
}

.msg-other {
  justify-content: flex-start;
}

.msg-inner {
  display: flex;
  flex-direction: column;
  max-width: 70%;
}

.msg-self .msg-inner {
  align-items: flex-end;
}

.msg-other .msg-inner {
  align-items: flex-start;
}

.msg-inner {
  display: flex;
  flex-direction: column;
  max-width: 70%;
}

.msg-self .msg-inner {
  align-items: flex-end;
}

.msg-other .msg-inner {
  align-items: flex-start;
}

.msg-sender {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 0 12px 2px;
  font-size: 12px;
}

.msg-sender-other {
  justify-content: flex-start;
}

.msg-sender-self {
  justify-content: flex-end;
}

.sender-avatar {
  font-size: 14px;
  line-height: 1;
}

.sender-name {
  color: #6b7280;
  font-weight: 500;
}

.msg-wrapper {
  display: flex;
  max-width: 70%;
  align-items: flex-start;
  gap: 8px;
}

.wrapper-self {
  flex-direction: row-reverse;
}

.msg-content-col {
  display: flex;
  flex-direction: column;
  flex: 1;
  min-width: 0;
}

.msg-self .msg-content-col {
  align-items: flex-end;
}

.msg-name {
  font-size: 15px;
  color: #6b7280;
  font-weight: 500;
  padding-bottom: 2px;
  white-space: nowrap;
}

.msg-avatar {
  width: 50px;
  height: 50px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 20px;
  flex-shrink: 0;
  background: #e5e7eb;
}

.msg-other .msg-avatar {
  background: #dbeafe;
}

.avatar-img {
  width: 100%;
  height: 100%;
  border-radius: 50%;
  object-fit: cover;
}

.bubble {
  max-width: 100%;
  padding: 10px 16px;
  border-radius: 18px;
  font-size: 15px;
  line-height: 1.5;
  word-break: break-word;
}

.msg-self .bubble {
  background: #2563eb;
  color: #fff;
  border-bottom-right-radius: 4px;
}

.msg-other .bubble {
  background: #e5e7eb;
  color: #1f2937;
  border-bottom-left-radius: 4px;
}

.chat-footer {
  display: flex;
  gap: 8px;
  padding: 12px 20px;
  background: #fff;
  border-top: 1px solid #e5e7eb;
  flex-shrink: 0;
}

.chat-input {
  flex: 1;
  padding: 10px 14px;
  border: 1px solid #d1d5db;
  border-radius: 24px;
  font-size: 15px;
  outline: none;
  transition: border-color 0.2s;
  resize: vertical;
  font-family: inherit;
  line-height: 1.5;
  min-height: 40px;
  max-height: 200px;
}

.chat-input:focus {
  border-color: #2563eb;
}

.chat-input:disabled {
  background: #f3f4f6;
}

.chat-send {
  padding: 10px 20px;
  background: #2563eb;
  color: #fff;
  border: none;
  border-radius: 24px;
  font-size: 14px;
  cursor: pointer;
  transition: background 0.2s;
  flex-shrink: 0;
}

.chat-send:hover:not(:disabled) {
  background: #1d4ed8;
}

.chat-send:disabled {
  opacity: 0.4;
  cursor: default;
}

.chat-stop {
  width: 38px;
  height: 38px;
  border-radius: 50%;
  background: #e81123;
  color: #fff;
  border: none;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  transition: background 0.2s;
}

.chat-stop:hover {
  background: #c50f1f;
}

/* ---- Embed bubbles ---- */

.bubble-embed {
  max-width: 85%;
  padding: 8px;
}

.bubble-image,
.bubble-video,
.bubble-audio,
.bubble-game {
  background: #fff;
  border: 1px solid #e5e7eb;
  border-bottom-left-radius: 4px;
}

.msg-self .bubble-image,
.msg-self .bubble-video,
.msg-self .bubble-audio,
.msg-self .bubble-game {
  background: #eff6ff;
  border-color: #bfdbfe;
  border-bottom-left-radius: 18px;
  border-bottom-right-radius: 4px;
}

.embed-body {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.embed-img {
  max-width: 100%;
  max-height: 300px;
  border-radius: 10px;
  cursor: pointer;
  object-fit: contain;
  background: #f0f0f0;
}

.embed-video {
  max-width: 100%;
  max-height: 360px;
  border-radius: 10px;
  background: #000;
}

.embed-audio {
  width: 100%;
  min-width: 280px;
  height: 40px;
}

.bubble-audio {
  max-width: 95%;
}

.embed-title {
  font-size: 12px;
  color: #6b7280;
  padding: 0 4px;
}

.audio-label {
  font-size: 14px;
  font-weight: 600;
  color: #374151;
  padding: 4px 0;
}

/* Game embed */
.embed-game {
  flex-direction: row;
  align-items: center;
  gap: 12px;
  padding: 8px 4px;
}

.game-icon {
  font-size: 32px;
  flex-shrink: 0;
}

.game-info {
  display: flex;
  flex-direction: column;
  gap: 6px;
  flex: 1;
}

.game-name {
  font-size: 15px;
  font-weight: 600;
  color: #1f2937;
}

.game-link {
  display: inline-block;
  padding: 6px 16px;
  background: #2563eb;
  color: #fff;
  border-radius: 20px;
  text-decoration: none;
  font-size: 13px;
  text-align: center;
  transition: background 0.2s;
}

.game-link:hover {
  background: #1d4ed8;
}

/* Markdown text styling */
.bubble :deep(p) {
  margin: 0 0 8px;
}
.bubble :deep(p:last-child) {
  margin-bottom: 0;
}
.bubble :deep(code) {
  background: rgba(0,0,0,0.07);
  padding: 2px 6px;
  border-radius: 4px;
  font-size: 13px;
}
.bubble :deep(pre) {
  background: rgba(0,0,0,0.07);
  padding: 10px 14px;
  border-radius: 8px;
  overflow-x: auto;
  font-size: 13px;
  margin: 8px 0;
}
.bubble :deep(pre code) {
  background: none;
  padding: 0;
}
.bubble :deep(a) {
  color: inherit;
  text-decoration: underline;
}
.bubble :deep(ul),
.bubble :deep(ol) {
  padding-left: 20px;
  margin: 4px 0;
}
.bubble :deep(blockquote) {
  border-left: 3px solid #9ca3af;
  margin: 8px 0;
  padding: 4px 12px;
  color: #6b7280;
}
.bubble :deep(h1),
.bubble :deep(h2),
.bubble :deep(h3),
.bubble :deep(h4) {
  margin: 8px 0 4px;
  font-size: inherit;
  font-weight: 600;
}
.bubble :deep(img) {
  max-width: 100%;
  border-radius: 8px;
  margin: 4px 0;
}
.bubble :deep(.mermaid-wrapper) {
  position: relative;
}
.bubble :deep(.mermaid-toggle) {
  position: absolute;
  top: 4px;
  left: 4px;
  z-index: 1;
  background: rgba(0,0,0,0.35);
  border: none;
  color: #fff;
  border-radius: 4px;
  padding: 2px 8px;
  cursor: pointer;
  font-size: 12px;
  line-height: 1.4;
  opacity: 0.5;
  transition: opacity 0.2s;
}
.bubble :deep(.mermaid-wrapper:hover .mermaid-toggle) {
  opacity: 1;
}
.bubble :deep(.mermaid-source) {
  display: none;
  margin: 0;
  padding: 10px 14px;
  background: rgba(0,0,0,0.07);
  border-radius: 8px;
  font-size: 13px;
  line-height: 1.5;
  overflow-x: auto;
  white-space: pre;
}
.bubble :deep(.mermaid-wrapper.show-source .mermaid-source) {
  display: block;
}
.bubble :deep(.mermaid-wrapper.show-source .mermaid > svg) {
  display: none;
}

/* Reasoning / thinking block */
.reasoning-block {
  margin-bottom: 8px;
  font-size: 13px;
  border-left: 3px solid #9ca3af;
  padding-left: 10px;
}

.reasoning-summary {
  cursor: pointer;
  color: #6b7280;
  font-weight: 500;
  user-select: none;
  padding: 2px 0;
}

.reasoning-summary:hover {
  color: #4b5563;
}

.reasoning-content {
  margin-top: 6px;
  color: #6b7280;
  font-size: 13px;
  line-height: 1.5;
  white-space: pre-wrap;
  word-break: break-word;
}
</style>
