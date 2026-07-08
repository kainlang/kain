"""Test GEMMA 4 vision with a clear image via OpenAI-compatible endpoint."""
import base64, json, struct, zlib
from urllib.request import urlopen, Request

def make_png(w, h, pixels):
    def chunk(ctype, data):
        c = ctype + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    raw = b''
    for y in range(h):
        raw += b'\x00'
        for x in range(w):
            r, g, b = pixels[y * w + x]
            raw += struct.pack('BBB', r, g, b)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')

# 200x150: left red, right blue, with a green square in the center
pixels = []
for y in range(150):
    for x in range(200):
        # Green square in center
        if 75 <= x <= 125 and 50 <= y <= 100:
            pixels.append((0, 255, 0))
        elif x < 100:
            pixels.append((255, 0, 0))
        else:
            pixels.append((0, 0, 255))

png = make_png(200, 150, pixels)
b64 = base64.b64encode(png).decode()

payload = {
    "model": "google/gemma-4-e2b",
    "messages": [
        {"role": "user", "content": [
            {"type": "text", "text": "Describe this image. What colors do you see? What shapes? Be specific."},
            {"type": "image_url", "image_url": {"url": f"data:image/png;base64,{b64}"}}
        ]}
    ]
}

print(f"Image: 200x150, {len(b64)} base64 chars. Sending...")
req = Request("http://localhost:1234/v1/chat/completions",
              data=json.dumps(payload).encode(),
              headers={"Content-Type": "application/json"})
resp = urlopen(req, timeout=120)
data = json.loads(resp.read().decode())
print(json.dumps(data, indent=2)[:2000])
