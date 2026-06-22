const WS_URL = `ws://${location.hostname}:3001`
const MAX_RETRIES = 5

export function createGameSocket(game, width = 20, height = 20) {
  let ws = null
  let reconnectAttempts = 0
  let reconnectTimer = null
  const listeners = new Set()
  let intentionalClose = false

  function connect() {
    if (intentionalClose) return
    ws = new WebSocket(WS_URL)

    ws.onopen = () => {
      reconnectAttempts = 0
      ws.send(JSON.stringify({ action: 'new_game', game, width, height }))
    }

    ws.onmessage = (e) => {
      try {
        const data = JSON.parse(e.data)
        for (const fn of listeners) fn(data)
      } catch {
        for (const fn of listeners) fn({ type: 'error', msg: 'Invalid message' })
      }
    }

    ws.onerror = () => {
      for (const fn of listeners) fn({ type: 'error', msg: 'Connection error' })
    }

    ws.onclose = () => {
      if (!intentionalClose && reconnectAttempts < MAX_RETRIES) {
        reconnectAttempts++
        const delay = Math.min(1000 * Math.pow(2, reconnectAttempts - 1), 8000)
        for (const fn of listeners) fn({ type: 'reconnecting', delay, attempt: reconnectAttempts, max: MAX_RETRIES })
        reconnectTimer = setTimeout(connect, delay)
      } else {
        for (const fn of listeners) fn({ type: 'closed' })
      }
    }
  }

  connect()

  return {
    send(msg) {
      if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(msg))
    },
    tick(value) {
      this.send({ action: 'tick', value })
    },
    endGame() {
      intentionalClose = true
      if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null }
      this.send({ action: 'end_game' })
      if (ws) ws.close()
    },
    onState(fn) {
      listeners.add(fn)
      return () => listeners.delete(fn)
    },
    close() {
      intentionalClose = true
      if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null }
      if (ws) ws.close()
    },
  }
}
