import struct
import zlib

gat_file = "/opt/robrowser-client/data/vip_lounge.gat"
rsw_file = "/opt/robrowser-client/data/vip_lounge.rsw"

with open(rsw_file, "rb") as f:
    rsw = f.read()
water_level = int(struct.unpack("<f", rsw[166:170])[0])

with open(gat_file, "rb") as f:
    magic = f.read(4)
    ver = f.read(2)
    xs, ys = struct.unpack("<II", f.read(8))
    num_cells = xs * ys
    cells = bytearray(num_cells)
    for xy in range(num_cells):
        data = f.read(20)
        height, h2, h3, h4, ctype = struct.unpack("<ffffI", data)
        if ctype == 0 and water_level != -1 and height > water_level:
            ctype = 3
        cells[xy] = ctype & 0xff

comp = zlib.compress(bytes(cells))
map_name = b"vip_lounge\x00\x00"
map_info = map_name + struct.pack("<hhi", xs, ys, len(comp))

total_file_size = 8 + len(map_info) + len(comp)
main_hdr = struct.pack("<IHxx", total_file_size, 1)

with open("/opt/rAthena/db/import/map_cache.dat", "wb") as f:
    f.write(main_hdr + map_info + comp)

print(f"Updated /opt/rAthena/db/import/map_cache.dat ({total_file_size} bytes, map: vip_lounge, xs={xs}, ys={ys})")
