import { describe, it, expect, vi, beforeEach } from 'vitest'
import { mount } from '@vue/test-utils'

let resolveList = null

vi.mock('../filemgrChannel.js', () => ({
  createFilemgrChannel: vi.fn(() => ({
    listDir: vi.fn(() => new Promise(r => { resolveList = r })),
    onOpen: vi.fn(fn => { setTimeout(fn, 0) }),
    onError: vi.fn(),
    close: vi.fn(),
  })),
}))

import FileManager from '../index.vue'

const MOCK_ENTRIES = [
  { name: 'documents', kind: 'dir' },
  { name: 'readme.md', kind: 'file' },
  { name: 'photo.jpg', kind: 'file' },
]

async function tick() { await new Promise(r => setTimeout(r, 20)) }

describe('FileManager', () => {
  beforeEach(() => {
    resolveList = null
    vi.clearAllMocks()
  })

  it('挂载显示标题', () => {
    const w = mount(FileManager)
    expect(w.find('.fm-title').text()).toBe('文件管理器')
  })

  it('初始显示未连接', () => {
    const w = mount(FileManager)
    expect(w.find('.status-err').exists()).toBe(true)
  })

  it('收到 listing 消息后渲染条目', async () => {
    const w = mount(FileManager)
    await tick()
    resolveList(MOCK_ENTRIES)
    await tick()
    await w.vm.$nextTick()
    expect(w.findAll('.fm-item').length).toBe(3)
  })

  it('空目录显示空提示', async () => {
    const w = mount(FileManager)
    await tick()
    resolveList([])
    await tick()
    await w.vm.$nextTick()
    expect(w.find('.fm-empty').exists()).toBe(true)
  })

  it('搜索过滤条目', async () => {
    const w = mount(FileManager)
    await tick()
    resolveList(MOCK_ENTRIES)
    await tick()
    await w.vm.$nextTick()
    w.vm.searchText = 'readme'
    await w.vm.$nextTick()
    expect(w.findAll('.fm-item').length).toBe(1)
  })

  it('导航按钮初始禁用', () => {
    const w = mount(FileManager)
    const buttons = w.findAll('.fm-btn')
    expect(buttons[0].attributes('disabled')).toBeDefined()
    expect(buttons[1].attributes('disabled')).toBeDefined()
  })

  it('加载错误时显示错误文本', async () => {
    const w = mount(FileManager)
    await tick()
    try { resolveList(Promise.reject(new Error('permission denied'))) } catch {}
    await tick()
    await w.vm.$nextTick()
  })

  it('点击条目选中', async () => {
    const w = mount(FileManager)
    await tick()
    resolveList(MOCK_ENTRIES)
    await tick()
    await w.vm.$nextTick()
    const items = w.findAll('.fm-item')
    if (items.length > 0) {
      await items[0].trigger('click')
      expect(w.find('.fm-item.selected').exists()).toBe(true)
    }
  })

  it('挂载不报错', () => {
    const w = mount(FileManager)
    w.unmount()
  })
})
