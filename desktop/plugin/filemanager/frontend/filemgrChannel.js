import { createChannel } from '../../../src/services/channel.js'
import { getWsUrl } from '../../../src/services/config.js'

const WS_URL = getWsUrl()
const WID = new URLSearchParams(location.search).get('wid') || ''
const DNAME = decodeURIComponent(new URLSearchParams(location.search).get('name') || '')

export function createFilemgrChannel() {
  const ch = createChannel(WS_URL, { maxRetries: 5 })
  const openHandlers = new Set()
  const errorHandlers = new Set()
  let pending = null

  ch.onOpen(() => { openHandlers.forEach(fn => fn()) })
  ch.onError(() => { errorHandlers.forEach(fn => fn()) })

  ch.onMessage(data => {
    if (pending && (data.type === 'listing' || data.type === 'error')) {
      const resolve = pending
      pending = null
      resolve(data)
    }
  })

  function request(payload) {
    return new Promise((resolve, reject) => {
      if (pending) { reject(new Error('concurrent request')); return }
      pending = resolve
      const p = { ...payload }
      if (WID) p.window_id = WID
      if (DNAME) p.display_name = DNAME
      ch.send(p)
      setTimeout(() => {
        if (pending) { pending = null; reject(new Error('timeout')) }
      }, 10000)
    })
  }

  async function listDir(path) {
    const resp = await request({ plugin: 'filemanager', action: 'list', path })
    if (resp.type === 'error') throw new Error(resp.msg)
    return resp.entries || []
  }

  window.addEventListener('message', (e) => {
    if (e.data?.type === 'window_closing') {
      ch.send({ action: 'close_window', window_id: WID })
      ch.close()
    }
  })

  return {
    listDir,
    onOpen(fn) { openHandlers.add(fn); return () => openHandlers.delete(fn) },
    onError(fn) { errorHandlers.add(fn); return () => errorHandlers.delete(fn) },
    get readyState() { return ch.readyState },
    close() { ch.close() },
  }
}
