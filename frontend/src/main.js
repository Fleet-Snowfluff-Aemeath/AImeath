import { createApp } from 'vue'
import { createRouter, createWebHashHistory } from 'vue-router'
import App from './App.vue'
import HomePage from './views/HomePage.vue'
import GamePage from './views/GamePage.vue'
import { INFO } from './config/games.js'

const routes = [
  { path: '/', component: HomePage },
  ...Object.keys(INFO).map(key => ({
    path: `/${key}`,
    component: GamePage,
    props: { gameType: key },
  })),
]

const router = createRouter({
  history: createWebHashHistory(),
  routes,
})

createApp(App).use(router).mount('#app')
