import struct
import os
import sys

def rotate_gat(file_path):
    print(f"--- Rotando GAT: {file_path} ---")
    with open(file_path, "rb") as f:
        magic = f.read(4)
        ver = f.read(2)
        w, h = struct.unpack("<II", f.read(8))
        print(f"GAT Magic: {magic}, Version: {ver}, Width: {w}, Height: {h}")
        
        cells = []
        for _ in range(w * h):
            h1, h2, h3, h4 = struct.unpack("<4f", f.read(16))
            gat_type = struct.unpack("<I", f.read(4))[0]
            cells.append([h1, h2, h3, h4, gat_type])
            
    # Plaza range in GAT: x in [72..199], y in [190..317]
    min_x, max_x = 72, 199
    min_y, max_y = 190, 317
    
    new_cells = [list(c) for c in cells]
    rotated_count = 0
    
    for y in range(min_y, max_y + 1):
        for x in range(min_x, max_x + 1):
            nx = 271 - x
            ny = 507 - y
            old_idx = y * w + x
            new_idx = ny * w + nx
            
            h1, h2, h3, h4, gat_type = cells[old_idx]
            # 180 degree rotation swaps opposite corners:
            # 1 <-> 4, 2 <-> 3
            new_cells[new_idx] = [h4, h3, h2, h1, gat_type]
            rotated_count += 1
            
    print(f"GAT Celdas rotadas: {rotated_count}")
    
    with open(file_path, "wb") as f:
        f.write(magic)
        f.write(ver)
        f.write(struct.pack("<II", w, h))
        for c in new_cells:
            f.write(struct.pack("<4fI", c[0], c[1], c[2], c[3], c[4]))
            
    print("GAT guardado correctamente.")

