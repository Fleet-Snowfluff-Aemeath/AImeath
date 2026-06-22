export const INFO = {
  snake:   { name: 'Snake', clickGame: false },
  gomoku:  { name: 'Gomoku', clickGame: true },
  pacman:  { name: 'Pac-Man', clickGame: false },
  go:      { name: '围棋', size: 19, clickGame: true, isGo: true },
}

export const STYLES = {
  snake: {
    O: { background: '#4CAF50', borderRadius: '4px' },
    '#': { background: '#2E7D32' },
    $: { background: '#f44336', borderRadius: '50%' },
    ' ': { background: '#111' },
  },
  gomoku: {
    '.': { background: '#DEB887' },
    B: { background: '#222', borderRadius: '50%' },
    W: { background: '#fff', borderRadius: '50%', border: '1px solid #888' },
  },
  pacman: {
    '@': { background: '#FFEB3B', borderRadius: '50%' },
    '*': { background: '#fff', borderRadius: '50%' },
    ' ': { background: '#111' },
  },
  go: {
    '.': { background: '#DEB887' },
    B: { background: '#1a1a1a', borderRadius: '50%', boxShadow: '0 0 2px rgba(255,255,255,0.2)' },
    W: { background: '#f5f5f5', borderRadius: '50%', boxShadow: '0 1px 3px rgba(0,0,0,0.4)' },
  },
}

export const GO_ACTIONS = {
  PASS: -1,
  RESIGN: -2,
  CLEAR_DEAD: -3,
  CONFIRM_DEAD: -4,
}

export const KEY_MAP = {
  ArrowUp: 0, ArrowDown: 1, ArrowLeft: 2, ArrowRight: 3,
  w: 0, W: 0, s: 1, S: 1, a: 2, A: 2, d: 3, D: 3,
  Up: 0, Down: 1, Left: 2, Right: 3,
}

export const STATE_FIELDS = ['grid', 'w', 'h', 'score', 'over', 'winner', 'turn', 'passes', 'capsB', 'capsW', 'komi', 'marking', 'deadMask']

export const FIELD_MAP = { over: 'gameOver' }
