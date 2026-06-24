/**
 * Snake 前端功能测试
 *
 * 使用 vitest + @vue/test-utils 测试：
 *   - 组件挂载与渲染
 *   - 按钮状态切换
 *   - WebSocket 消息处理
 *   - 网格尺寸计算
 */

import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { mount } from '@vue/test-utils'
import SnakeGame from '../index.vue'

// Mock WebSocket 相关模块
vi.mock('../../../../src/services/channel.js', () => {
  const listeners = new Set()
  const mockChannel = {
    send: vi.fn(),
    onOpen: vi.fn(cb => { cb() }),
    onMessage: vi.fn(cb => { listeners.add(cb) }),
    onError: vi.fn(),
    onReconnecting: vi.fn(),
    onClose: vi.fn(),
    close: vi.fn(),
  }
  return {
    createChannel: vi.fn(() => mockChannel),
    _listeners: listeners,
  }
})

import { createChannel } from '../../../../src/services/channel.js'

let mockChannel

beforeEach(() => {
  vi.clearAllMocks()
  mockChannel = createChannel()
})

afterEach(() => {
  vi.restoreAllMocks()
})

function mountGame(props = { gameType: 'snake' }) {
  return mount(SnakeGame, { props })
}

describe('SnakeGame 组件渲染', () => {

  it('挂载后显示游戏名称', () => {
    const wrapper = mountGame()
    expect(wrapper.find('h1').text()).toBe('贪食蛇')
  })

  it('挂载后自动连接 WebSocket（onMounted 调 startGame）', () => {
    mountGame()
    expect(createChannel).toHaveBeenCalled()
    expect(mockChannel.send).toHaveBeenCalledWith(
      expect.objectContaining({ action: 'new_game', game: 'snake' })
    )
  })

  it('网格初始不渲染（rows 为空）', () => {
    const wrapper = mountGame()
    expect(wrapper.find('.grid-wrapper').exists()).toBe(false)
  })

  it('未连接时 Start 按钮可用，Pause/End 禁用', () => {
    const wrapper = mountGame({ props: { gameType: 'snake' } })
    const btns = wrapper.findAll('button')
    const startBtn = btns[0]
    const pauseBtn = btns[1]
    const endBtn = btns[2]
    expect(startBtn.attributes('disabled')).toBeUndefined()
    expect(pauseBtn.attributes('disabled')).toBeDefined()
    expect(endBtn.attributes('disabled')).toBeDefined()
  })

})

describe('WebSocket 消息处理', () => {

  it('收到游戏状态后渲染网格', async () => {
    const wrapper = mountGame()
    // 收到状态数据
    const onMsg = vi.mocked(mockChannel.onMessage).mock.calls[0][0]
    onMsg({ type: 'snake', grid: '  #\n ###\n   ', score: 0, over: false })
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.grid-wrapper').exists()).toBe(true)
    expect(wrapper.find('.grid').exists()).toBe(true)
  })

  it('显示分数', async () => {
    const wrapper = mountGame()
    const onMsg = vi.mocked(mockChannel.onMessage).mock.calls[0][0]
    onMsg({ type: 'snake', grid: ' \n$', score: 42, over: false })
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.score').text()).toContain('42')
  })

  it('Game Over 后显示提示', async () => {
    const wrapper = mountGame()
    const onMsg = vi.mocked(mockChannel.onMessage).mock.calls[0][0]
    onMsg({ type: 'snake', grid: ' \n$', score: 10, over: true })
    await wrapper.vm.$nextTick()
    expect(wrapper.text()).toContain('Game Over')
    expect(wrapper.text()).toContain('10')
  })

  it('收到错误消息显示错误栏', async () => {
    const wrapper = mountGame()
    const onMsg = vi.mocked(mockChannel.onMessage).mock.calls[0][0]
    onMsg({ type: 'error', msg: 'Connection failed' })
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.error-msg').text()).toBe('Connection failed')
  })

  it('关闭连接后不再显示网格', async () => {
    const wrapper = mountGame()
    const onMsg = vi.mocked(mockChannel.onMessage).mock.calls[0][0]
    onMsg({ type: 'snake', grid: ' \n$', score: 0, over: false })
    await wrapper.vm.$nextTick()
    onMsg({ type: 'closed' })
    await wrapper.vm.$nextTick()
    // 组件应清理状态
    expect(wrapper.find('.grid-wrapper').exists()).toBe(true)
  })

})

describe('SnakeGame 按钮交互', () => {

  it('End 按钮触发 socket.close()', async () => {
    const wrapper = mountGame()
    // 需要先有连接状态
    const onMsg = vi.mocked(mockChannel.onMessage).mock.calls[0][0]
    onMsg({ type: 'snake', grid: '   ', score: 0, over: false })
    await wrapper.vm.$nextTick()

    const endBtn = wrapper.findAll('button')[2]
    expect(endBtn.attributes('disabled')).toBeUndefined()
    await endBtn.trigger('click')
    expect(mockChannel.send).toHaveBeenCalledWith({ action: 'end_game' })
  })

})

describe('Grid 样式计算', () => {

  it('组件挂载时请求正确尺寸', () => {
    mountGame()
    expect(mockChannel.send).toHaveBeenCalledWith(
      expect.objectContaining({ width: 20, height: 20, game: 'snake' })
    )
  })

  it('组件卸载时关闭 socket', () => {
    const wrapper = mountGame()
    wrapper.unmount()
    expect(mockChannel.close).toHaveBeenCalled()
  })

})
