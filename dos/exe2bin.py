import sys

with open(sys.argv[1], "rb") as f:
    bytes = f.read()
    org = bytes[0x14] | (bytes[0x15]<<8)
    bytes = bytes[org+512:]
    with open(sys.argv[2], "wb") as f2:
        f2.write(bytes)
