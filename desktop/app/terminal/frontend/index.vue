<template>
  <div ref="termContainer" class="term-container"></div>
</template>

<script setup>
import { ref, onMounted, onBeforeUnmount } from 'vue'
import { Terminal } from 'xterm'
import { FitAddon } from '@xterm/addon-fit'
import 'xterm/css/xterm.css'
import { getWsUrl } from '../../../src/services/config.js'
import { createChannel } from '../../../src/services/channel.js'

const WS_URL = getWsUrl()
const BASE = 'desktop/public/home'
const WID = new URLSearchParams(location.search).get('wid') || ''
const DNAME = decodeURIComponent(new URLSearchParams(location.search).get('name') || '')

const termContainer = ref(null)
let term = null
let fitAddon = null
let ch = null
let pollTimer = null

function bindSocket() {
  ch = createChannel(WS_URL, { maxRetries: 5 })

  ch.onOpen((isReconnect) => {
    if (isReconnect) {
      const p = { action: 'resume', app: 'terminal' }
      if (WID) p.window_id = WID
      if (DNAME) p.display_name = DNAME
      ch.send(p)
    } else {
      const p = {
        app: 'terminal',
        action: 'exec',
        cmd: `cd ${BASE} && PS1='\\w # ' bash --norc`
      }
      if (WID) p.window_id = WID
      if (DNAME) p.display_name = DNAME
      ch.send(p)
    }
  })

  ch.onMessage((data) => {
    if (data.type === 'output' && data.text) {
      term.write(data.text)
    }
  })

  ch.onClose(() => {
    term.write('\r\n\x1b[31m连接断开\x1b[0m\r\n')
  })

  ch.onError(() => {
    term.write('\r\n\x1b[31m连接错误\x1b[0m\r\n')
  })
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
    ch.send({ action: 'stdin', data })
  })

  bindSocket()

  pollTimer = setInterval(() => {
    ch.send({ action: 'stdout' })
  }, 60)

  window.addEventListener('resize', () => {
    fitAddon?.fit()
    ch.send({
      action: 'resize',
      rows: term.rows,
      cols: term.cols
    })
  })
})

onBeforeUnmount(() => {
  if (pollTimer) clearInterval(pollTimer)
  ch.send({ action: 'close_window', window_id: WID })
  ch.close()
  setTimeout(() => {
    if (term) term.dispose()
  }, 50)
})

window.addEventListener('message', (e) => {
  if (e.data?.type === 'window_closing') {
    ch.send({ action: 'close_window', window_id: WID })
    ch.close()
    setTimeout(() => { if (term) term.dispose() }, 50)
  }
})
</script>

<style scoped>
.term-container {
  height: 100vh;
  width: 100%;
  background: #0d1117;
}
</style>
