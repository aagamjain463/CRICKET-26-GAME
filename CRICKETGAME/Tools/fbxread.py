#!/usr/bin/env python3
"""Minimal binary FBX 7.x reader: enough to extract bone rest poses and animation curves.

Reads the node hierarchy, LimbNode Models with Properties70 (LclTranslation/LclRotation/
LclScale), OO connections (bone parenting), and AnimationCurveNode/AnimationCurve data.
Pure stdlib (zlib for compressed arrays).
"""
import struct
import sys
import zlib


class FBXNode:
    __slots__ = ("name", "props", "children", "had_null", "end_offset")

    def __init__(self, name):
        self.name = name
        self.props = []      # list of (type_code, value)
        self.children = []
        self.had_null = False
        self.end_offset = 0

    def find(self, name, recursive=False):
        for c in self.children:
            if c.name == name:
                return c
        if recursive:
            for c in self.children:
                r = c.find(name, recursive=True)
                if r is not None:
                    return r
        return None

    def findall(self, name, recursive=False, out=None):
        if out is None:
            out = []
        for c in self.children:
            if c.name == name:
                out.append(c)
            if recursive:
                c.findall(name, recursive=True, out=out)
        return out

    def prop(self, index):
        return self.props[index][1] if index < len(self.props) else None

    def p70(self, key, wanted=None):
        """Return the value of a Properties70 property by (partial) name."""
        p70 = self.find("Properties70")
        if p70 is None:
            return None
        for p in p70.children:
            if p.name != "P":
                continue
            if len(p.props) < 5:
                continue
            name = p.props[0][1]
            if wanted is not None and name != wanted:
                continue
            if key not in name:
                continue
            # value props start at index 4 (type-specific)
            vals = [x[1] for x in p.props[4:]]
            return vals
        return None

    def set_p70(self, key, values, wanted=None):
        """Overwrite the value properties of a Properties70 entry (by partial name)."""
        p70 = self.find("Properties70")
        if p70 is None:
            return False
        for p in p70.children:
            if p.name != "P" or len(p.props) < 5:
                continue
            name = p.props[0][1]
            if wanted is not None and name != wanted:
                continue
            if key not in name:
                continue
            newprops = list(p.props[:4])
            for i, v in enumerate(values):
                code, _old = p.props[4 + i]
                newprops.append((code, v))
            p.props = newprops
            return True
        return False


def read_property(buf, pos):
    code = buf[pos:pos+1].decode("ascii")
    pos += 1
    if code == "Y":
        v = struct.unpack_from("<h", buf, pos)[0]; pos += 2
    elif code == "C":
        v = buf[pos] != 0; pos += 1
    elif code == "I":
        v = struct.unpack_from("<i", buf, pos)[0]; pos += 4
    elif code == "F":
        v = struct.unpack_from("<f", buf, pos)[0]; pos += 4
    elif code == "D":
        v = struct.unpack_from("<d", buf, pos)[0]; pos += 8
    elif code == "L":
        v = struct.unpack_from("<q", buf, pos)[0]; pos += 8
    elif code in ("f", "d", "l", "i", "b"):
        array_len, encoding, comp_len = struct.unpack_from("<III", buf, pos)
        pos += 12
        data = buf[pos:pos+comp_len]
        pos += comp_len
        if encoding == 1:
            data = zlib.decompress(data)
        fmt = {"f": "f", "d": "d", "l": "q", "i": "i", "b": "B"}[code]
        v = list(struct.unpack("<%d%s" % (array_len, fmt), data[:struct.calcsize("<%d%s" % (array_len, fmt))]))
    elif code == "S" or code == "R":
        length = struct.unpack_from("<I", buf, pos)[0]
        pos += 4
        raw = buf[pos:pos+length]
        pos += length
        try:
            v = raw.decode("utf-8")
        except UnicodeDecodeError:
            v = raw
    else:
        raise ValueError("unknown property code %r at %d" % (code, pos))
    return code, v, pos


def parse(buf, pos=0, end=None, version=7400):
    """Returns (nodes, ended_with_null_record)."""
    if end is None:
        end = len(buf)
    nodes = []
    big = version >= 7500
    while pos < end:
        if big:
            end_offset, num_props, prop_len, name_len = struct.unpack_from("<QQQB", buf, pos)
            pos += 21
        else:
            end_offset, num_props, prop_len, name_len = struct.unpack_from("<IIIB", buf, pos)
            pos += 13
        if end_offset == 0 and num_props == 0 and prop_len == 0 and name_len == 0:
            return nodes, True  # null terminator
        name = buf[pos:pos+name_len].decode("ascii", "replace")
        pos += name_len
        node = FBXNode(name)
        ppos = pos
        for _ in range(num_props):
            code, v, ppos = read_property(buf, ppos)
            node.props.append((code, v))
        pos = ppos
        if pos < end_offset:
            node.children, ended = parse(buf, pos, end_offset, version)
            node.had_null = ended
            pos = end_offset
        pos = end_offset
        node.end_offset = end_offset
        nodes.append(node)
    return nodes, False