def rotate_gnd(file_path):
    print(f"--- Rotando GND: {file_path} ---")
    with open(file_path, "rb") as f:
        magic = f.read(4)
        ver = f.read(2)
        w, h, scale = struct.unpack("<IIf", f.read(12))
        tex_count, max_tex_len = struct.unpack("<II", f.read(8))
        textures_data = f.read(tex_count * max_tex_len)
        
        lm_count, lm_w, lm_h, lm_cell = struct.unpack("<IIII", f.read(16))
        lm_data = bytearray(f.read(lm_count * lm_w * lm_h * 4))
        
        tile_count = struct.unpack("<I", f.read(4))[0]
        tiles = []
        for _ in range(tile_count):
            uvs = list(struct.unpack("<8f", f.read(32)))
            tex_idx = struct.unpack("<h", f.read(2))[0]
            lm_idx = struct.unpack("<H", f.read(2))[0]
            color = f.read(4)
            tiles.append({"uvs": uvs, "tex": tex_idx, "lm": lm_idx, "color": color})
            
        cubes = []
        for y in range(h):
            row = []
            for x in range(w):
                h1, h2, h3, h4 = struct.unpack("<4f", f.read(16))
                up, front, side = struct.unpack("<3i", f.read(12))
                row.append({"h": [h1, h2, h3, h4], "up": up, "front": front, "side": side})
            cubes.append(row)
            
    print(f"GND: {w}x{h}, {tex_count} texturas, {lm_count} lightmaps, {tile_count} tiles")
    
    # Plaza range in GND: x in [36..99], y in [95..158]
    min_x, max_x = 36, 99
    min_y, max_y = 95, 158
    
    new_cubes = [[dict(cubes[y][x]) for x in range(w)] for y in range(h)]
    
    rotated_tiles = set()
    rotated_lms_180 = set()
    rotated_lms_horiz = set()
    
    def rotate_lm_180(lm_idx):
        if lm_idx in rotated_lms_180 or lm_idx < 0 or lm_idx >= lm_count:
            return
        rotated_lms_180.add(lm_idx)
        offset = lm_idx * 256
        # Shadow (64 bytes)
        shadow = lm_data[offset : offset + 64]
        lm_data[offset : offset + 64] = shadow[::-1]
        # RGB (192 bytes = 64 pixels * 3 bytes)
        rgb = lm_data[offset + 64 : offset + 256]
        new_rgb = bytearray(192)
        for i in range(64):
            src_p = i * 3
            dst_p = (63 - i) * 3
            new_rgb[dst_p : dst_p + 3] = rgb[src_p : src_p + 3]
        lm_data[offset + 64 : offset + 256] = new_rgb
        
    def flip_lm_horiz(lm_idx):
        if lm_idx in rotated_lms_horiz or lm_idx < 0 or lm_idx >= lm_count:
            return
        rotated_lms_horiz.add(lm_idx)
        offset = lm_idx * 256
        # Shadow: 8 rows of 8 bytes
        for r in range(8):
            row_off = offset + r * 8
            lm_data[row_off : row_off + 8] = lm_data[row_off : row_off + 8][::-1]
        # RGB: 8 rows of 8 pixels (24 bytes)
        for r in range(8):
            row_off = offset + 64 + r * 24
            row = lm_data[row_off : row_off + 24]
            new_row = bytearray(24)
            for c in range(8):
                new_row[(7 - c) * 3 : (7 - c) * 3 + 3] = row[c * 3 : c * 3 + 3]
            lm_data[row_off : row_off + 24] = new_row
            
    # Clear existing wall assignments in the plaza region
    for y in range(min_y, max_y + 1):
        for x in range(min_x, max_x + 1):
            new_cubes[y][x]["front"] = -1
            new_cubes[y][x]["side"] = -1
            
    # Rotate cubes, tileUp, and walls
    for y in range(min_y, max_y + 1):
        for x in range(min_x, max_x + 1):
            nx = 135 - x
            ny = 253 - y
            old_c = cubes[y][x]
            new_c = new_cubes[ny][nx]
            
            # 1. Heights
            new_c["h"] = [old_c["h"][3], old_c["h"][2], old_c["h"][1], old_c["h"][0]]
            
            # 2. TileUp
            t_up = old_c["up"]
            new_c["up"] = t_up
            if t_up >= 0 and t_up not in rotated_tiles:
                rotated_tiles.add(t_up)
                tile = tiles[t_up]
                # In file: u1, u2, u3, u4, v1, v2, v3, v4
                u = tile["uvs"][:4]
                v = tile["uvs"][4:]
                # 180 deg rotation swaps 1<->4, 2<->3
                new_u = [u[3], u[2], u[1], u[0]]
                new_v = [v[3], v[2], v[1], v[0]]
                tile["uvs"] = new_u + new_v
                rotate_lm_180(tile["lm"])
                
            # 3. TileSide (right wall)
            t_side = old_c["side"]
            if t_side >= 0:
                target_x = 134 - x
                target_y = 253 - y
                new_cubes[target_y][target_x]["side"] = t_side
                if t_side not in rotated_tiles:
                    rotated_tiles.add(t_side)
                    tile = tiles[t_side]
                    # Wall horizontal flip: 1<->2, 3<->4
                    u = tile["uvs"][:4]
                    v = tile["uvs"][4:]
                    new_u = [u[1], u[0], u[3], u[2]]
                    new_v = [v[1], v[0], v[3], v[2]]
                    tile["uvs"] = new_u + new_v
                    flip_lm_horiz(tile["lm"])
                    
            # 4. TileFront (bottom wall)
            t_front = old_c["front"]
            if t_front >= 0:
                target_x = 135 - x
                target_y = 252 - y
                new_cubes[target_y][target_x]["front"] = t_front
                if t_front not in rotated_tiles:
                    rotated_tiles.add(t_front)
                    tile = tiles[t_front]
                    # Wall horizontal flip: 1<->2, 3<->4
                    u = tile["uvs"][:4]
                    v = tile["uvs"][4:]
                    new_u = [u[1], u[0], u[3], u[2]]
                    new_v = [v[1], v[0], v[3], v[2]]
                    tile["uvs"] = new_u + new_v
                    flip_lm_horiz(tile["lm"])

    print(f"GND: Tiles rotados: {len(rotated_tiles)}, Lightmaps rotados 180: {len(rotated_lms_180)}, Lightmaps flipeados h: {len(rotated_lms_horiz)}")
    
    with open(file_path, "wb") as f:
        f.write(magic)
        f.write(ver)
        f.write(struct.pack("<IIf", w, h, scale))
        f.write(struct.pack("<II", tex_count, max_tex_len))
        f.write(textures_data)
        f.write(struct.pack("<IIII", lm_count, lm_w, lm_h, lm_cell))
        f.write(lm_data)
        f.write(struct.pack("<I", tile_count))
        for t in tiles:
            f.write(struct.pack("<8f", *t["uvs"]))
            f.write(struct.pack("<h", t["tex"]))
            f.write(struct.pack("<H", t["lm"]))
            f.write(t["color"])
        for y in range(h):
            for x in range(w):
                c = new_cubes[y][x]
                f.write(struct.pack("<4f", *c["h"]))
                f.write(struct.pack("<3i", c["up"], c["front"], c["side"]))
                
    print("GND guardado correctamente.")

