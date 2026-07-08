<template>
  <div class="game-page">
    <h1>{{ gameName }}</h1>
    <div class="controls">
      <button @click="startGame" :disabled="!!socket">Start</button>
      <button @click="togglePause" :disabled="!socket || gameState.gameOver">{{ paused ? 'Resume' : 'Pause' }}</button>
      <button @click="endGame" :disabled="!socket">End</button>
      <span class="score" v-if="gameState.score >= 0">Score: {{ gameState.score }}</span>
    </div>
    <div class="error-msg" v-if="errorMsg">{{ errorMsg }}</div>
    <div class="error-msg" v-else-if="reconnecting">重连中 ({{ reconnecting }}/5)...</div>
    <div class="status" v-else-if="socket && !rows.length && !gameState.gameOver">连接中...</div>
    <div class="grid-wrapper" v-if="rows.length">
      <div class="grid" :style="gridStyle">
        <div v-for="(cell,i) in cells" :key="i" class="cell" :style="cellStyle(cell)"></div>
      </div>
    </div>
    <div class="status" v-if="gameState.gameOver">Game Over! Score: {{ gameState.score }}</div>
    <div class="key-hint">Use arrow keys or WASD to move</div>
  </div>
</template>

<script setup>
import { useGame } from '../../../src/composables/useGame.js'
import { info } from './config.js'

const props = defineProps({ gameType: { type: String, required: true } })

const {
  socket, errorMsg, reconnecting, paused, gameState,
  gameName, rows, cells, gridStyle,
  startGame, endGame, togglePause, cellStyle,
} = useGame(props.gameType)
</script>

<style scoped>
.game-page { text-align: center; font-family: monospace; background: #1a1a2e; color: #eee; min-height: 100vh; padding: 20px; }
h1 { margin: 0 0 16px; font-size: 24px; }
.controls { margin-bottom: 16px; display: flex; gap: 10px; justify-content: center; align-items: center; flex-wrap: wrap; }
.controls button { padding: 8px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; background: #4CAF50; color: #fff; }
.controls button:disabled { opacity: 0.4; cursor: default; }
.controls button:not(:disabled):hover { opacity: 0.8; }
.score { font-size: 18px; margin-left: 10px; }
.error-msg { margin-bottom: 12px; padding: 8px 16px; background: #f44336; color: #fff; border-radius: 4px; display: inline-block; font-size: 14px; }
.grid-wrapper { display: inline-block; padding: 10px; background: #222; border-radius: 8px; max-width: 100%; overflow-x: auto; }
.grid { display: inline-grid; gap: 1px; }
.cell { width: 24px; height: 24px; }
.status { margin-top: 16px; font-size: 20px; color: #FF9800; font-weight: bold; }
.key-hint { margin-top: 12px; color: #888; font-size: 12px; }
</style>
