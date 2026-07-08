import { bench, describe } from 'vitest'

describe('fileSystem 工具函数性能', () => {
  describe('getIcon', () => {
    bench('已知扩展名 (jpg)', () => {
      const item = { name: 'photo.jpg', kind: 'file' }
      const map = {
        jpg: '🖼️', png: '🖼️', gif: '🖼️', mp4: '🎬', mp3: '🎵',
        pdf: '📄', txt: '📝', md: '📝', html: '🌐', js: '⚡',
        cpp: '⚙️', py: '🐍', zip: '📦',
      }
      const ext = item.name.split('.').pop().toLowerCase()
      map[ext] || '📄'
    })

    bench('未知扩展名 (fallback)', () => {
      const item = { name: 'file.xyz', kind: 'file' }
      const map = { jpg: '🖼️', png: '🖼️' }
      const ext = item.name.split('.').pop().toLowerCase()
      map[ext] || '📄'
    })
  })

  describe('formatSize', () => {
    bench('Bytes', () => { formatSize(123) })
    bench('KB', () => { formatSize(50000) })
    bench('MB', () => { formatSize(50000000) })
    bench('GB', () => { formatSize(5000000000) })
  })

  describe('formatDate', () => {
    bench('格式化日期', () => {
      formatDate('2024-12-25T10:30:00')
    })
  })
})

describe('Vue 模板渲染性能', () => {
  describe('computed 计算', () => {
    bench('breadcrumbs 拆分', () => {
      const parts = '/home/user/documents/work'.split('/').filter(Boolean)
      const crumbs = []
      for (let i = 0; i < parts.length; i++)
        crumbs.push({ name: parts[i], path: '/' + parts.slice(0, i + 1).join('/') })
    })
  })

  describe('条目列表过滤', () => {
    const items = Array.from({ length: 100 }, (_, i) => ({
      name: `file-${i}.${i % 3 === 0 ? 'jpg' : 'txt'}`,
      kind: 'file',
    }))

    bench('无过滤 (100 items)', () => {
      items
    })

    bench('关键词过滤 (100 items)', () => {
      items.filter(item => item.name.toLowerCase().includes('jpg'))
    })

    bench('无匹配过滤 (100 items)', () => {
      items.filter(item => item.name.toLowerCase().includes('xyzzy'))
    })
  })
})

function formatSize(bytes) {
  if (bytes >= 1e9) return (bytes / 1e9).toFixed(1) + ' GB'
  if (bytes >= 1e6) return (bytes / 1e6).toFixed(1) + ' MB'
  if (bytes >= 1000) return (bytes / 1000).toFixed(0) + ' KB'
  return bytes + ' B'
}

function formatDate(dateStr) {
  if (!dateStr) return ''
  const d = new Date(dateStr)
  const pad = n => String(n).padStart(2, '0')
  return `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`
}
