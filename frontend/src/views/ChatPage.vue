<template>
  <div class="chat-page">
    <header class="chat-header">
      <a class="chat-back" href="#/" target="_self">← 返回</a>
      <h1>Chat 喵</h1>
      <span class="chat-status" :class="statusClass">{{ statusText }}</span>
    </header>
    <main class="chat-main" ref="msgBox">
      <div
        v-for="(m, i) in messages"
        :key="i"
        :class="['msg', m.isSelf ? 'msg-self' : 'msg-other']"
      >
        <div class="bubble">{{ m.text }}</div>
      </div>
    </main>
    <footer class="chat-footer">
      <input
        v-model="input"
        class="chat-input"
        placeholder="输入消息..."
        @keydown.enter="send"
        :disabled="!connected"
      />
      <button class="chat-send" @click="send" :disabled="!connected || !input.trim()">发送</button>
    </footer>
  </div>
</template>

<script setup>
import { ref, computed, nextTick, onBeforeUnmount } from 'vue'

const WS_URL = `ws://${location.hostname}:3001/chat`

const input = ref('')
const messages = ref([])
const connected = ref(false)
const ws = ref(null)

const statusText = computed(() => connected.value ? '已连接' : '未连接')
const statusClass = computed(() => connected.value ? 'status-ok' : 'status-err')

const msgBox = ref(null)

function scrollBottom() {
  nextTick(() => {
    const el = msgBox.value
    if (el) el.scrollTop = el.scrollHeight
  })
}

function send() {
  const text = input.value.trim()
  if (!text || !connected.value || !ws.value) return
  ws.value.send(JSON.stringify({ text }))
  messages.value.push({ text, isSelf: true })
  input.value = ''
  scrollBottom()
}

function connect() {
  const sock = new WebSocket(WS_URL)
  ws.value = sock

  sock.onopen = () => {
    connected.value = true
  }

  sock.onmessage = (e) => {
    try {
      const data = JSON.parse(e.data)
      if (data.text !== undefined) {
        messages.value.push({ text: data.text, isSelf: false })
        scrollBottom()
      }
    } catch {}
  }

  sock.onerror = () => {
    connected.value = false
  }

  sock.onclose = () => {
    connected.value = false
  }
}

connect()

onBeforeUnmount(() => {
  if (ws.value) ws.value.close()
})
</script>

<style scoped>
.chat-page {
  display: flex;
  flex-direction: column;
  height: 100vh;
  max-width: 600px;
  margin: 0 auto;
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

.chat-back {
  color: #2563eb;
  text-decoration: none;
  font-size: 14px;
  flex-shrink: 0;
}

.chat-back:hover {
  text-decoration: underline;
}

.chat-header h1 {
  font-size: 18px;
  font-weight: 600;
  flex: 1;
  margin: 0;
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
  flex: 1;
  overflow-y: auto;
  padding: 16px 20px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.msg {
  display: flex;
}

.msg-self {
  justify-content: flex-end;
}

.msg-other {
  justify-content: flex-start;
}

.bubble {
  max-width: 70%;
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
</style>
