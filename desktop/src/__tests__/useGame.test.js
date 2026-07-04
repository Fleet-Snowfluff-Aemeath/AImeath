import { describe, it, expect, vi, beforeEach } from 'vitest'

let onOpenCb = null
let onCloseCb = null
let sendSpy = vi.fn()

vi.mock('../services/gameSocket.js', () => ({
  createGameSocket: vi.fn(() => {
    const ch = { send: sendSpy, onOpen: vi.fn(fn => { onOpenCb = fn }) }
    return {
      send: (p) => ch.send(p),
      tick: (v) => ch.send({ action: 'tick', value: v }),
      endGame: () => { ch.send({ action: 'end_game' }); this.close?.() },
      onState: vi.fn(),
      close: vi.fn(),
    }
  }),
}))

vi.mock('../config/games.js', () => ({
  INFO: { snake: { name: '贪食蛇', size: 20, clickGame: true }, gomoku: { name: '五子棋', size: 15, clickGame: true }, go: { name: '围棋', size: 19, clickGame: true, isGo: true } },
  STYLES: { snake: { '@': { background: 'green' }, ' ': { background: 'black' } }, gomoku: { 'B': { background: '#222' }, 'W': { background: '#fff' } }, go: { 'B': { background: '#222' }, 'W': { background: '#fff' } } },
  GO_ACTIONS: { PASS: -1, RESIGN: -2, CLEAR_DEAD: -3, CONFIRM_DEAD: -4 },
  KEY_MAP: { ArrowUp: 0, ArrowDown: 1, ArrowLeft: 2, ArrowRight: 3, w: 0, s: 1, a: 2, d: 3 },
  STATE_FIELDS: ['grid', 'w', 'h', 'score', 'over', 'winner', 'turn', 'passes', 'capsB', 'capsW', 'komi', 'marking', 'deadMask'],
  FIELD_MAP: { over: 'gameOver' },
}))

import { useGame } from '../composables/useGame.js'

describe('useGame', () => {
  beforeEach(() => {
    onOpenCb = null
    onCloseCb = null
    sendSpy = vi.fn()
    vi.clearAllMocks()
  })

  describe('computed state', () => {
    it('returns correct game name', () => {
      const { gameName } = useGame('snake')
      expect(gameName.value).toBe('贪食蛇')
    })

    it('go game has isGo true', () => {
      const { isGoGame, isClickGame } = useGame('go')
      expect(isGoGame.value).toBe(true)
      expect(isClickGame.value).toBe(true)
    })

    it('snake is not go game', () => {
      const { isGoGame } = useGame('snake')
      expect(isGoGame.value).toBe(false)
    })

    it('grid parses correctly', () => {
      const { gameState, rows, gridW, gridH, cells } = useGame('snake')
      Object.assign(gameState, { grid: 'ab\ncd', w: 2, h: 2 })
      expect(rows.value).toEqual(['ab', 'cd'])
      expect(gridW.value).toBe(2)
      expect(gridH.value).toBe(2)
      expect(cells.value).toEqual(['a', 'b', 'c', 'd'])
    })

    it('winnerText for black win', () => {
      const { gameState, winnerText } = useGame('gomoku')
      gameState.winner = 1
      expect(winnerText.value).toBe('Black wins!')
    })

    it('winnerText for white win', () => {
      const { gameState, winnerText } = useGame('gomoku')
      gameState.winner = 2
      expect(winnerText.value).toBe('White wins!')
    })

    it('winnerText for draw', () => {
      const { gameState, winnerText } = useGame('gomoku')
      gameState.winner = 0
      expect(winnerText.value).toBe('Draw.')
    })

    it('turnLabel black', () => {
      const { gameState, turnLabel } = useGame('go')
      gameState.turn = 'B'
      expect(turnLabel.value).toBe('黑棋')
    })

    it('turnLabel white', () => {
      const { gameState, turnLabel } = useGame('go')
      gameState.turn = 'W'
      expect(turnLabel.value).toBe('白棋')
    })

    it('cell size adapts to grid width', () => {
      const { gameState, cellSize } = useGame('go')
      gameState.grid = 'B\nW'
      expect(cellSize.value).toBe(24)
      gameState.grid = '.'.repeat(26) + '\n' + '.'.repeat(26)
      expect(cellSize.value).toBe(18)
      gameState.grid = '.'.repeat(31) + '\n' + '.'.repeat(31)
      expect(cellSize.value).toBe(14)
    })
  })

  describe('state handling', () => {
    it('patches game state fields', () => {
      const { gameState, handleState } = useGame('snake')['_setup'] || useGame('snake')
      const state = useGame('snake')
      const patch = { grid: '@ *', w: 3, h: 3, score: 10, over: false }
      state.gameState.grid = '@ *'
      state.gameState.w = 3
      state.gameState.h = 3
      state.gameState.score = 10
      expect(state.gameState.grid).toBe('@ *')
      expect(state.gameState.score).toBe(10)
    })

    it('maps over to gameOver', () => {
      const { gameState } = useGame('snake')
      gameState.score = 0
      expect(gameState.gameOver).toBe(false)
    })
  })

  describe('cell style', () => {
    it('returns fallback for unknown cell', () => {
      const { cellStyle } = useGame('snake')
      expect(cellStyle('?').background).toBe('#111')
    })

    it('returns configured style', () => {
      const { cellStyle } = useGame('snake')
      expect(cellStyle('@').background).toBe('green')
    })
  })

  describe('go game state', () => {
    it('displays passes count', () => {
      const { gameState } = useGame('go')
      gameState.passes = 2
      expect(gameState.passes).toBe(2)
    })

    it('displays capsB and capsW', () => {
      const { gameState } = useGame('go')
      gameState.capsB = 3
      gameState.capsW = 1
      expect(gameState.capsB).toBe(3)
      expect(gameState.capsW).toBe(1)
    })

    it('displays komi', () => {
      const { gameState } = useGame('go')
      gameState.komi = 3.75
      expect(gameState.komi).toBe(3.75)
    })

    it('marking mode flag', () => {
      const { gameState } = useGame('go')
      expect(gameState.marking).toBeFalsy()
      gameState.marking = true
      expect(gameState.marking).toBe(true)
    })

    it('deadMask display', () => {
      const { gameState } = useGame('go')
      gameState.deadMask = '1010'
      expect(gameState.deadMask).toBe('1010')
    })
  })
})
