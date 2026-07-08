/**
 * Snake 前端性能基准测试
 *
 * 使用 vitest bench 测试：
 *   - 网格渲染性能（不同尺寸）
 *   - 状态更新吞吐量
 *   - computed 计算开销
 */

import { bench, describe } from 'vitest'
import { ref, reactive, computed } from 'vue'

const STYLES = {
  O: { background: '#4CAF50', borderRadius: '4px' },
  '#': { background: '#2E7D32' },
  $: { background: '#f44336', borderRadius: '50%' },
  ' ': { background: '#111' },
}

function makeGrid(w, h, density = 0.3) {
  let grid = ''
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const r = Math.random()
      grid += r < density ? (r < density * 0.2 ? '$' : r < density * 0.6 ? 'O' : '#') : ' '
    }
    if (y < h - 1) grid += '\n'
  }
  return grid
}

function cellStyle(c, styles, cellSize) {
  const base = styles[c] || { background: '#111' }
  return { ...base, width: cellSize + 'px', height: cellSize + 'px' }
}

describe('Grid 计算性能', () => {

  const grid10 = makeGrid(10, 10)
  const grid20 = makeGrid(20, 20)
  const grid50 = makeGrid(50, 50)

  bench('10x10 grid 拆分为 rows', () => {
    grid10.split('\n').filter(r => r.length > 0)
  })

  bench('20x20 grid 拆分为 rows', () => {
    grid20.split('\n').filter(r => r.length > 0)
  })

  bench('50x50 grid 拆分为 rows', () => {
    grid50.split('\n').filter(r => r.length > 0)
  })

  const rows20 = grid20.split('\n').filter(r => r.length > 0)
  const cells20 = ref(rows20.join('').split(''))
  const cellSize20 = ref(24)

  bench('400 个 cell 样式计算', () => {
    for (let i = 0; i < cells20.value.length; i++)
      cellStyle(cells20.value[i], STYLES, cellSize20.value)
  })

})

describe('Computed 开销', () => {

  bench('ref 读取', () => {
    const r = ref(42)
    for (let i = 0; i < 1000; i++) r.value
  })

  bench('computed 读取 (无依赖变化)', () => {
    const r = ref(42)
    const c = computed(() => r.value * 2)
    for (let i = 0; i < 1000; i++) c.value
  })

  bench('String split + join + split', () => {
    const grid = ref(makeGrid(20, 20))
    for (let i = 0; i < 100; i++) {
      grid.value.split('\n').filter(r => r.length > 0).join('').split('')
    }
  })

})
