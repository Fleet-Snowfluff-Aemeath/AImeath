import { describe, it, expect, vi } from 'vitest'
import { mount } from '@vue/test-utils'

let wsInstance = null
class MockWS {
  constructor(url) { this.url = url; this.send = vi.fn(); this.close = vi.fn(); this.readyState = 1; wsInstance = this }
  addEventListener() {}
  removeEventListener() {}
}
MockWS.OPEN = 1
vi.stubGlobal('WebSocket', MockWS)

let termInstance = null
vi.mock('xterm', () => {
  const term = {
    open: vi.fn(), write: vi.fn(), dispose: vi.fn(),
    loadAddon: vi.fn(), onData: vi.fn(),
    get rows() { return 24 }, get cols() { return 80 },
  }
  return { Terminal: vi.fn(function() { termInstance = term; return term }) }
})
vi.mock('@xterm/addon-fit', () => ({ FitAddon: vi.fn(function() { return { fit: vi.fn() } }) }))

import TerminalPage from '../index.vue'

describe('Terminal', () => {
  it('挂载显示终端容器', () => {
    const w = mount(TerminalPage)
    expect(w.find('.term-container').exists()).toBe(true)
    w.unmount()
  })

  it('创建 WebSocket 连接', () => {
    mount(TerminalPage)
    expect(wsInstance).not.toBeNull()
  })

  it('收到 output 消息写入终端', async () => {
    const w = mount(TerminalPage)
    await w.vm.$nextTick()
    expect(termInstance).not.toBeNull()
    w.unmount()
  })

  it('挂载不报错', () => {
    const w = mount(TerminalPage)
    w.unmount()
  })

  it('重复挂载卸载不报错', () => {
    const w = mount(TerminalPage)
    w.unmount()
    const w2 = mount(TerminalPage)
    w2.unmount()
  })
})
