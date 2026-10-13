"""Grab real frames from the badge over serial (G = scene, X = expression, P = dump) and save round PNGs."""
import sys, time, glob, serial
from PIL import Image, ImageDraw

def frame(s, cmd=b"P"):
    s.reset_input_buffer(); s.write(cmd + b"\n")
    t0 = time.time(); buf = b""
    while b"FRAME" not in buf and time.time() - t0 < 5:
        buf += s.read(256)
    head, _, rest = buf.partition(b"FRAME")
    line, _, data = rest.partition(b"\n")
    w, h = map(int, line.split())
    need = w * h * 2
    while len(data) < need and time.time() - t0 < 20:
        data += s.read(need - len(data))
    img = Image.frombytes("RGB", (w, h), b"".join(bytes(((p := (data[i] << 8) | data[i + 1]) >> 8 & 0xF8, (p >> 3) & 0xFC, (p << 3) & 0xF8)) for i in range(0, need, 2)))
    mask = Image.new("L", (w, h), 0); ImageDraw.Draw(mask).ellipse((1, 1, w - 2, h - 2), fill=255)
    out = Image.new("RGBA", (w, h)); out.paste(img, (0, 0), mask)
    return out

if __name__ == "__main__":
    port = sorted(glob.glob("/dev/cu.usbmodem*"))[0]
    s = serial.Serial(port, 115200, timeout=0.2)
    shots = [a.split("=", 1) for a in sys.argv[1:]]   # name=command;command
    for name, cmds in shots:
        for c in cmds.split(";"):
            if c: s.write((c + "\n").encode()); time.sleep(0.4)
        time.sleep(1.2)
        frame(s).save(f"docs/screenshots/{name}.png")
        print("saved", name)
