import { bench, describe } from 'vitest'

const STYLES = {
  '.': { background: '#DEB887' },
  'B': { background: '#1a1a1a', borderRadius: '50%', boxShadow: '0 0 2px rgba(255,255,255,0.2)' },
  'W': { background: '#f5f5f5', borderRadius: '50%', boxShadow: '0 1px 3px rgba(0,0,0,0.4)' },
}

function makeGrid(w = 19, h = 19, density = 0.3) {
  let grid = ''
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const r = Math.random()
      grid += r < density ? (r < density * 0.4 ? 'B' : 'W') : '.'
    }
    if (y < h - 1) grid += '\n'
  }
  return grid
}

function cellStyle(c, styles, cellSize) {
  const base = styles[c] || {}
  return { ...base, width: cellSize + 'px', height: cellSize + 'px' }
}

describe('Go 网格性能', () => {
  const grid19 = makeGrid(19, 19, 0.4)
  const rows19 = grid19.split('\n').filter(r => r.length > 0)
  const cells19 = rows19.join('').split('')

  bench('19x19 grid 拆分为 rows', () => {
    grid19.split('\n').filter(r => r.length > 0)
  })

  bench('361 个 cell 样式计算', () => {
    for (let i = 0; i < cells19.length; i++)
      cellStyle(cells19[i], STYLES, 24)
  })

  bench('deadMask 000 字符串分割', () => {
    '01100110110'.split('').map(Number)
  })
})

describe('Computed 开销', () => {
  bench('turnLabel 字符串比较', () => {
    let turn = 'B'
    for (let i = 0; i < 1000; i++) {
      if (turn === 'B') turn = 'W'; else turn = 'B'
    }
  })

  bench('winnerText 分支判断', () => {
    const state = { winner: 1 }
    for (let i = 0; i < 1000; i++) {
      if (state.winner === 1) 'Black wins!'
      else if (state.winner === 2) 'White wins!'
      else 'Draw.'
    }
  })
})
