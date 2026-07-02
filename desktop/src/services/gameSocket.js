import { createChannel } from './channel.js'

const WS_URL = `ws://${location.hostname}:3001`
const WID = new URLSearchParams(location.search).get('wid') || ''
const DNAME = decodeURIComponent(new URLSearchParams(location.search).get('name') || '')

export function createGameSocket(game, width = 20, height = 20) {
  const ch = createChannel(WS_URL, { maxRetries: 5 })
  const stateListeners = new Set()

  ch.onOpen(() => {
    const p = { action: 'new_game', game, width, height }
    if (WID) p.window_id = WID
    if (DNAME) p.display_name = DNAME
    ch.send(p)
  })
  ch.onMessage(data => stateListeners.forEach(fn => fn(data)))
  ch.onError(() => stateListeners.forEach(fn => fn({ type: 'error', msg: 'Connection error' })))
  ch.onReconnecting(({ attempt, max, delay }) =>
    stateListeners.forEach(fn => fn({ type: 'reconnecting', attempt, max, delay })))
  ch.onClose(() => stateListeners.forEach(fn => fn({ type: 'closed' })))

  window.addEventListener('message', (e) => {
    if (e.data?.type === 'window_closing') {
      ch.send({ action: 'close_window', window_id: WID })
      ch.close()
    }
  })

  return {
    send(payload) {
      const p = { ...payload }
      if (WID) p.window_id = WID
      ch.send(p)
    },
    tick(value) { ch.send({ action: 'tick', value, window_id: WID }) },
    endGame() { ch.send({ action: 'end_game', window_id: WID }); ch.close() },
    onState(fn) {
      stateListeners.add(fn)
      return () => stateListeners.delete(fn)
    },
    close() { ch.close() },
  }
}
