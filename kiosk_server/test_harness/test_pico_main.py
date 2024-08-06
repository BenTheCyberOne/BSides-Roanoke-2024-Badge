import random
import select
import sys
import time

buf = []
badge_connected = False

def buf_manage():
    global badge_connected
    while len(buf) > 0:
        word = buf.pop().strip()
        if word == "detect":
            if random.randint(1,5) > 1:
                badge_connected = True
                print("Connected 00A00D0C\nEOT\n")
            else:
                badge_connected = False
                print("No Badge\n")
        elif word == "dump":
            if badge_connected:
                print(f"Chunks=3\nEOT\n")
                time.sleep(random.uniform(.001, .7))
                print("PART_01\nA0DEADA0BEEF\nEOT\n")
                time.sleep(random.uniform(.001, .7))
                print("PART_02\n0002EA\nEOT\n")
                time.sleep(random.uniform(.001, .7))
                print("PART_03\n901353131234567887654321\nEOT\n")
            else:
                print("ERROR: Cannot dump")
    
while True:
    if select.select([sys.stdin], [],[],0)[0]:
        ch = sys.stdin.readline()
        buf.append(ch)
        buf_manage()
        