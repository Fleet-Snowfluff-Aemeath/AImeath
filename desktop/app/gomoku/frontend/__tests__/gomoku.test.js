import { describe, it, expect, vi } from 'vitest'
import { mount } from '@vue/test-utils'
let onMsgCb = null
vi.mock('../../../../src/services/channel.js', () => ({
  createChannel: vi.fn(() => ({ send:vi.fn(),onOpen:vi.fn(fn=>fn()),onMessage:vi.fn(fn=>{onMsgCb=fn}),onError:vi.fn(),onReconnecting:vi.fn(),onClose:vi.fn(),close:vi.fn() })),
}))
const fire=(d)=>onMsgCb&&onMsgCb(d)
import G from '../index.vue';const m=p=>mount(G,{props:p})
describe('Gomoku',()=>{
  it('名称',()=>{expect(m({gameType:'gomoku'}).find('h1').text()).toBe('五子棋')})
  it('网格',async()=>{const w=m({gameType:'gomoku'});fire({type:'gomoku',grid:'BW\nWB',score:0,over:false});await w.vm.$nextTick();expect(w.find('.grid').exists()).toBe(true)})
  it('错误',async()=>{const w=m({gameType:'gomoku'});fire({type:'error',msg:'err'});await w.vm.$nextTick();expect(w.find('.error-msg').text()).toBe('err')})
})
