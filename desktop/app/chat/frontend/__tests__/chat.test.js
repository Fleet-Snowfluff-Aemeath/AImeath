import { describe, it, expect, vi, beforeEach } from 'vitest'
import { mount } from '@vue/test-utils'

let onOpenCb = null
let onMsgCb = null
let sentMessages = []
vi.mock('../../../../src/services/channel.js', () => ({
  createChannel: vi.fn(() => ({
    send: vi.fn((p) => { sentMessages.push(p) }),
    onOpen: vi.fn(fn => { onOpenCb = fn }),
    onMessage: vi.fn(fn => { onMsgCb = fn }),
    onError: vi.fn(),
    onClose: vi.fn(),
    close: vi.fn(),
  })),
}))
const fire = (d) => onMsgCb && onMsgCb(d)
const fireOpen = () => onOpenCb && onOpenCb()

import ChatPage from '../index.vue'

describe('Chat', () => {
  beforeEach(() => {
    sentMessages = []
    onOpenCb = null
    onMsgCb = null
  })

  it('挂载显示标题', () => {
    const w = mount(ChatPage)
    expect(w.find('h1').text()).toBe('聊天')
  })

  it('显示未连接状态', () => {
    const w = mount(ChatPage)
    const status = w.find('.chat-status')
    expect(status.exists()).toBe(true)
    expect(status.text()).toBe('未连接')
    expect(status.classes()).toContain('status-err')
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

  it('收到 stream_end 后清除流状态', async () => {
    const w = mount(ChatPage)
    fire({ type: 'stream_start' })
    await w.vm.$nextTick()
    fire({ type: 'delta', text: 'hello' })
    await w.vm.$nextTick()
    fire({ type: 'stream_end' })
    await w.vm.$nextTick()
    const stopBtn = w.find('.chat-stop')
    expect(stopBtn.exists()).toBe(false)
  })

  it('收到纯文本消息渲染 markdown', async () => {
    const w = mount(ChatPage)
    fire({ text: '**bold** text' })
    await w.vm.$nextTick()
    const bubbles = w.findAll('.bubble')
    expect(bubbles.length).toBeGreaterThan(0)
    expect(w.html()).toContain('<strong>')
  })

  it('空输入不发送消息', async () => {
    const w = mount(ChatPage)
    fireOpen()
    await w.vm.$nextTick()
    const initCount = sentMessages.length
    w.vm.input = '   '
    w.vm.send()
    await w.vm.$nextTick()
    expect(sentMessages.length).toBe(initCount)
  })

  it('发送消息后输入框清空', async () => {
    const w = mount(ChatPage)
    fireOpen()
    await w.vm.$nextTick()
    w.vm.connected = true
    w.vm.input = 'hello'
    w.vm.send()
    await w.vm.$nextTick()
    expect(w.vm.input).toBe('')
    const textMsgs = sentMessages.filter(m => m.text)
    expect(textMsgs.length).toBeGreaterThan(0)
    expect(textMsgs[0].text).toBe('hello')
  })

  it('agent open_app 消息 postMessage 到 parent', async () => {
    const messages = []
    const origPostMessage = window.parent.postMessage
    window.parent.postMessage = (data, origin) => messages.push(data)
    const w = mount(ChatPage)
    fire({ type: 'agent', action: 'open_app', app: 'snake' })
    await w.vm.$nextTick()
    expect(messages.length).toBe(1)
    expect(messages[0].type).toBe('agent_open_app')
    expect(messages[0].app).toBe('snake')
    window.parent.postMessage = origPostMessage
  })

  it('agent close_app 消息 postMessage 到 parent', async () => {
    const messages = []
    const origPostMessage = window.parent.postMessage
    window.parent.postMessage = (data, origin) => messages.push(data)
    const w = mount(ChatPage)
    fire({ type: 'agent', action: 'close_app', app: 'pacman' })
    await w.vm.$nextTick()
    expect(messages.length).toBe(1)
    expect(messages[0].type).toBe('agent_close_app')
    window.parent.postMessage = origPostMessage
  })

  it('window_closing postMessage 发送 close_window', async () => {
    const w = mount(ChatPage)
    window.dispatchEvent(new MessageEvent('message', { data: { type: 'window_closing' } }))
    await w.vm.$nextTick()
    const closeMsg = sentMessages.find(m => m.action === 'close_window')
    expect(closeMsg).toBeDefined()
  })
})
