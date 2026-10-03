#!/usr/bin/env python3
"""
Generate the data files behind the VPN page's background globe from Natural
Earth (https://www.naturalearthdata.com), which is in the public domain:

    src/assets/globe/land.png             equirectangular land mask, 1 bit per pixel
    src/assets/globe/country_centers.json country code -> [latitude, longitude]

Usage:
    python3 tools/generate_globe_data.py <ne_50m_land.geojson> <ne_50m_admin_0_countries.geojson>

Both inputs are in the geojson/ folder of
https://github.com/nvkelso/natural-earth-vector. Only the Python standard
library is used, so the output can be regenerated anywhere.
"""

import json
import math
import os
import struct
import sys
import zlib

MASK_WIDTH = 2048
MASK_HEIGHT = 1024

# Proton VPN uses a few codes that are not the ISO 3166-1 ones Natural Earth
# uses. Each alias gets the same center as the code it stands for.
CODE_ALIASES = {"UK": "GB"}

OUTPUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "assets", "globe")


def polygon_rings(geometry):
    """Every ring (outer boundaries and holes) of a Polygon or MultiPolygon."""
    if geometry["type"] == "Polygon":
        return list(geometry["coordinates"])
    if geometry["type"] == "MultiPolygon":
        return [ring for polygon in geometry["coordinates"] for ring in polygon]
    return []


def centered_pixels(start, end, limit):
    """Pixels whose center (index + 0.5) lies in [start, end), within [0, limit)."""
    return range(max(0, math.ceil(start - 0.5)), min(limit, math.ceil(end - 0.5)))


def rasterize_land(features):
    """
    Even-odd scanline fill of every ring at once, sampling pixel centers.
    Holes (the Caspian Sea, large lakes) come out as water, and Natural Earth
    land polygons never overlap, so even-odd is exact.
    """
    crossings = [[] for _ in range(MASK_HEIGHT)]
    for feature in features:
        for ring in polygon_rings(feature["geometry"]):
            points = [((lon + 180.0) / 360.0 * MASK_WIDTH, (90.0 - lat) / 180.0 * MASK_HEIGHT)
                      for lon, lat in ring]
            for (x0, y0), (x1, y1) in zip(points, points[1:] + points[:1]):
                if y0 == y1:
                    continue
                for row in centered_pixels(min(y0, y1), max(y0, y1), MASK_HEIGHT):
                    center = row + 0.5
                    crossings[row].append(x0 + (center - y0) * (x1 - x0) / (y1 - y0))

    mask = [bytearray(MASK_WIDTH) for _ in range(MASK_HEIGHT)]
    for row, xs in enumerate(crossings):
        xs.sort()
        for start, end in zip(xs[0::2], xs[1::2]):
            for col in centered_pixels(start, end, MASK_WIDTH):
                mask[row][col] = 1
    return mask


def write_mono_png(path, mask):
    """Writes a 1-bit grayscale PNG (land = white)."""
    def chunk(kind, data):
        payload = kind + data
        return struct.pack(">I", len(data)) + payload + struct.pack(">I", zlib.crc32(payload) & 0xFFFFFFFF)

    raw = bytearray()
    for row in mask:
        raw.append(0)  # filter type: none
        for byte_start in range(0, MASK_WIDTH, 8):
            byte = 0
            for bit in range(8):
                byte = (byte << 1) | row[byte_start + bit]
            raw.append(byte)

    header = struct.pack(">IIBBBBB", MASK_WIDTH, MASK_HEIGHT, 1, 0, 0, 0, 0)
    with open(path, "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n")
        out.write(chunk(b"IHDR", header))
        out.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        out.write(chunk(b"IEND", b""))


def country_centers(features):
    """ISO code -> [lat, lon] of Natural Earth's label point for the country."""
    centers = {}
    for feature in features:
        props = feature["properties"]
        # ISO_A2_EH fills in the codes ISO_A2 leaves as -99 (France, Norway, Kosovo).
        code = props.get("ISO_A2_EH") or props.get("ISO_A2")
        if not code or code == "-99":
            continue
        centers[code] = [round(props["LABEL_Y"], 2), round(props["LABEL_X"], 2)]
    for alias, code in CODE_ALIASES.items():
        if code in centers:
            centers[alias] = centers[code]
    return dict(sorted(centers.items()))


def main():
    if len(sys.argv) != 3:
        sys.exit(f"Usage: {sys.argv[0]} <ne_50m_land.geojson> <ne_50m_admin_0_countries.geojson>")

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    with open(sys.argv[1], encoding="utf-8") as f:
        land = json.load(f)["features"]
    land_path = os.path.join(OUTPUT_DIR, "land.png")
    write_mono_png(land_path, rasterize_land(land))
    print(f"wrote {land_path} ({os.path.getsize(land_path)} bytes)")

    with open(sys.argv[2], encoding="utf-8") as f:
        countries = json.load(f)["features"]
    centers = country_centers(countries)
    centers_path = os.path.join(OUTPUT_DIR, "country_centers.json")
    with open(centers_path, "w", encoding="utf-8") as out:
        json.dump(centers, out, separators=(",", ":"))
        out.write("\n")
    print(f"wrote {centers_path} ({len(centers)} countries, {os.path.getsize(centers_path)} bytes)")


if __name__ == "__main__":
    main()
