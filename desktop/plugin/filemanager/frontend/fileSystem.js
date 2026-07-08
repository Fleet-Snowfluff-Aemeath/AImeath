function formatSize(bytes) {
  if (bytes >= 1e9) return (bytes / 1e9).toFixed(1) + ' GB'
  if (bytes >= 1e6) return (bytes / 1e6).toFixed(1) + ' MB'
  if (bytes >= 1000) return (bytes / 1000).toFixed(0) + ' KB'
  return bytes + ' B'
}

function formatDate(dateStr) {
  if (!dateStr) return ''
  const d = new Date(dateStr)
  const pad = n => String(n).padStart(2, '0')
  return `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`
}

function getIcon(item) {
  if (item.kind === 'dir') return '📁'
  const ext = item.name.split('.').pop().toLowerCase()
  const map = {
    pdf: '📄', txt: '📝', md: '📝', csv: '📊',
    jpg: '🖼️', jpeg: '🖼️', png: '🖼️', gif: '🖼️', webp: '🖼️', svg: '🖼️',
    mp4: '🎬', avi: '🎬', mov: '🎬', mkv: '🎬',
    mp3: '🎵', wav: '🎵', flac: '🎵', m3u: '🎵',
    html: '🌐', css: '🎨', js: '⚡', ts: '⚡',
    cpp: '⚙️', hpp: '⚙️', c: '⚙️', h: '⚙️',
    py: '🐍', java: '☕', rs: '🦀',
    deb: '📦', tar: '📦', gz: '📦', zip: '📦',
    docx: '📋', doc: '📋',
    cmake: '⚙️',
  }
  return map[ext] || '📄'
}

function getMimeCategory(name) {
  const ext = name.split('.').pop().toLowerCase()
  const textExts = new Set([
    'txt','csv','json','xml','yml','yaml','toml','ini','cfg','conf',
    'log','sh','bat','ps1','py','js','ts','jsx','tsx','css','scss','less',
    'html','htm','xhtml','cpp','cxx','cc','c','h','hpp','hxx','hh',
    'java','rs','go','rb','php','pl','lua','sql','r','swift','kt',
    'cmake','mk','makefile','gradle','sln','csproj','vim','lisp','clj',
  ])
  if (textExts.has(ext)) return 'text'

  const imgExts = new Set(['jpg','jpeg','png','gif','bmp','webp','svg','ico','tiff','tif'])
  if (imgExts.has(ext)) return 'image'

  const videoExts = new Set(['mp4','avi','mov','mkv','webm','flv','wmv','m4v','3gp'])
  if (videoExts.has(ext)) return 'video'

  const audioExts = new Set(['mp3','wav','flac','ogg','aac','wma','m4a','opus'])
  if (audioExts.has(ext)) return 'audio'

  if (ext === 'md') return 'markdown'
  if (ext === 'pdf') return 'pdf'

  const binExts = new Set(['doc','docx','xls','xlsx','ppt','pptx',
    'zip','rar','7z','tar','gz','bz2','xz','deb','rpm','iso','dmg',
    'exe','msi','dll','so','dylib','bin','dat','db','sqlite',
    'ttf','otf','woff','woff2','eot',
  ])
  if (binExts.has(ext)) return 'binary'

  return 'unknown'
}

export { formatSize, formatDate, getIcon, getMimeCategory }
