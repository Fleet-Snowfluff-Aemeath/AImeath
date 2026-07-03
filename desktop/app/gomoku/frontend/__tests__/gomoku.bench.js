import { bench, describe } from 'vitest'
import { ref } from 'vue'
import { styles } from '../config.js'

function makeGrid(w, h, chars) { let g = ''; for (let y = 0; y < h; y++) { for (let x = 0; x < w; x++) g += chars[Math.floor(Math.random() * chars.length)]; if (y < h - 1) g += '\n' } return g }
function cellStyle(c, s, cs) { const b = s[c] || { background: '#111' }; return { ...b, width: cs + 'px', height: cs + 'px' } }

describe('Gomoku 网格性能', () => {
  const g15 = makeGrid(15, 15, ['B', 'W', '.'])
  const cells15 = ref(g15.split('\n').filter(r => r.length > 0).join('').split(''))
  bench('15x15 cell 样式 (225 cells)', () => {
    for (let i = 0; i < cells15.value.length; i++) cellStyle(cells15.value[i], styles, 24)
  })
})

describe('Computed 开销', () => {
  bench('Grid 拆分 (15x15)', () => {
    const g15 = makeGrid(15, 15, ['B', 'W', '.'])
    g15.split('\n').filter(r => r.length > 0)
  })

  bench('Winner 分支判断', () => {
    const state = { score: 1 }
    for (let i = 0; i < 1000; i++) {
      if (state.score === 1) 'Black wins!'
      else if (state.score === 2) 'White wins!'
    }
  })
})
