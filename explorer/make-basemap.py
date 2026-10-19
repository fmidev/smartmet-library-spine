#!/usr/bin/env python3
"""Generate the background map of the data explorer (basemap.json).

The map is drawn when no tile layer is configured (api_catalog.map_tiles), so
that the selections can be made without any online map service. The source is
Natural Earth (https://www.naturalearthdata.com/), which is in the public
domain: the 1:50m land, lakes and land boundary lines, version 5.1.2:

  https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/

Usage: make-basemap.py <directory of the GeoJSON files> > basemap.json
       make-basemap.py --10m <directory of the GeoJSON files> > basemap-10m.json

The second form makes the more detailed map of enlarged and zoomed in maps from
the 1:10m files of the same release (ne_10m_land, ne_10m_lakes and
ne_10m_admin_0_boundary_lines_land).

The coordinates are rounded (to 0.01 degrees, 0.002 degrees for 1:10m) and
delta encoded as integers: each ring or line is [lon0, lat0, dlon1, dlat1, ...]
in units of 1/scale degrees.
"""

import json
import os
import sys

LAYERS = {
    "land": ("ne_{}_land.geojson", True),
    "lakes": ("ne_{}_lakes.geojson", True),
    "borders": ("ne_{}_admin_0_boundary_lines_land.geojson", False),
}

# The resolution of the map: the scale of the files and the units of the coordinates
SCALES = {"50m": 100, "10m": 500}


def rings(geometry, polygons):
    kind = geometry["type"]
    coords = geometry["coordinates"]
    if polygons:
        if kind == "Polygon":
            return coords
        if kind == "MultiPolygon":
            return [ring for polygon in coords for ring in polygon]
    else:
        if kind == "LineString":
            return [coords]
        if kind == "MultiLineString":
            return coords
    return []


def encode(ring, scale):
    out = []
    px = py = None
    for lon, lat in ((p[0], p[1]) for p in ring):
        x = round(lon * scale)
        y = round(lat * scale)
        if (x, y) == (px, py):
            continue
        if px is None:
            out += [x, y]
        else:
            out += [x - px, y - py]
        px, py = x, y
    return out


def main(directory, resolution):
    scale = SCALES[resolution]
    doc = {"source": f"Natural Earth 1:{resolution} 5.1.2 (public domain)", "scale": scale}
    for name, (filename, polygons) in LAYERS.items():
        with open(os.path.join(directory, filename.format(resolution)), encoding="utf-8") as f:
            features = json.load(f)["features"]
        encoded = []
        for feature in features:
            for ring in rings(feature["geometry"], polygons):
                e = encode(ring, scale)
                # Rings collapsed to a few points at this precision are left out
                if len(e) >= (8 if polygons else 4):
                    encoded.append(e)
        doc[name] = encoded
    json.dump(doc, sys.stdout, separators=(",", ":"))


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--10m":
        main(sys.argv[2], "10m")
    else:
        main(sys.argv[1], "50m")
