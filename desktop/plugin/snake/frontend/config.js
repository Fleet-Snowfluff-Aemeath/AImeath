export const info = {
  name: '贪食蛇', clickGame: false,
  icon: `<svg viewBox="0 0 48 48" width="36" height="36">
    <rect x="4" y="4" width="40" height="40" rx="4" fill="#1a1a2e" opacity="0.8"/>
    <circle cx="16" cy="16" r="4" fill="#4ade80"/><circle cx="24" cy="16" r="4" fill="#4ade80"/>
    <circle cx="32" cy="16" r="4" fill="#4ade80"/><circle cx="32" cy="24" r="4" fill="#4ade80"/>
    <circle cx="32" cy="32" r="4" fill="#4ade80"/><circle cx="24" cy="32" r="4" fill="#4ade80"/>
    <circle cx="16" cy="32" r="4" fill="#22c55e"/><circle cx="16" cy="24" r="4" fill="#22c55e"/>
    <rect x="13" y="21" width="6" height="6" rx="1" fill="#fff"/>
    <rect x="29" y="13" width="6" height="6" rx="1" fill="#fff"/>
  </svg>`,
}

export const styles = {
  O: { background: '#4CAF50', borderRadius: '4px' },
  '#': { background: '#2E7D32' },
  $: { background: '#f44336', borderRadius: '50%' },
  ' ': { background: '#111' },
}

export const meta = { size: 20 }
