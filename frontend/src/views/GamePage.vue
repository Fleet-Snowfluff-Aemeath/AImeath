<template>
  <div class="game-page">
    <h1>{{ gameName }}</h1>
    <div class="controls">
      <button @click="startGame" :disabled="!!socket">Start</button>
      <button @click="togglePause" :disabled="!socket || gameState.gameOver">{{ paused ? 'Resume' : 'Pause' }}</button>
      <button @click="endGame" :disabled="!socket">End</button>
      <template v-if="isGoGame">
        <button @click="goPass" :disabled="!socket || gameState.gameOver">虚着</button>
        <button @click="goResign" :disabled="!socket || gameState.gameOver">认输</button>
      </template>
      <span class="score" v-if="gameState.score >= 0 && !gameState.marking">Score: {{ gameState.score }}</span>
      <span class="score marking-hint" v-if="gameState.marking">标记死子中</span>
    </div>
    <div class="go-info" v-if="isGoGame && socket">
      <span>回合: <b :class="turnClass">{{ turnLabel }}</b></span>
      <span>虚着: {{ gameState.passes }}</span>
      <span>提子: B{{ gameState.capsB }} W{{ gameState.capsW }}</span>
      <span>贴目: {{ gameState.komi }}</span>
      <span v-if="gameState.marking" class="marking-ctrl">
        <button @click="clearDeadMarks">清除标记</button>
        <button @click="confirmDead" class="confirm-btn">确认死子</button>
      </span>
    </div>
    <div class="error-msg" v-if="errorMsg">{{ errorMsg }}</div>
    <div class="error-msg" v-else-if="reconnecting">重连中 ({{ reconnecting }}/5)...</div>
    <div class="status" v-else-if="socket && !rows.length && !gameState.gameOver">连接中...</div>
    <div class="grid-wrapper" v-if="rows.length">
      <div class="grid" :style="gridStyle" @click="handleGridClick">
        <div
          v-for="(cell,i) in cells" :key="i"
          class="cell"
          :class="cellClass(i)"
          :style="cellStyle(cell)"
          :title="isClickGame ? `(${Math.floor(i / gridW)}, ${i % gridW})` : ''"
        ></div>
      </div>
    </div>
    <div class="status" v-if="gameState.gameOver && !gameState.marking && isClickGame">{{ winnerText }}</div>
    <div class="status" v-if="gameState.gameOver && !isClickGame">Game Over! Score: {{ gameState.score }}</div>
    <div class="key-hint" v-if="!isClickGame">Use arrow keys or WASD to move</div>
    <div class="key-hint" v-if="isGoGame && !gameState.marking">Click to place · P 虚着 R 认输</div>
    <div class="key-hint" v-if="isGoGame && gameState.marking">点击棋子标记死子 → 确认死子</div>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted, onBeforeUnmount } from 'vue'
import { createGameSocket } from '../services/gameSocket.js'
import { INFO, STYLES, GO_ACTIONS, KEY_MAP, STATE_FIELDS, FIELD_MAP } from '../config/games.js'

const GRID_GAP = 1

const props = defineProps({
  gameType: { type: String, required: true },
})

const socket = ref(null)
const errorMsg = ref('')
const reconnecting = ref(0)
const paused = ref(false)
const gameState = reactive({
  grid: '',
  w: 0, h: 0,
  score: -1,
  gameOver: false,
  winner: 0,
  turn: 'B',
  passes: 0,
  capsB: 0, capsW: 0,
  komi: 3.75,
  marking: false,
  deadMask: '',
})

const meta = computed(() => INFO[props.gameType] || {})
const gameName = computed(() => meta.value.name || props.gameType)
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

function resetState() {
  errorMsg.value = ''
  reconnecting.value = 0
  Object.assign(gameState, {
    grid: '', w: 0, h: 0,
    score: -1, gameOver: false, winner: 0,
    turn: 'B', passes: 0, capsB: 0, capsW: 0,
    komi: 3.75, marking: false, deadMask: '',
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

function startGame() {
  if (socket.value) return
  resetState()
  const size = INFO[props.gameType]?.size || 20
  socket.value = createGameSocket(props.gameType, size, size)
  socket.value.onState(handleState)
  window.addEventListener('keydown', handleKey)
}

function endGame() {
  if (socket.value) { socket.value.endGame(); socket.value = null }
  window.removeEventListener('keydown', handleKey)
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
  const base = STYLES[props.gameType]?.[c] || { background: '#111' }
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
</script>

<style scoped>
.game-page {
  text-align: center;
  font-family: monospace;
  background: #1a1a2e;
  color: #eee;
  min-height: 100vh;
  padding: 20px;
}
h1 { margin: 0 0 16px; font-size: 24px; }
.controls {
  margin-bottom: 16px; display: flex; gap: 10px;
  justify-content: center; align-items: center; flex-wrap: wrap;
}
.controls button {
  padding: 8px 20px; border: none; border-radius: 4px;
  cursor: pointer; font-size: 14px; background: #4CAF50; color: #fff;
}
.controls button:disabled { opacity: 0.4; cursor: default; }
.controls button:not(:disabled):hover { opacity: 0.8; }
.score { font-size: 18px; margin-left: 10px; }
.error-msg {
  margin-bottom: 12px; padding: 8px 16px;
  background: #f44336; color: #fff; border-radius: 4px;
  display: inline-block; font-size: 14px;
}
.go-info {
  margin-bottom: 16px; display: flex; gap: 20px;
  justify-content: center; font-size: 14px; color: #ccc; flex-wrap: wrap;
}
.go-info span { white-space: nowrap; }
.turn-black { color: #bbb; } .turn-white { color: #fff; }
.grid-wrapper {
  display: inline-block; padding: 10px;
  background: #222; border-radius: 8px; max-width: 100%; overflow-x: auto;
}
.grid { display: inline-grid; gap: 1px; cursor: pointer; }
.cell-dead {
  opacity: 0.35; outline: 2px dashed #f44336; outline-offset: -2px;
}
.marking-hint { color: #f44336; }
.marking-ctrl { display: flex; gap: 6px; }
.marking-ctrl button {
  padding: 4px 12px; border: none; border-radius: 4px;
  cursor: pointer; font-size: 13px; background: #555; color: #fff;
}
.marking-ctrl .confirm-btn { background: #f44336; }
.status {
  margin-top: 16px; font-size: 20px; color: #FF9800; font-weight: bold;
}
.key-hint { margin-top: 12px; color: #888; font-size: 12px; }
</style>
