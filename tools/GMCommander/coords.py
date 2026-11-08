"""Koordinat metni ayristirma (.gps ciktisi, 'x y z o' satirlari, virgullu listeler)."""
import re

_NUM = r"(-?\d+(?:\.\d+)?)"
_GPS = re.compile(
    rf"X:\s*{_NUM}\s*Y:\s*{_NUM}\s*Z:\s*{_NUM}(?:\s*(?:Orientation|O):\s*{_NUM})?", re.I
)
_PLAIN = re.compile(rf"^\s*{_NUM}[\s,;]+{_NUM}[\s,;]+{_NUM}(?:[\s,;]+{_NUM})?")


def parse_coords(text: str):
    """[(x, y, z, o), ...] dondurur. Once .gps formatini, yoksa satir satir sayilari arar."""
    found = [tuple(float(v) if v else 0.0 for v in m.groups()) for m in _GPS.finditer(text)]
    if found:
        return found
    out = []
    for line in text.splitlines():
        line = line.strip().strip("()")
        m = _PLAIN.match(line)
        if m:
            out.append(tuple(float(v) if v else 0.0 for v in m.groups()))
    return out