def load(path):
    with open(path, "rb") as f:
        buf = f.read()
    magic = b"Kaydara FBX Binary  \x00\x1a\x00"
    if buf[:len(magic)] != magic:
        raise ValueError("not a binary FBX: %s" % path)
    version = struct.unpack_from("<I", buf, 23)[0]
    nodes, ended = parse(buf, 27, version=version)
    null_size = 25 if version >= 7500 else 13
    tail_start = (nodes[-1].end_offset + null_size) if nodes else 27 + null_size
    return nodes, version, buf[tail_start:]


# ------------------------------------------------------------------------- writing

_FMT = {"Y": "<h", "C": "<B", "I": "<i", "F": "<f", "D": "<d", "L": "<q",
        "f": "<f", "d": "<d", "l": "<q", "i": "<i", "b": "<B"}


def _write_property(out, code, value):
    out += code.encode("ascii")
    if code in ("Y", "C", "I", "F", "D", "L"):
        out += struct.pack(_FMT[code], value)
    elif code in ("f", "d", "l", "i", "b"):
        fmt = _FMT[code]
        data = struct.pack("<%d%s" % (len(value), fmt[1:]), *value)
        out += struct.pack("<III", len(value), 0, len(data))
        out += data
    elif code in ("S", "R"):
        if isinstance(value, str):
            raw = value.encode("utf-8")
        else:
            raw = value
        out += struct.pack("<I", len(raw))
        out += raw


def _serialize_node(node, version, out, base):
    """Serialize one node at absolute offset `base`; returns absolute end offset."""
    body = bytearray()
    for code, value in node.props:
        _write_property(body, code, value)
    name = node.name.encode("ascii", "replace")
    header_len = (25 if version >= 7500 else 13) + len(name)
    children_base = base + header_len + len(body)
    child_bytes = bytearray()
    pos = children_base
    for c in node.children:
        pos = _serialize_node(c, version, child_bytes, pos)
    if node.children or node.had_null:
        null_size = 25 if version >= 7500 else 13
        child_bytes += b"\x00" * null_size
        pos += null_size
    end_offset = pos
    if version >= 7500:
        out += struct.pack("<QQQB", end_offset, len(node.props), len(body), len(name))
    else:
        out += struct.pack("<IIIB", end_offset, len(node.props), len(body), len(name))
    out += name
    out += body
    out += child_bytes
    return end_offset


def save(path, nodes, version, tail=b""):
    out = bytearray()
    out += b"Kaydara FBX Binary  \x00\x1a\x00"
    out += struct.pack("<I", version)
    pos = 27
    for n in nodes:
        pos = _serialize_node(n, version, out, pos)
    out += b"\x00" * (25 if version >= 7500 else 13)
    out += tail
    with open(path, "wb") as f:
        f.write(bytes(out))


# --------------------------------------------------------------------------- helpers

def get_objects(nodes):
    root = None
    for n in nodes:
        if n.name in ("Objects", "Connections"):
            pass
    obj = next((n for n in nodes if n.name == "Objects"), None)
    conn = next((n for n in nodes if n.name == "Connections"), None)
    return obj, conn


def bone_models(objects):
    """{fbx_id: name} for LimbNode models and the model nodes themselves."""
    out = {}
    for n in objects.children:
        if n.name == "Model":
            sub = n.prop(2) if len(n.props) > 2 else ""
            if sub in ("LimbNode", "Limb", "Null"):
                out[n.prop(0)] = n
    return out


def connections(conn):
    """Build child->parent maps for OO connections.

    Only dst that are MODELS count as parents: a bone also links OO to its
    NodeAttribute, and treating that as a parent corrupts the hierarchy."""
    parent_of = {}
    children_of = {}
    for c in conn.children:
        if c.name != "C":
            continue
        if len(c.props) < 3:
            continue
        ctype, src, dst = c.props[0][1], c.props[1][1], c.props[2][1]
        if ctype == "OO":
            parent_of.setdefault(src, []).append(dst)
            children_of.setdefault(dst, []).append(src)
    return parent_of, children_of


def bone_local_rest(model):
    """(translation, rotation_euler_deg, scale) from Properties70."""
    t = model.p70("LclTranslation")
    r = model.p70("LclRotation")
    s = model.p70("LclScaling")
    return t, r, s


def anim_layers(objects):
    """{layer_id: [(curve_node_id, bone_model_id)]} via connections done by caller."""
    layers = {}
    for n in objects.children:
        if n.name == "AnimationLayer":
            layers[n.prop(0)] = n
    return layers


def curve_nodes(objects):
    """{node_id: AnimationCurveNode}"""
    return {n.prop(0): n for n in objects.children if n.name == "AnimationCurveNode"}


def curves(objects):
    """{curve_id: AnimationCurve}"""
    return {n.prop(0): n for n in objects.children if n.name == "AnimationCurve"}


if __name__ == "__main__":
    nodes, version = load(sys.argv[1])
    print("FBX version", version)
    obj, conn = get_objects(nodes)
    bones = bone_models(obj)
    print("limb nodes:", len(bones))
    for bid, m in list(bones.items())[:5]:
        print(bid, m.prop(1), bone_local_rest(m))