def rotate_rsw(file_path):
    print(f"--- Rotando RSW: {file_path} ---")
    with open(file_path, "rb") as f:
        header = f.read(242)
        obj_count = struct.unpack("<i", f.read(4))[0]
        
        objects_data = []
        rotated_models = 0
        
        for i in range(obj_count):
            t = struct.unpack("<i", f.read(4))[0]
            if t == 1:
                name = f.read(40)
                anim = f.read(12)
                mf = f.read(80)
                on = f.read(80)
                pos = list(struct.unpack("<3f", f.read(12)))
                rot = list(struct.unpack("<3f", f.read(12)))
                scale = f.read(12)
                
                # Filter for plaza models: pos[2] >= -100 and pos[2] < 1000
                if pos[2] >= -100.0 and pos[2] < 1000.0:
                    pos[0] = -200.0 - pos[0]
                    pos[2] = 580.0 - pos[2]
                    rot[1] = (rot[1] + 180.0) % 360.0
                    rotated_models += 1
                    
                obj_bytes = (
                    struct.pack("<i", 1) +
                    name + anim + mf + on +
                    struct.pack("<3f", *pos) +
                    struct.pack("<3f", *rot) +
                    scale
                )
                objects_data.append(obj_bytes)
            elif t == 2:
                name = f.read(80)
                pos = f.read(12)
                color = f.read(12)
                rng = f.read(4)
                obj_bytes = struct.pack("<i", 2) + name + pos + color + rng
                objects_data.append(obj_bytes)
            elif t == 3:
                data = f.read(80 + 80 + 12 + 4 + 4 + 4 + 4 + 4)
                objects_data.append(struct.pack("<i", 3) + data)
            elif t == 4:
                data = f.read(80 + 12 + 4 + 4 + 16)
                objects_data.append(struct.pack("<i", 4) + data)
            else:
                raise ValueError(f"Tipo de objeto desconocido: {t} en objeto {i}")
                
        remaining_data = f.read()
        
    print(f"RSW: Total objetos: {obj_count}, Modelos de la plaza rotados: {rotated_models}, Resto archivo: {len(remaining_data)} bytes")
    
    with open(file_path, "wb") as f:
        f.write(header)
        f.write(struct.pack("<i", obj_count))
        for ob in objects_data:
            f.write(ob)
        f.write(remaining_data)
        
    print("RSW guardado correctamente.")

if __name__ == "__main__":
    base_dir = r"c:\xampp_php8\htdocs\ragnarok\grf\data"
    gat_file = os.path.join(base_dir, "vip_lounge.gat")
    gnd_file = os.path.join(base_dir, "vip_lounge.gnd")
    rsw_file = os.path.join(base_dir, "vip_lounge.rsw")
    
    rotate_gat(gat_file)
    rotate_gnd(gnd_file)
    rotate_rsw(rsw_file)
    print("\n¡ROTACIÓN DE LA PLAZA A 180° COMPLETADA CON ÉXITO!")
