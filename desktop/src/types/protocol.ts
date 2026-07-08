export interface WsRequest {
  action: string
  plugin?: string
  game?: string
  width?: number
  height?: number
  value?: number
  text?: string
  cols?: number
  rows?: number
  cmd?: string
  path?: string
}

export interface WsError {
  type: 'error'
  msg: string
}

// ---- Game messages ----

export interface GameState {
  grid?: number[]
  w: number
  h: number
  score?: number
  over: boolean
  winner?: number
  turn?: number
  passes?: number
  capsB?: number
  capsW?: number
  komi?: number
  marking?: number[]
  deadMask?: number[]
}

export type GameAction = 'new_game' | 'tick' | 'pause' | 'resume' | 'end_game'

export interface GameRequest extends WsRequest {
  action: GameAction
  game: string
  width: number
  height: number
}

// ---- Chat messages ----

export interface ChatRequest extends WsRequest {
  text: string
}

export interface StreamStart {
  type: 'stream_start'
}

export interface Delta {
  type: 'delta'
  text: string
}

export interface Reasoning {
  type: 'reasoning'
  text: string
}

export interface StreamEnd {
  type: 'stream_end'
}

export interface Embed {
  type: 'embed'
  data: string
}

export type ChatResponse = StreamStart | Delta | Reasoning | StreamEnd | Embed | WsError

// ---- Terminal messages ----

export interface TerminalExec {
  action: 'exec'
  cmd?: string
}

export interface TerminalStdin {
  action: 'stdin'
  text: string
}

export interface TerminalResize {
  action: 'resize'
  cols: number
  rows: number
}

export type TerminalRequest = WsRequest & (TerminalExec | TerminalStdin | TerminalResize)

export interface TerminalOutput {
  type: 'output'
  text: string
}

// ---- File manager messages ----

export interface FileListRequest {
  action: 'list'
  path: string
}

export interface FileReadRequest {
  action: 'read'
  path: string
}

export interface FileWriteRequest {
  action: 'write'
  path: string
  content: string
}

export interface FileMkdirRequest {
  action: 'mkdir'
  path: string
}

export interface FileRemoveRequest {
  action: 'remove'
  path: string
}

export type FileManagerRequest = WsRequest & (FileListRequest | FileReadRequest | FileWriteRequest | FileMkdirRequest | FileRemoveRequest)

export interface FileListing {
  type: 'listing'
  entries: Array<{ name: string; isDir: boolean; size: number }>
}

export interface FileContent {
  type: 'file'
  path: string
  content: string
}

export type FileManagerResponse = FileListing | FileContent | WsError
