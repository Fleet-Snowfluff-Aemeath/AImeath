import { createPlugin } from 'vue'
import { createRouter, createWebHashHistory } from 'vue-router'
import Plugin from './App.vue'
import HomePage from './views/HomePage.vue'
import FileViewer from './views/FileViewer.vue'
import { APPS } from './config/games.js'

const routes = [
  { path: '/', component: HomePage },
  { path: '/view', component: FileViewer },
  ...Object.keys(APPS).map(key => ({
    path: `/${key}`,
    component: APPS[key].page,
    props: { gameType: key },
  })),
]

const router = createRouter({
  history: createWebHashHistory(),
  routes,
})

createPlugin(Plugin).use(router).mount('#plugin')
