import re
import numpy as np
import matplotlib.pyplot as plt

WIDTH = 120
HEIGHT = 90
LAYERS = 10
PIXELS_PER_LAYER = WIDTH * HEIGHT  # 10800

# 1. Trích xuất toàn bộ mảng dữ liệu theo đúng thứ tự xuất hiện
with open("mieng_bin.h", "r", encoding="utf-8") as f:
    content = f.read()

hex_tokens = re.findall(r"0x[0-9a-fA-F]{4}", content)
words = np.array([int(x, 16) for x in hex_tokens], dtype=np.uint16)

# 2. Xử lý và giải mã từng ảnh
images = []
for i in range(LAYERS):
    # Cắt đúng đoạn data của layer i
    chunk = words[i * PIXELS_PER_LAYER : (i + 1) * PIXELS_PER_LAYER]
    
    # Giải mã RGB565 sang RGB888
    r = ((chunk >> 11) & 0x1F) * 255 // 31
    g = ((chunk >> 5) & 0x3F) * 255 // 63
    b = (chunk & 0x1F) * 255 // 31
    
    img = np.dstack((r, g, b)).astype(np.uint8).reshape((HEIGHT, WIDTH, 3))
    images.append(img)

# 3. Hiển thị toàn bộ 10 ảnh theo thứ tự 2 hàng x 5 cột
fig, axes = plt.subplots(2, 5, figsize=(15, 6))
fig.suptitle("Danh sách 10 layer/frame theo thứ tự từ 0 -> 9", fontsize=14)

for idx, ax in enumerate(axes.flat):
    if idx < LAYERS:
        ax.imshow(images[idx])
        ax.set_title(f"Frame {idx} (Index {idx*PIXELS_PER_LAYER})")
        ax.axis("off")

plt.tight_layout()
plt.show()