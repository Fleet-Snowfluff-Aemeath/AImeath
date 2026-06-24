import { describe, it, expect, vi } from 'vitest'
import { mount } from '@vue/test-utils'

// WebSocket mock (terminal creates new WebSocket directly)
class MockWS {
  constructor() { this.send = vi.fn(); this.close = vi.fn(); this.readyState = 1 }
  addEventListener() {}
  removeEventListener() {}
}
MockWS.OPEN = 1
globalThis.WebSocket = MockWS

import TerminalPage from '../index.vue'

describe('Terminal', () => {
  it('挂载不崩溃', () => {
    const w = mount(TerminalPage)
    expect(w.find('.term-container').exists()).toBe(true)
    w.unmount()
  })
})
