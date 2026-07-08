import { bench, describe } from 'vitest'

describe('Terminal 渲染性能', () => {
  bench('JSON.parse 短消息', () => {
    JSON.parse('{"type":"output","text":"hello world"}')
  })

  bench('JSON.parse 长消息 (1KB)', () => {
    const text = 'x'.repeat(1024)
    JSON.parse(JSON.stringify({ type: 'output', text }))
  })

  bench('WebSocket 消息构造', () => {
    JSON.stringify({ action: 'stdin', data: 'echo hello\r' })
  })

  bench('ANSI 简单输出', () => {
    '\x1b[31mhello\x1b[0m'.replace(/\x1b\[[0-9;]*m/g, '')
  })
})
