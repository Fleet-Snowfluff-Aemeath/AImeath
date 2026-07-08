import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { resolve, dirname } from 'path'
import { fileURLToPath } from 'url'

const __dirname = dirname(fileURLToPath(import.meta.url))

const backendPort = process.env.VITE_BACKEND_PORT || '3001'

export default defineConfig({
  plugins: [vue()],
  cacheDir: 'node_modules/.vite',
  resolve: {
    alias: {
      '@plugins': resolve(__dirname, 'plugin'),
    },
  },
  server: {
    host: '0.0.0.0',
    port: 5173,
    proxy: {
      '/api/config': {
        target: `http://127.0.0.1:${backendPort}`,
        changeOrigin: true,
        rewrite: () => '/',
      },
    },
  },
  test: {
    environment: 'jsdom',
    globals: true,
    setupFiles: ['./test/setup.js'],
  },
})
