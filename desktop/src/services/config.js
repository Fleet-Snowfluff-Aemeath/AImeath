const DEFAULT_PORT = 3001
const FETCH_RETRIES = 5
const FETCH_RETRY_DELAY_MS = 1000

let cachedPort = null
let fetchDone = false

async function fetchPort() {
  const hostname = location.hostname
  const fetchUrl = '/api/config'
  console.log(`[config] hostname="${hostname}", defaultPort=${DEFAULT_PORT}, fetchUrl="${fetchUrl}" (via Vite proxy)`)

  for (let attempt = 1; attempt <= FETCH_RETRIES; attempt++) {
    try {
      console.log(`[config] fetching ${fetchUrl} (attempt ${attempt}/${FETCH_RETRIES})...`)
      const res = await fetch(fetchUrl)
      const data = await res.json()
      console.log(`[config] response status=${res.status}, body=`, data)

      if (data && typeof data.port === 'number') {
        cachedPort = data.port
        fetchDone = true
        console.log(`[config] RESOLVED: backend port = ${cachedPort}`)
        return
      }
      console.warn(`[config] response missing "port" field, will retry`)
    } catch (err) {
      console.warn(`[config] fetch attempt ${attempt}/${FETCH_RETRIES} failed:`, err.message || err)
    }

    if (attempt < FETCH_RETRIES) {
      await new Promise(r => setTimeout(r, FETCH_RETRY_DELAY_MS))
    }
  }

  cachedPort = DEFAULT_PORT
  fetchDone = true
  console.warn(`[config] all ${FETCH_RETRIES} fetch attempts failed, falling back to default port ${DEFAULT_PORT}`)
  console.warn(`[config] check that gameserver is running: LD_LIBRARY_PATH=build/output/lib ./build/output/gameserver`)
  console.warn(`[config] if using WSL, try accessing frontend at http://<WSL-IP>:5173 instead of localhost`)
}

const portPromise = fetchPort().catch(() => {
  cachedPort = DEFAULT_PORT
  fetchDone = true
})

export function getWsUrl(path = '') {
  const port = cachedPort || DEFAULT_PORT
  const url = `ws://${location.hostname}:${port}${path}`
  if (!fetchDone) {
    console.log(`[config] getWsUrl("${path}") -> "${url}" (fetch not yet complete, using ${cachedPort ? 'cached ' + cachedPort : 'default ' + DEFAULT_PORT})`)
  } else {
    console.log(`[config] getWsUrl("${path}") -> "${url}"`)
  }
  return url
}

export async function ensureConfig() {
  await portPromise
  return cachedPort
}

export function getPort() {
  return cachedPort || DEFAULT_PORT
}
