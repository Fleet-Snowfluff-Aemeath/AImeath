const DEFAULTS = {
  maxRetries: 5,
  retryDelay: 1000,
  retryBackoff: 2,
  maxRetryDelay: 8000,
}

export function createChannel(url, options = {}) {
  const opts = { ...DEFAULTS, ...options }

  let ws = null
  let reconnectAttempts = 0
  let reconnectTimer = null
  let intentionalClose = false

  const typeHandlers = {}
  const allHandlers = new Set()
  const openHandlers = new Set()
  const errorHandlers = new Set()
  const reconnectingHandlers = new Set()
  const closeHandlers = new Set()

  function connect() {
    if (intentionalClose) return
    ws = new WebSocket(url)

    ws.onopen = () => {
      reconnectAttempts = 0
      openHandlers.forEach(fn => fn())
    }

    ws.onmessage = (e) => {
      try {
        const data = JSON.parse(e.data)
        const type = data.type
        allHandlers.forEach(fn => fn(data))
        if (type) typeHandlers[type]?.forEach(fn => fn(data))
      } catch {
        // malformed message, silently ignored
      }
    }

    ws.onerror = () => {
      errorHandlers.forEach(fn => fn())
    }

    ws.onclose = () => {
      if (!intentionalClose && (opts.maxRetries < 0 || reconnectAttempts < opts.maxRetries)) {
        reconnectAttempts++
        const delay = Math.min(
          opts.retryDelay * Math.pow(opts.retryBackoff, reconnectAttempts - 1),
          opts.maxRetryDelay,
        )
        reconnectingHandlers.forEach(fn => fn({ attempt: reconnectAttempts, max: opts.maxRetries, delay }))
        reconnectTimer = setTimeout(connect, delay)
      } else {
        closeHandlers.forEach(fn => fn())
      }
    }
  }

  connect()

  function close() {
    intentionalClose = true
    if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null }
    setTimeout(() => {
      try { if (ws) ws.close() } catch (_) {}
      ws = null
    }, 50)
  }

  return {
    send(payload) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(payload))
      }
    },
    on(type, fn) {
      if (!typeHandlers[type]) typeHandlers[type] = new Set()
      typeHandlers[type].add(fn)
      return () => typeHandlers[type].delete(fn)
    },
    onMessage(fn) {
      allHandlers.add(fn)
      return () => allHandlers.delete(fn)
    },
    onOpen(fn) {
      openHandlers.add(fn)
      return () => openHandlers.delete(fn)
    },
    onError(fn) {
      errorHandlers.add(fn)
      return () => errorHandlers.delete(fn)
    },
    onReconnecting(fn) {
      reconnectingHandlers.add(fn)
      return () => reconnectingHandlers.delete(fn)
    },
    onClose(fn) {
      closeHandlers.add(fn)
      return () => closeHandlers.delete(fn)
    },
    close,
    get readyState() {
      return ws ? ws.readyState : WebSocket.CLOSED
    },
  }
}
