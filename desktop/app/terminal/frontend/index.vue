<template>
  <div ref="termContainer" class="term-container"></div>
</template>

<script setup>
import { ref, onMounted, onBeforeUnmount } from 'vue'
import { Terminal } from 'xterm'
import { FitAddon } from '@xterm/addon-fit'
import 'xterm/css/xterm.css'

const WS_URL = `ws://${location.hostname}:3001`
const BASE = 'desktop/public/home'

const termContainer = ref(null)
let term = null
let fitAddon = null
let ws = null
let pollTimer = null

function connect() {
  ws = new WebSocket(WS_URL)

  ws.onopen = () => {
    ws.send(JSON.stringify({
      app: 'terminal',
      action: 'exec',
      cmd: `cd ${BASE} && PS1='\\w # ' bash --norc`
    }))
  }

  ws.onmessage = (e) => {
    try {
      const data = JSON.parse(e.data)
      if (data.type === 'output' && data.text) {
        term.write(data.text)
      }
    } catch (err) {
      // ignore
    }
  }

  ws.onclose = () => {
    term.write('\r\n\x1b[31m连接断开\x1b[0m\r\n')
  }

  ws.onerror = () => {
    term.write('\r\n\x1b[31m连接错误\x1b[0m\r\n')
  }
}

onMounted(() => {
  term = new Terminal({
    cursorBlink: true,
    cursorStyle: 'bar',
    fontSize: 14,
    fontFamily: '"Cascadia Code", "Fira Code", "Consolas", "Courier New", monospace',
    theme: {
      background: '#0d1117',
      foreground: '#e6edf3',
      cursor: '#00ff88'
    }
  })

  fitAddon = new FitAddon()
  term.loadAddon(fitAddon)
  term.open(termContainer.value)
  fitAddon.fit()

  term.onData(data => {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ action: 'stdin', data }))
    }
  })

  connect()

  pollTimer = setInterval(() => {
    if (ws && ws.readyState === WebSocket.OPEN)
      ws.send(JSON.stringify({ action: 'stdout' }))
  }, 60)

  window.addEventListener('resize', () => {
    fitAddon?.fit()
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({
        action: 'resize',
        rows: term.rows,
        cols: term.cols
      }))
    }
  })
})

onBeforeUnmount(() => {
  if (pollTimer) clearInterval(pollTimer)
  if (ws) ws.close()
  if (term) term.dispose()
})
</script>

<style scoped>
.term-container {
  height: 100vh;
  width: 100%;
  background: #0d1117;
}
</style>
