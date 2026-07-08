"""Quick test: send test image to GEMMA 4 via LM Studio."""
import base64, json, struct, zlib
from urllib.request import urlopen, Request

def make_png(w, h, pixels):
    """pixels: list of (r,g,b) tuples, row-major"""
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

# 100x100 image: left half red, right half blue
pixels = []
for y in range(100):
    for x in range(100):
        if x < 50:
            pixels.append((255, 0, 0))
        else:
            pixels.append((0, 0, 255))

png = make_png(100, 100, pixels)
b64 = base64.b64encode(png).decode()

# Try LM Studio format with image field
payload = {
    "model": "google/gemma-4-e2b",
    "system_prompt": "You are a vision analyzer. Describe EXACTLY what you see.",
    "input": "What do you see in this image?",
    "image": b64
}

print("Test 1: LM Studio format with 'image' field...")
try:
    req = Request("http://localhost:1234/api/v1/chat",
                  data=json.dumps(payload).encode(),
                  headers={"Content-Type": "application/json"})
    resp = urlopen(req, timeout=60)
    data = json.loads(resp.read().decode())
    print(json.dumps(data, indent=2)[:1500])
except Exception as e:
    print(f"  FAILED: {e}")

# Try OpenAI format
payload2 = {
    "model": "google/gemma-4-e2b",
    "messages": [
        {"role": "system", "content": "You are a vision analyzer. Describe EXACTLY what you see."},
        {"role": "user", "content": [
            {"type": "text", "text": "What do you see in this image? Describe the colors and layout."},
            {"type": "image_url", "image_url": {"url": f"data:image/png;base64,{b64}"}}
        ]}
    ]
}

print("\nTest 2: OpenAI messages format...")
try:
    req = Request("http://localhost:1234/api/v1/chat",
                  data=json.dumps(payload2).encode(),
                  headers={"Content-Type": "application/json"})
    resp = urlopen(req, timeout=60)
    data = json.loads(resp.read().decode())
    print(json.dumps(data, indent=2)[:1500])
except Exception as e:
    print(f"  FAILED: {e}")
