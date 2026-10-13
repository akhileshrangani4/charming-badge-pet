"""Reset the badge and capture serial output for a few seconds (dev helper)."""
import sys, time, serial, glob
port = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] else sorted(glob.glob("/dev/cu.usbmodem*"))[0]
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 6
send = sys.argv[3:]  # optional lines to send after 2 s
s = serial.Serial(port, 115200, timeout=0.1)
if "--reset" in send:
    send.remove("--reset"); s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
t0 = time.time(); sent = False
while time.time() - t0 < secs:
    if send and not sent and time.time() - t0 > 2:
        for line in send: s.write((line + "\n").encode()); time.sleep(0.3)
        sent = True
    data = s.read(4096)
    if data: sys.stdout.write(data.decode(errors="replace")); sys.stdout.flush()
