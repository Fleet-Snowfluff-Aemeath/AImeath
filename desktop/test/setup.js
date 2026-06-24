HTMLCanvasElement.prototype.getContext = () => ({
  createLinearGradient: () => ({ addColorStop: () => {} }),
  fillRect: () => {}, fillText: () => {}, measureText: () => ({ width: 0 }),
  clearRect: () => {}, beginPath: () => {}, rect: () => {}, clip: () => {},
  save: () => {}, restore: () => {}, scale: () => {}, translate: () => {},
  setTransform: () => {}, drawImage: () => {}, putImageData: () => {},
  getImageData: () => ({ data: new Uint8ClampedArray(4) }),
  createImageData: () => ({ data: new Uint8ClampedArray(4) }),
  isPointInPath: () => false,
})
globalThis.matchMedia = () => ({
  matches: false,
  addListener: () => {},
  removeListener: () => {},
  addEventListener: () => {},
  removeEventListener: () => {},
})
