import os
from PIL import Image

masks_dir = "Assets/distribution-masks"
for file in os.listdir(masks_dir):
    if file.startswith("maska-"):
        path = os.path.join(masks_dir, file)
        img = Image.open(path).convert('L')
        data = img.getdata()
        count = sum(1 for pixel in data if pixel > 20)
        print(f"{file}: {count} pixels > 20 (total {len(data)})")
