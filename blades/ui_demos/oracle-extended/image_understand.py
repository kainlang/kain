#!/usr/bin/env python3
"""
Oracle Vision GPU — BLIP image captioning via transformers + CUDA.
Called by oracle.exe (C#) via Process.Start.

Usage:
    image_understand.py <image_path>
Output: JSON to stdout
  { "success": true, "caption": "a dog sitting in grass", "device": "cuda:Quadro RTX 3000", "time_seconds": 0.45 }
"""

import sys, json, os, time
from PIL import Image

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "Usage: image_understand.py <image_path>"}))
        return 1

    image_path = sys.argv[1]
    if not os.path.exists(image_path):
        print(json.dumps({"success": False, "error": f"Image not found: {image_path}"}))
        return 1

    try:
        import torch
        from transformers import BlipProcessor, BlipForConditionalGeneration

        device = "cuda" if torch.cuda.is_available() else "cpu"
        device_name = torch.cuda.get_device_name(0) if torch.cuda.is_available() else "cpu"
        start = time.time()

        # Load model
        processor = BlipProcessor.from_pretrained("Salesforce/blip-image-captioning-base")
        model = BlipForConditionalGeneration.from_pretrained(
            "Salesforce/blip-image-captioning-base"
        ).to(device)

        load_time = time.time() - start

        # Run captioning
        image = Image.open(image_path).convert("RGB")
        inputs = processor(image, return_tensors="pt").to(device)
        out = model.generate(**inputs, max_new_tokens=50, num_beams=3)
        caption = processor.decode(out[0], skip_special_tokens=True)

        total_time = time.time() - start

        result = {
            "success": True,
            "caption": caption,
            "device": f"{device}:{device_name}",
            "load_time_seconds": round(load_time, 2),
            "time_seconds": round(total_time, 2),
            "model": "blip-image-captioning-base",
            "params_m": 224,
        }
        print(json.dumps(result))
        return 0

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        return 1

if __name__ == "__main__":
    sys.exit(main())
