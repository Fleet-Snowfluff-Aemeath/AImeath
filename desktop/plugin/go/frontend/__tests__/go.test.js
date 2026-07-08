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

describe('Go', () => {
  it('游戏名称', () => {
    expect(m({ gameType: 'go' }).find('h1').text()).toBe('围棋')
  })

  it('初始状态无网格', () => {
    const w = m({ gameType: 'go' })
    expect(w.find('.grid-wrapper').exists()).toBe(false)
  })

  it('收到状态渲染网格', async () => {
    const w = m({ gameType: 'go' })
    fireMsg({ type: 'go', grid: 'BW\nWB', score: 0, over: false, turn: 'B', passes: 0, capsB: 0, capsW: 0, komi: 3.75 })
    await w.vm.$nextTick()
    expect(w.find('.grid').exists()).toBe(true)
  })

  it('显示回合信息', async () => {
    const w = m({ gameType: 'go' })
    fireMsg({ type: 'go', grid: 'B', score: 0, over: false, turn: 'B', passes: 1, capsB: 2, capsW: 1, komi: 3.75 })
    await w.vm.$nextTick()
    expect(w.text()).toContain('黑棋')
    expect(w.text()).toContain('B2')
    expect(w.text()).toContain('W1')
    expect(w.text()).toContain('3.75')
  })

  it('回合切换为白棋', async () => {
    const w = m({ gameType: 'go' })
    fireMsg({ type: 'go', grid: 'B\n W', score: 0, over: false, turn: 'W' })
    await w.vm.$nextTick()
    expect(w.text()).toContain('白棋')
  })

  it('Black wins', async () => {
    const w = m({ gameType: 'go' })
    fireMsg({ type: 'go', grid: 'BW', score: 0, over: true, winner: 1 })
    await w.vm.$nextTick()
    expect(w.text()).toContain('Black wins!')
  })

  it('White wins', async () => {
    const w = m({ gameType: 'go' })
    fireMsg({ type: 'go', grid: 'WB', score: 0, over: true, winner: 2 })
    await w.vm.$nextTick()
    expect(w.text()).toContain('White wins!')
  })

  it('Draw', async () => {
    const w = m({ gameType: 'go' })
    fireMsg({ type: 'go', grid: '..', score: 0, over: true, winner: 0 })
    await w.vm.$nextTick()
    expect(w.text()).toContain('Draw.')
  })

  it('标记死子模式', async () => {
    const w = m({ gameType: 'go' })
    fireMsg({ type: 'go', grid: 'BW\nWB', score: 0, over: true, winner: 0, marking: true, deadMask: '1000' })
    await w.vm.$nextTick()
    expect(w.text()).toContain('确认死子')
  })

  it('Game Over 时隐藏 winner (marking 模式)', async () => {
    const w = m({ gameType: 'go' })
    fireMsg({ type: 'go', grid: 'BW', score: 0, over: true, winner: 1, marking: true })
    await w.vm.$nextTick()
    expect(w.text()).not.toContain('Black wins!')
  })

  it('错误消息显示', async () => {
    const w = m({ gameType: 'go' })
    fireMsg({ type: 'error', msg: '无效落子' })
    await w.vm.$nextTick()
    expect(w.find('.error-msg').text()).toBe('无效落子')
  })

  it('卸载清理', () => {
    const w = m({ gameType: 'go' })
    expect(() => w.unmount()).not.toThrow()
  })
})
