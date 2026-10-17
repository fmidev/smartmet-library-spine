#!/usr/bin/env python3
"""Generate the background map of the data explorer (basemap.json).

The map is drawn when no tile layer is configured (api_catalog.map_tiles), so
that the selections can be made without any online map service. The source is
Natural Earth (https://www.naturalearthdata.com/), which is in the public
domain: the 1:50m land, lakes and land boundary lines, version 5.1.2:

  https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/

Usage: make-basemap.py <directory of the GeoJSON files> > basemap.json

The coordinates are rounded to 0.01 degrees and delta encoded as integers:
each ring or line is [lon0, lat0, dlon1, dlat1, ...] in hundredths of a degree.
"""

import json
import os
import sys

LAYERS = {
    "land": ("ne_50m_land.geojson", True),
    "lakes": ("ne_50m_lakes.geojson", True),
    "borders": ("ne_50m_admin_0_boundary_lines_land.geojson", False),
}


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


def encode(ring):
    out = []
    px = py = None
    for lon, lat in ((p[0], p[1]) for p in ring):
        x = round(lon * 100)
        y = round(lat * 100)
        if (x, y) == (px, py):
            continue
        if px is None:
            out += [x, y]
        else:
            out += [x - px, y - py]
        px, py = x, y
    return out


def main(directory):
    doc = {"source": "Natural Earth 1:50m 5.1.2 (public domain)", "scale": 100}
    for name, (filename, polygons) in LAYERS.items():
        with open(os.path.join(directory, filename), encoding="utf-8") as f:
            features = json.load(f)["features"]
        encoded = []
        for feature in features:
            for ring in rings(feature["geometry"], polygons):
                e = encode(ring)
                # Rings collapsed to a few points at this precision are left out
                if len(e) >= (8 if polygons else 4):
                    encoded.append(e)
        doc[name] = encoded
    json.dump(doc, sys.stdout, separators=(",", ":"))


if __name__ == "__main__":
    main(sys.argv[1])
