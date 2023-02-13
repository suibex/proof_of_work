import hashlib


DIG = b"\x31"*40
has = hashlib.sha1(DIG).digest()
for byte in has:
    print(hex(byte)[2:])
