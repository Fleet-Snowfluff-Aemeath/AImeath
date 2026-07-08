import { bench, describe } from 'vitest'
import { ref } from 'vue'
import { styles } from '../config.js'

function makeGrid(w, h, chars) {
  let g = ''; for (let y = 0; y < h; y++) { for (let x = 0; x < w; x++) g += chars[Math.floor(Math.random() * chars.length)]; if (y < h - 1) g += '\n' }
  return g
}
function cellStyle(c, s, cs) { const b = s[c] || { background: '#111' }; return { ...b, width: cs + 'px', height: cs + 'px' } }

describe('Pacman 网格性能', () => {
  const g20 = makeGrid(20, 20, ['@', '*', ' '])
  const cells20 = ref(g20.split('\n').filter(r => r.length > 0).join('').split(''))

  bench('20x20 cell 样式 (400 cells)', () => {
    for (let i = 0; i < cells20.value.length; i++) cellStyle(cells20.value[i], styles, 24)
  })

  bench('50x50 cell 样式 (2500 cells)', () => {
    const g50 = makeGrid(50, 50, ['@', '*', ' '])
    const c = g50.split('\n').filter(r => r.length > 0).join('').split('')
    for (let i = 0; i < c.length; i++) cellStyle(c[i], styles, 14)
  })
})
