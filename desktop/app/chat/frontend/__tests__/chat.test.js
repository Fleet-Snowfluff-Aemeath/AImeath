import { describe, it, expect, vi } from 'vitest'
import { mount } from '@vue/test-utils'

// Chat 组件通过 channel.js 创建 WebSocket，mock channel 即可
let onMsgCb = null
vi.mock('../../../../src/services/channel.js', () => ({
  createChannel: vi.fn(() => ({
    send: vi.fn(), onOpen: vi.fn(fn => fn()), onMessage: vi.fn(fn => { onMsgCb = fn }),
    onError: vi.fn(), onClose: vi.fn(), close: vi.fn(),
  })),
}))
const fire = (d) => onMsgCb && onMsgCb(d)

import ChatPage from '../index.vue'

describe('Chat', () => {
  it('挂载显示标题', () => {
    const w = mount(ChatPage)
    expect(w.find('h1').text()).toBe('聊天')
  })

  it('收到 embed 消息', async () => {
    const w = mount(ChatPage)
    fire({ type: 'embed', kind: 'image', url: '/test.png', title: 'test' })
    await w.vm.$nextTick()
    expect(w.find('.bubble-embed').exists()).toBe(true)
  })

  it('收到 stream 消息', async () => {
    const w = mount(ChatPage)
    fire({ type: 'stream_start' })
    await w.vm.$nextTick()
    fire({ type: 'delta', text: 'hello' })
    await w.vm.$nextTick()
    expect(w.findAll('.bubble').length).toBeGreaterThan(0)
  })
})
