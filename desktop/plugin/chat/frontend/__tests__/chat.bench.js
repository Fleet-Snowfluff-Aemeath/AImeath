import { bench, describe } from 'vitest'
import { marked } from 'marked'

describe('Markdown 渲染性能', () => {
  bench('短文本 (10 chars)', () => {
    marked.parse('hello world')
  })

  bench('中等文本 (100 chars)', () => {
    marked.parse('hello '.repeat(20))
  })

  bench('长文本 (1000 chars)', () => {
    marked.parse('hello world. '.repeat(100))
  })

  bench('代码块渲染', () => {
    const text = 'code:\n```\n' + 'console.log("hello")\n'.repeat(10) + '```\n'
    marked.parse(text)
  })

  bench('中文文本', () => {
    marked.parse('这是一段中文文本，用于测试性能。'.repeat(20))
  })
})

describe('消息处理性能', () => {
  describe('消息构造开销', () => {
    bench('构造 100 条空文本消息', () => {
      const msgs = []
      for (let i = 0; i < 100; i++)
        msgs.push({ text: '', isSelf: false })
    })

    bench('构造 100 条含文本消息', () => {
      const msgs = []
      for (let i = 0; i < 100; i++)
        msgs.push({ text: 'message content here', isSelf: i % 2 === 0 })
    })
  })

  describe('String 操作', () => {
    bench('delta 缓冲累加 (50 chars x 20 次)', () => {
      let buf = ''
      for (let i = 0; i < 20; i++)
        buf += 'hello!'
    })

    bench('流缓冲拼接后清空', () => {
      let buf = ''
      for (let i = 0; i < 100; i++)
        buf += 'x'
      buf = ''
    })
  })
})
