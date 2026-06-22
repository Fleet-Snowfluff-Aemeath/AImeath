import socket, struct, base64

def ws_encode(payload):
    frame = bytearray()
    frame.append(0x81)
    data = payload.encode()
    mask_key = b"mask"
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
s.connect(("127.0.0.1", 5173))
key = base64.b64encode(b"via_vite").decode()
req = (
    "GET /ws HTTP/1.1\r\n"
    "Host: localhost:5173\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: " + key + "\r\n"
    "Sec-WebSocket-Version: 13\r\n\r\n"
)
s.send(req.encode())
resp = s.recv(4096)
assert b"101" in resp, "handshake failed: " + resp[:100].decode()
print("Vite proxy handshake OK")

r = send_msg(s, '{"action":"new_game","game":"snake","width":20,"height":20}')
print("State:", r[:100])
assert '"type":"snake"' in r
print("Test passed!")
s.close()
