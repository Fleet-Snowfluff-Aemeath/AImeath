<template>
  <div class="fm-page">
    <div class="fm-header">
      <svg viewBox="0 0 48 48" width="24" height="24"><rect x="4" y="10" width="40" height="30" rx="2" fill="#FFB300" opacity="0.9"/><polygon points="4,10 18,10 22,6 30,6 34,10 44,10 44,14 4,14" fill="#FFA000"/></svg>
      <span class="fm-title">文件管理器</span>
      <span v-if="loading" class="fm-loading">加载中...</span>
      <span v-if="error" class="fm-error">{{ error }}</span>
    </div>
    <div class="fm-toolbar">
      <div class="fm-toolbar-left">
        <button class="fm-btn" @click="goBack" :disabled="historyIdx <= 0" title="后退">◀</button>
        <button class="fm-btn" @click="goForward" :disabled="historyIdx >= history.length - 1" title="前进">▶</button>
        <button class="fm-btn" @click="goUp" :disabled="curPath === '/' || curPath === '/home'" title="上一级">⬆</button>
      </div>
      <div class="fm-pathbar">
        <span v-for="(seg, i) in breadcrumbs" :key="i"
              class="fm-pathseg" :class="{ active: i === breadcrumbs.length - 1 }"
              @click="navigate(seg.path)">{{ seg.name }}</span>
      </div>
      <div class="fm-toolbar-right">
        <input class="fm-search" v-model="searchText" placeholder="搜索..." />
      </div>
    </div>
    <div class="fm-content" @click="selected = null">
      <div v-if="loading && !items.length" class="fm-empty">
        <span class="fm-empty-icon">⏳</span>
        <span>加载中...</span>
      </div>
      <div v-else-if="!items.length" class="fm-empty">
        <span class="fm-empty-icon">📂</span>
        <span>此文件夹为空</span>
      </div>
      <div v-else class="fm-grid">
        <div v-for="(item, i) in filteredItems" :key="i"
             class="fm-item" :class="{ selected: selected === item }"
             @click.stop="selected = item"
             @dblclick="openItem(item)">
          <span class="fm-item-icon">{{ getIcon(item) }}</span>
          <span class="fm-item-name">{{ item.name }}</span>
        </div>
      </div>
    </div>
    <div class="fm-status">
      <span v-if="connected" class="status-ok">● 已连接</span>
      <span v-else class="status-err">● 未连接</span>
      <span v-if="searchText && filteredItems.length !== items.length">
        | 找到 {{ filteredItems.length }} / {{ items.length }} 项
      </span>
      <span v-else>
        | {{ items.length }} 项
      </span>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onBeforeUnmount } from 'vue'
import { formatSize, formatDate, getIcon, getMimeCategory } from './fileSystem.js'
import { createFilemgrChannel } from './filemgrChannel.js'

const curPath = ref('/home')
const history = ref(['/home'])
const historyIdx = ref(0)
const searchText = ref('')
const selected = ref(null)
const items = ref([])
const loading = ref(false)
const error = ref('')
const connected = ref(false)

let ch = null

const breadcrumbs = computed(() => {
  const parts = curPath.value.split('/').filter(Boolean)
  const crumbs = []
  for (let i = 0; i < parts.length; i++) {
    crumbs.push({ name: parts[i], path: '/' + parts.slice(0, i + 1).join('/') })
  }
  return crumbs
})

const filteredItems = computed(() => {
  const q = searchText.value.toLowerCase()
  if (!q) return items.value
  return items.value.filter(item => item.name.toLowerCase().includes(q))
})

async function loadDir(path) {
  if (!ch) return
  loading.value = true
  error.value = ''
  try {
    items.value = await ch.listDir(path)
  } catch (e) {
    error.value = e.message || '加载失败'
    items.value = []
  } finally {
    loading.value = false
  }
}

async function navigate(path) {
  curPath.value = path
  selected.value = null
  history.value = history.value.slice(0, historyIdx.value + 1)
  history.value.push(path)
  historyIdx.value = history.value.length - 1
  await loadDir(path)
}

function goBack() {
  if (historyIdx.value <= 0) return
  historyIdx.value--
  curPath.value = history.value[historyIdx.value]
  selected.value = null
  loadDir(curPath.value)
}

function goForward() {
  if (historyIdx.value >= history.length - 1) return
  historyIdx.value++
  curPath.value = history.value[historyIdx.value]
  selected.value = null
  loadDir(curPath.value)
}

