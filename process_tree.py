from PIL import Image
import sys
import os

if len(sys.argv) < 3:
    print("Usage: python process_tree.py <input> <output>")
    sys.exit(1)

input_path = sys.argv[1]
output_path = sys.argv[2]

img = Image.open(input_path).convert("RGBA")
data = img.getdata()

new_data = []
for item in data:
    # If the pixel is very close to white, make it transparent
    if item[0] > 240 and item[1] > 240 and item[2] > 240:
        new_data.append((255, 255, 255, 0))
    else:
        # Optionally, remove a bit of white fringe?
        new_data.append(item)

img.putdata(new_data)
os.makedirs(os.path.dirname(output_path), exist_ok=True)
img.save(output_path, "PNG")
print(f"Saved processed tree to {output_path}")
