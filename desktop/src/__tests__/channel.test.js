import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { createChannel } from '../services/channel.js'

let mockWs = null
let wsOpen = false

class MockWebSocket {
  constructor(url) {
    this.url = url
    this.readyState = 0
    this.onopen = null
    this.onmessage = null
    this.onerror = null
    this.onclose = null
    mockWs = this
    setTimeout(() => {
      this.readyState = 1
      if (this.onopen) this.onopen()
    }, 0)
  }
  send(data) {}
  close() {
    this.readyState = 3
    if (this.onclose) this.onclose()
  }
}

vi.stubGlobal('WebSocket', MockWebSocket)
MockWebSocket.OPEN = 1
MockWebSocket.CLOSED = 3

describe('createChannel', () => {
  beforeEach(() => {
    vi.useFakeTimers()
    mockWs = null
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('creates channel and connects', () => {
    const ch = createChannel('ws://test')
    expect(mockWs).not.toBeNull()
    expect(mockWs.url).toBe('ws://test')
  })

  it('fires onOpen after connection', async () => {
    const fn = vi.fn()
    const ch = createChannel('ws://test')
    ch.onOpen(fn)
    vi.advanceTimersByTime(10)
    expect(fn).toHaveBeenCalled()
  })

  it('sends JSON payload', () => {
    const ch = createChannel('ws://test')
    vi.advanceTimersByTime(10)
    mockWs.send = vi.fn()
    ch.send({ action: 'tick', value: 5 })
    expect(mockWs.send).toHaveBeenCalledWith('{"action":"tick","value":5}')
  })

  it('does not send when websocket is null', () => {
    const ch = createChannel('ws://test')
    ch.send({ action: 'test' })
  })

  it('routes messages by type', async () => {
    const ch = createChannel('ws://test')
    vi.advanceTimersByTime(10)
    const typeFn = vi.fn()
    ch.on('game', typeFn)
    mockWs.onmessage({ data: JSON.stringify({ type: 'game', grid: 'test' }) })
    expect(typeFn).toHaveBeenCalledWith({ type: 'game', grid: 'test' })
  })

  it('fires onMessage for all messages', async () => {
    const ch = createChannel('ws://test')
    vi.advanceTimersByTime(10)
    const fn = vi.fn()
    ch.onMessage(fn)
    mockWs.onmessage({ data: JSON.stringify({ type: 'game' }) })
    expect(fn).toHaveBeenCalled()
  })

  it('ignores malformed messages', async () => {
    const ch = createChannel('ws://test')
    vi.advanceTimersByTime(10)
    const fn = vi.fn()
    ch.onMessage(fn)
    mockWs.onmessage({ data: 'not json' })
    expect(fn).not.toHaveBeenCalled()
  })

  it('fires onError on ws error', () => {
    const ch = createChannel('ws://test')
    vi.advanceTimersByTime(10)
    const fn = vi.fn()
    ch.onError(fn)
    mockWs.onerror()
    expect(fn).toHaveBeenCalled()
  })

  it('fires onClose when intentional close', () => {
    const ch = createChannel('ws://test', { maxRetries: 0 })
    vi.advanceTimersByTime(10)
    const fn = vi.fn()
    ch.onClose(fn)
    ch.close()
    vi.advanceTimersByTime(100)
    expect(fn).toHaveBeenCalled()
  })

  it('returns unsubscribe from on', () => {
    const ch = createChannel('ws://test')
    const unsub = ch.on('test', vi.fn())
    unsub()
  })

  it('returns readyState', () => {
    const ch = createChannel('ws://test')
    vi.advanceTimersByTime(10)
    expect(ch.readyState).toBe(1)
  })

  it('close then send does not throw', () => {
    const ch = createChannel('ws://test', { maxRetries: 0 })
    vi.advanceTimersByTime(10)
    ch.close()
    vi.advanceTimersByTime(100)
    expect(() => ch.send({ action: 'test' })).not.toThrow()
  })

  it('fires onReconnecting with attempt count', () => {
    const ch = createChannel('ws://test', { maxRetries: 3, retryDelay: 10 })
    vi.advanceTimersByTime(10)
    const fn = vi.fn()
    ch.onReconnecting(fn)
    mockWs.readyState = 3
    mockWs.onclose({ code: 1006 })
    vi.advanceTimersByTime(100)
    expect(fn).toHaveBeenCalledWith(expect.objectContaining({ attempt: 1 }))
  })

  it('removes type listener via unsubscribe', () => {
    const ch = createChannel('ws://test')
    vi.advanceTimersByTime(10)
    const fn = vi.fn()
    const unsub = ch.on('game', fn)
    mockWs.onmessage({ data: JSON.stringify({ type: 'game', grid: 'x' }) })
    expect(fn).toHaveBeenCalledTimes(1)
    unsub()
    mockWs.onmessage({ data: JSON.stringify({ type: 'game', grid: 'y' }) })
    expect(fn).toHaveBeenCalledTimes(1)
  })
})
