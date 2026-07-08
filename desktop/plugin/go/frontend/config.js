export const info = {
  name: '围棋', size: 19, clickGame: true, isGo: true,
  icon: `<svg viewBox="0 0 48 48" width="36" height="36">
    <rect x="4" y="4" width="40" height="40" rx="4" fill="#1a1a2e" opacity="0.8"/>
    <rect x="8" y="8" width="32" height="32" rx="2" fill="#d97706"/>
    <line x1="12" y1="16" x2="36" y2="16" stroke="#92400e" stroke-width="0.5"/>
    <line x1="12" y1="24" x2="36" y2="24" stroke="#92400e" stroke-width="0.5"/>
    <line x1="12" y1="32" x2="36" y2="32" stroke="#92400e" stroke-width="0.5"/>
    <line x1="16" y1="12" x2="16" y2="36" stroke="#92400e" stroke-width="0.5"/>
    <line x1="24" y1="12" x2="24" y2="36" stroke="#92400e" stroke-width="0.5"/>
    <line x1="32" y1="12" x2="32" y2="36" stroke="#92400e" stroke-width="0.5"/>
    <circle cx="16" cy="16" r="3" fill="#1f2937"/><circle cx="24" cy="16" r="3" fill="#fafafa"/>
    <circle cx="24" cy="24" r="3" fill="#1f2937"/><circle cx="32" cy="24" r="3" fill="#fafafa"/>
    <circle cx="16" cy="32" r="3" fill="#fafafa"/>
  </svg>`,
}

export const styles = {
  '.': { background: '#DEB887' },
  B: { background: '#1a1a1a', borderRadius: '50%', boxShadow: '0 0 2px rgba(255,255,255,0.2)' },
  W: { background: '#f5f5f5', borderRadius: '50%', boxShadow: '0 1px 3px rgba(0,0,0,0.4)' },
}

export const meta = { size: 19 }
