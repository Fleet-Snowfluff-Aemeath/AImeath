<template>
  <div class="layout">
    <header class="header">
      <h1 class="header__title">Lab 导航</h1>
      <p class="header__subtitle">C++17 模块化实验室 — 共享库动态加载演示</p>
    </header>
    <main class="main">
      <NavCard
        v-for="cat in categories"
        :key="cat.title"
        :title="cat.title"
        :items="cat.items"
      />
    </main>
    <footer class="footer">
      <code>./build.sh &amp;&amp; ./build/demo</code>
    </footer>
  </div>
</template>

<script setup>
import NavCard from '../components/NavCard.vue'

const categories = [
  {
    title: '游戏模块',
    items: [
      { name: '贪食蛇', url: '/snake', desc: '方向键控制吃豆' },
      { name: '五子棋', url: '/gomoku', desc: '点击落子，15×15' },
      { name: '吃豆豆', url: '/pacman', desc: '方向键控制吃豆' },
      { name: '围棋', url: '/go', desc: '点击落子，19×19' },
    ],
  },
  {
    title: '外部链接',
    items: [
      { name: 'Vue 3', url: 'https://vuejs.org/', desc: '官方文档' },
      { name: 'Vite', url: 'https://vitejs.dev/', desc: '构建工具' },
      { name: 'Google Test', url: 'https://google.github.io/googletest/', desc: '单元测试框架' },
      { name: 'Google Benchmark', url: 'https://github.com/google/benchmark', desc: '性能基准框架' },
      { name: 'Boost', url: 'https://www.boost.org/', desc: 'C++ 库集合' },
    ],
  },
]
</script>

<style scoped>
.layout {
  max-width: 720px;
  margin: 0 auto;
  padding: 40px 20px;
  display: flex;
  flex-direction: column;
  gap: 24px;
}
.header { text-align: center; }
.header__title {
  font-size: 28px;
  font-weight: 700;
  color: #1a1a2e;
}
.header__subtitle {
  margin-top: 6px;
  font-size: 14px;
  color: #6b7280;
}
.main { display: flex; flex-direction: column; gap: 20px; }
.footer {
  text-align: center;
  padding-top: 8px;
  font-size: 13px;
  color: #9ca3af;
}
.footer code {
  background: #e8ecf4;
  padding: 4px 10px;
  border-radius: 4px;
  font-size: 12px;
}
</style>
