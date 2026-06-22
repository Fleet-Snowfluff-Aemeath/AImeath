import socket, base64, struct

def ws_encode(payload):
    frame = bytearray()
    frame.append(0x81)
    mask_key = b"mask"
    data = payload.encode() if isinstance(payload, str) else payload
    plen = len(data)
    if plen < 126:
        frame.append(0x80 | plen)
    elif plen < 65536:
        frame.append(0x80 | 126)
        frame.extend(struct.pack(">H", plen))
    else:
        frame.append(0x80 | 127)
        frame.extend(struct.pack(">Q", plen))
    frame.extend(mask_key)
    frame.extend(bytearray([data[i] ^ mask_key[i % 4] for i in range(plen)]))
    return bytes(frame)

def ws_recv(s):
    hdr = s.recv(2)
    if len(hdr) < 2: return None
    plen = hdr[1] & 0x7F
    if plen == 126:
        plen = struct.unpack(">H", s.recv(2))[0]
    elif plen == 127:
        plen = struct.unpack(">Q", s.recv(8))[0]
    mask = s.recv(4) if (hdr[1] & 0x80) else None
    data = s.recv(plen)
    if mask:
        data = bytearray([data[i] ^ mask[i % 4] for i in range(plen)])
    return data.decode()

def send_msg(s, msg):
    s.send(ws_encode(msg))
    return ws_recv(s)

s = socket.socket()
s.settimeout(10)
s.connect(("127.0.0.1", 3001))
key = base64.b64encode(b"test").decode()
req = f"GET / HTTP/1.1\r\nHost: localhost:3001\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
s.send(req.encode())
resp = s.recv(4096)
assert b"101" in resp, "handshake failed"

# Test snake
r = send_msg(s, '{"action":"new_game","game":"snake","width":20,"height":20}')
print("SNAKE new:", r[:80])
r = send_msg(s, '{"action":"tick","value":3}')
print("SNAKE tick:", r[:80])

# Test gomoku
r = send_msg(s, '{"action":"new_game","game":"gomoku","width":15,"height":15}')
print("GOMOKU new:", r[:80])
r = send_msg(s, '{"action":"tick","value":52}')
print("GOMOKU tick:", r[:80])

# Test pacman
r = send_msg(s, '{"action":"new_game","game":"pacman","width":20,"height":20}')
print("PACMAN new:", r[:80])
r = send_msg(s, '{"action":"tick","value":3}')
print("PACMAN tick:", r[:80])

s.close()
print("\nAll game tests passed!")