function goUp() {
  const parts = curPath.value.split('/').filter(Boolean)
  if (!parts.length) return
  parts.pop()
  const parent = parts.length ? '/' + parts.join('/') : '/home'
  if (parent === curPath.value) return
  navigate(parent)
}

function openItem(item) {
  if (item.kind === 'dir') {
    const parts = curPath.value.split('/').filter(Boolean)
    parts.push(item.name)
    navigate('/' + parts.join('/'))
    return
  }
  const kind = getMimeCategory(item.name)
  window.parent.postMessage({
    type: 'openFile',
    url: item.url,
    name: item.name,
    kind,
  }, '*')
}

onMounted(() => {
  ch = createFilemgrChannel()
  ch.onOpen(() => { connected.value = true; loadDir('/home') })
  ch.onError(() => { connected.value = false })
})

onBeforeUnmount(() => {
  if (ch) ch.close()
})
</script>

<style scoped>
.fm-page {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: #1e1e2e;
  color: #cdd6f4;
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  user-select: none;
}
.fm-header {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 12px 20px;
  background: #181825;
  border-bottom: 1px solid #313244;
  font-size: 15px;
  font-weight: 600;
}
.fm-title { color: #cdd6f4; }
.fm-loading { font-size: 12px; color: #f9e2af; font-weight: 400; margin-left: auto; }
.fm-error  { font-size: 12px; color: #f38ba8; font-weight: 400; margin-left: auto; }
.fm-toolbar {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 16px;
  background: #1e1e2e;
  border-bottom: 1px solid #313244;
  flex-wrap: wrap;
}
.fm-toolbar-left, .fm-toolbar-right {
  display: flex;
  align-items: center;
  gap: 4px;
}
.fm-btn {
  background: #313244;
  border: none;
  color: #cdd6f4;
  width: 32px;
  height: 32px;
  border-radius: 6px;
  cursor: pointer;
  font-size: 13px;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: background 0.15s;
}
.fm-btn:hover:not(:disabled) { background: #45475a; }
.fm-btn:disabled { opacity: 0.35; cursor: default; }
.fm-pathbar {
  flex: 1;
  display: flex;
  align-items: center;
  gap: 2px;
  padding: 4px 10px;
  background: #181825;
  border-radius: 6px;
  min-height: 34px;
  overflow-x: auto;
  white-space: nowrap;
  min-width: 0;
}
.fm-pathseg {
  padding: 2px 8px;
  border-radius: 4px;
  cursor: pointer;
  font-size: 13px;
  color: #a6adc8;
  transition: background 0.15s;
}
.fm-pathseg:hover { background: #313244; color: #cdd6f4; }
.fm-pathseg.active { color: #89b4fa; font-weight: 500; }
.fm-pathseg:not(:last-child)::after {
  content: '›';
  margin-left: 6px;
  color: #6c7086;
}
.fm-search {
  background: #313244;
  border: 1px solid #45475a;
  border-radius: 6px;
  padding: 6px 12px;
  color: #cdd6f4;
  font-size: 13px;
  outline: none;
  width: 140px;
  transition: border-color 0.15s;
}
.fm-search:focus { border-color: #89b4fa; }
.fm-search::placeholder { color: #6c7086; }
.fm-content {
  flex: 1;
  padding: 16px;
  overflow-y: auto;
}
.fm-empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 80px 20px;
  color: #6c7086;
  gap: 12px;
  font-size: 14px;
}
.fm-empty-icon { font-size: 48px; }
.fm-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(100px, 1fr));
  gap: 8px;
}
.fm-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
  padding: 10px 6px 8px;
  border-radius: 8px;
  cursor: pointer;
  transition: background 0.15s;
  text-align: center;
}
.fm-item:hover { background: #313244; }
.fm-item.selected { background: #45475a; outline: 1px solid #89b4fa; }
.fm-item-icon { font-size: 36px; line-height: 1; }
.fm-item-name {
  font-size: 12px;
  line-height: 1.3;
  word-break: break-all;
  overflow: hidden;
  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
}
.fm-status {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 20px;
  background: #181825;
  border-top: 1px solid #313244;
  font-size: 12px;
  color: #6c7086;
}
.status-ok  { color: #a6e3a1; }
.status-err { color: #f38ba8; }
</style>
