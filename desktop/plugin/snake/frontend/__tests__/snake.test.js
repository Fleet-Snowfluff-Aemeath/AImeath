import { describe, it, expect, beforeEach, vi } from 'vitest'
import { mount } from '@vue/test-utils'

let onMsgCb = null
vi.mock('../../../../src/services/channel.js', () => ({
  createChannel: vi.fn(() => ({
    send: vi.fn(),
    onOpen: vi.fn(fn => fn()),
    onMessage: vi.fn(fn => { onMsgCb = fn }),
    onError: vi.fn(),
    onReconnecting: vi.fn(),
    onClose: vi.fn(),
    close: vi.fn(),
  })),
}))
const fireMsg = (data) => onMsgCb && onMsgCb(data)

import SnakeGame from '../index.vue'

describe('SnakeGame', () => {

  it('挂载后显示游戏名称', () => {
    const w = mount(SnakeGame, { props: { gameType: 'snake' } })
    expect(w.find('h1').text()).toBe('贪食蛇')
  })

  it('网格初始不渲染', () => {
    const w = mount(SnakeGame, { props: { gameType: 'snake' } })
    expect(w.find('.grid-wrapper').exists()).toBe(false)
  })

  it('收到游戏状态后渲染网格', async () => {
    const w = mount(SnakeGame, { props: { gameType: 'snake' } })
    fireMsg({ type: 'snake', grid: '  #\n ###\n   ', score: 0, over: false })
    await w.vm.$nextTick()
    expect(w.find('.grid-wrapper').exists()).toBe(true)
  })

  it('显示分数', async () => {
    const w = mount(SnakeGame, { props: { gameType: 'snake' } })
    fireMsg({ type: 'snake', grid: ' \n$', score: 42, over: false })
    await w.vm.$nextTick()
    expect(w.find('.score').text()).toContain('42')
  })

  it('Game Over 提示', async () => {
    const w = mount(SnakeGame, { props: { gameType: 'snake' } })
    fireMsg({ type: 'snake', grid: ' \n$', score: 10, over: true })
    await w.vm.$nextTick()
    expect(w.text()).toContain('Game Over')
    expect(w.text()).toContain('10')
  })

  it('错误消息', async () => {
    const w = mount(SnakeGame, { props: { gameType: 'snake' } })
    fireMsg({ type: 'error', msg: 'fail' })
    await w.vm.$nextTick()
    expect(w.find('.error-msg').text()).toBe('fail')
  })

  it('Unmount 关闭 socket', () => {
    const w = mount(SnakeGame, { props: { gameType: 'snake' } })
    w.unmount()
  })

})
