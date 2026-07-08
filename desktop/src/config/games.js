const configModules = import.meta.glob('@plugins/*/frontend/config.js', { eager: true })
const pageModules = import.meta.glob('@plugins/*/frontend/index.vue', { eager: true })

export const APPS = {}

for (const path in configModules) {
  const segs = path.replace(/\\/g, '/').split('/')
  const idx = segs.indexOf('plugin')
  const name = idx >= 0 && idx + 1 < segs.length ? segs[idx + 1] : null
  if (!name) continue
  const config = configModules[path]
  const pagePath = path.replace(/config\.js$/, 'index.vue')
  const pageModule = pageModules[pagePath]
  if (pageModule) {
    APPS[name] = {
      info: config.info,
      styles: config.styles,
      meta: config.meta,
      page: pageModule.default,
    }
  }
}

export const INFO = Object.fromEntries(
  Object.entries(APPS).map(([k, v]) => [k, v.info])
)

export const STYLES = Object.fromEntries(
  Object.entries(APPS).map(([k, v]) => [k, v.styles])
)

export const META = Object.fromEntries(
  Object.entries(APPS).map(([k, v]) => [k, v.meta])
)

export const GO_ACTIONS = {
  PASS: -1, RESIGN: -2, CLEAR_DEAD: -3, CONFIRM_DEAD: -4,
}

export const KEY_MAP = {
  ArrowUp: 0, ArrowDown: 1, ArrowLeft: 2, ArrowRight: 3,
  w: 0, W: 0, s: 1, S: 1, a: 2, A: 2, d: 3, D: 3,
  Up: 0, Down: 1, Left: 2, Right: 3,
}

export const STATE_FIELDS = ['grid', 'w', 'h', 'score', 'over', 'winner', 'turn', 'passes', 'capsB', 'capsW', 'komi', 'marking', 'deadMask']

export const FIELD_MAP = { over: 'gameOver' }
