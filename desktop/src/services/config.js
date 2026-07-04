const DEFAULT_PORT = 3001

const envPort = parseInt(import.meta.env.VITE_BACKEND_PORT)
let cachedPort = !isNaN(envPort) ? envPort : null

export function getWsUrl(path = '') {
  const port = cachedPort || DEFAULT_PORT
  return `ws://${location.hostname}:${port}${path}`
}

export function getPort() {
  return cachedPort || DEFAULT_PORT
}

async function fetchPort() {
  try {
    const res = await fetch('/api/config')
    const data = await res.json()
    if (data && typeof data.port === 'number') {
      cachedPort = data.port
      console.log(`[config] port from backend: ${cachedPort}`)
    }
  } catch (_) {}
}

if (!cachedPort) {
  fetchPort().catch(() => {})
}
