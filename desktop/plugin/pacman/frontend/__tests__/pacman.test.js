import { describe, it, expect, vi, beforeEach } from 'vitest'
import { mount } from '@vue/test-utils'

let onMsgCb = null
vi.mock('../../../../src/services/channel.js', () => ({
  createChannel: vi.fn(() => ({
    send: vi.fn(), onOpen: vi.fn(fn => fn()), onMessage: vi.fn(fn => { onMsgCb = fn }),
    onError: vi.fn(), onReconnecting: vi.fn(), onClose: vi.fn(), close: vi.fn(),
  })),
}))
const fireMsg = (d) => onMsgCb && onMsgCb(d)

import Game from '../index.vue'
const m = (p) => mount(Game, { props: p })

beforeEach(() => { onMsgCb = null })

describe('Pacman', () => {
  it('游戏名称', () => {
    expect(m({ gameType: 'pacman' }).find('h1').text()).toBe('吃豆豆')
  })

  it('初始状态无网格', () => {
    const w = m({ gameType: 'pacman' })
    expect(w.find('.grid-wrapper').exists()).toBe(false)
  })

  it('收到状态渲染网格', async () => {
    const w = m({ gameType: 'pacman' })
    fireMsg({ type: 'pacman', grid: '@ *\n* @', score: 5, over: false })
    await w.vm.$nextTick()
    expect(w.find('.grid-wrapper').exists()).toBe(true)
    expect(w.text()).toContain('Score: 5')
  })

  it('显示分数', async () => {
    const w = m({ gameType: 'pacman' })
    fireMsg({ type: 'pacman', grid: '@', score: 30, over: false })
    await w.vm.$nextTick()
    expect(w.text()).toContain('Score: 30')
  })

  it('Game Over 显示最终分数', async () => {
    const w = m({ gameType: 'pacman' })
    fireMsg({ type: 'pacman', grid: ' \n$', score: 10, over: true })
    await w.vm.$nextTick()
    expect(w.text()).toContain('Game Over')
    expect(w.text()).toContain('Score: 10')
  })

  it('多次状态更新不崩溃', async () => {
    const w = m({ gameType: 'pacman' })
    for (let i = 0; i < 5; i++) {
      fireMsg({ type: 'pacman', grid: '@ *', score: i * 10, over: false })
      await w.vm.$nextTick()
    }
    expect(w.find('.grid-wrapper').exists()).toBe(true)
  })

  it('错误消息显示', async () => {
    const w = m({ gameType: 'pacman' })
    fireMsg({ type: 'error', msg: '连接失败' })
    await w.vm.$nextTick()
    expect(w.find('.error-msg').text()).toBe('连接失败')
  })

  it('卸载清理', () => {
    const w = m({ gameType: 'pacman' })
    const unmount = w.unmount
    expect(() => w.unmount()).not.toThrow()
  })
})
