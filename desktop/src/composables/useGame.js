import { ref, reactive, computed, onMounted, onBeforeUnmount } from 'vue'
import { createGameSocket } from '../services/gameSocket.js'
import { INFO, STYLES, GO_ACTIONS, KEY_MAP, STATE_FIELDS, FIELD_MAP } from '../config/games.js'

const GRID_GAP = 1

export function useGame(gameType) {
  const socket = ref(null)
  const errorMsg = ref('')
  const reconnecting = ref(0)
  const paused = ref(false)
  const gameState = reactive({
    grid: '', w: 0, h: 0, score: -1,
    gameOver: false, winner: 0, turn: 'B',
    passes: 0, capsB: 0, capsW: 0, komi: 3.75,
    marking: false, deadMask: '',
  })

  const meta = computed(() => INFO[gameType] || {})
  const gameName = computed(() => meta.value.name || gameType)
  const isClickGame = computed(() => !!meta.value.clickGame)
  const isGoGame = computed(() => !!meta.value.isGo)
  const rows = computed(() => gameState.grid.split('\n').filter(r => r.length > 0))
  const gridW = computed(() => rows.value.length ? rows.value[0].length : gameState.w)
  const gridH = computed(() => rows.value.length || gameState.h)
  const cells = computed(() => rows.value.join('').split(''))
  const cellSize = computed(() => {
    const s = gridW.value
    return s > 30 ? 14 : s > 25 ? 18 : 24
  })
  const gridStyle = computed(() => {
    const cs = cellSize.value
    return {
      gridTemplateColumns: `repeat(${gridW.value}, ${cs}px)`,
      gridTemplateRows: `repeat(${gridH.value}, ${cs}px)`,
    }
  })
  const winnerText = computed(() => {
    const w = gameState.winner
    return w === 1 ? 'Black wins!' : w === 2 ? 'White wins!' : 'Draw.'
  })
  const turnLabel = computed(() => gameState.turn === 'B' ? '黑棋' : '白棋')
  const turnClass = computed(() => gameState.turn === 'B' ? 'turn-black' : 'turn-white')

  const styles = computed(() => STYLES[gameType] || {})

  function resetState() {
    errorMsg.value = ''
    reconnecting.value = 0
    Object.assign(gameState, {
      grid: '', w: 0, h: 0, score: -1, gameOver: false, winner: 0,
      turn: 'B', passes: 0, capsB: 0, capsW: 0, komi: 3.75,
      marking: false, deadMask: '',
    })
  }

  function handleState(data) {
    if (data.type === 'error') { errorMsg.value = data.msg || 'Connection error'; return }
    if (data.type === 'closed') { socket.value = null; return }
    if (data.type === 'reconnecting') { reconnecting.value = data.attempt; return }
    reconnecting.value = 0
    if (data.type === 'ok') return
    const patch = {}
    for (const f of STATE_FIELDS) {
      if (data[f] !== undefined) {
        patch[FIELD_MAP[f] || f] = data[f]
      }
    }
    if (data.s) { patch.w = data.s; patch.h = data.s }
    Object.assign(gameState, patch)
  }

  function handleAgentInput(e) {
    if (e.data?.type === 'agent_input') {
      const { action, value } = e.data
      if (!socket.value || gameState.gameOver) return
      if (action === 'tick' && typeof value === 'number') {
        socket.value.tick(value)
      } else if (action === 'pass' && isGoGame.value) {
        goPass()
      } else if (action === 'resign' && isGoGame.value) {
        goResign()
      }
    }
  }

  function startGame() {
    if (socket.value) return
    resetState()
    const size = INFO[gameType]?.size || 20
    socket.value = createGameSocket(gameType, size, size)
    socket.value.onState(handleState)
    window.addEventListener('keydown', handleKey)
    window.addEventListener('message', handleAgentInput)
  }

  function endGame() {
    if (socket.value) { socket.value.endGame(); socket.value = null }
    window.removeEventListener('keydown', handleKey)
    window.removeEventListener('message', handleAgentInput)
    resetState()
  }

  function togglePause() {
    if (!socket.value) return
    socket.value.send({ action: paused.value ? 'resume' : 'pause' })
    paused.value = !paused.value
  }

  function handleKey(e) {
    if (!socket.value || gameState.gameOver || e.repeat) return
    if (isGoGame.value) {
      if (e.key === 'p' || e.key === 'P') { e.preventDefault(); goPass(); return }
      if (e.key === 'r' || e.key === 'R') { e.preventDefault(); goResign(); return }
      return
    }
    const dir = KEY_MAP[e.key]
    if (dir !== undefined) { e.preventDefault(); socket.value.tick(dir) }
  }

  function goPass() { if (socket.value && !gameState.gameOver) socket.value.tick(GO_ACTIONS.PASS) }
  function goResign() { if (socket.value && !gameState.gameOver) socket.value.tick(GO_ACTIONS.RESIGN) }
  function clearDeadMarks() { if (socket.value) socket.value.tick(GO_ACTIONS.CLEAR_DEAD) }
  function confirmDead() { if (socket.value) socket.value.tick(GO_ACTIONS.CONFIRM_DEAD) }

  function handleGridClick(e) {
    if (!isClickGame.value || !socket.value || (gameState.gameOver && !gameState.marking)) return
    const rect = e.currentTarget.getBoundingClientRect()
    const cellPitch = cellSize.value + GRID_GAP
    const row = Math.floor((e.clientY - rect.top) / cellPitch)
    const col = Math.floor((e.clientX - rect.left) / cellPitch)
    if (row >= 0 && row < gridH.value && col >= 0 && col < gridW.value)
      socket.value.tick(row * gridW.value + col)
  }

  function cellStyle(c) {
    const base = styles.value[c] || { background: '#111' }
    return { ...base, width: cellSize.value + 'px', height: cellSize.value + 'px' }
  }

  function cellClass(i) {
    return isGoGame.value && gameState.deadMask[i] === '1' ? 'cell-dead' : ''
  }

  onMounted(startGame)
  onBeforeUnmount(() => {
    if (socket.value) socket.value.close()
    window.removeEventListener('keydown', handleKey)
  })

  return {
    socket, errorMsg, reconnecting, paused, gameState,
    gameName, isClickGame, isGoGame, rows, gridW, gridH, cells,
    cellSize, gridStyle, winnerText, turnLabel, turnClass, styles,
    startGame, endGame, togglePause, handleKey, handleGridClick,
    goPass, goResign, clearDeadMarks, confirmDead,
    cellStyle, cellClass,
  }
}
