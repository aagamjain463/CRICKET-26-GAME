#!/usr/bin/env python3
"""A pure-python mathutils shim: just enough Vector/Quaternion/Matrix for
ArtSource/Blender/Animation/c26_anim_author.py to be imported OUTSIDE Blender,
so the offline clip rebuilder (Tools/rebuild_authored_clips.py) and the Blender
authoring run share one source of truth for the keyframes.

Conventions mirror mathutils: quaternions are (w, x, y, z); matrices are 4x4
row-major acting on column vectors (M @ v); Matrix.translation is the last column.
"""
import math


class Vector:
    __slots__ = ('v',)

    def __init__(self, seq=(0.0, 0.0, 0.0)):
        self.v = tuple(float(c) for c in seq)

    def __getitem__(self, i):
        return self.v[i]

    def __setitem__(self, i, c):
        l = list(self.v)
        l[i] = float(c)
        self.v = tuple(l)

    def __len__(self):
        return len(self.v)

    def __iter__(self):
        return iter(self.v)

    def __add__(self, o):
        return Vector(tuple(a + b for a, b in zip(self.v, o.v if isinstance(o, Vector) else o)))

    __radd__ = __add__

    def __sub__(self, o):
        return Vector(tuple(a - b for a, b in zip(self.v, o.v if isinstance(o, Vector) else o)))

    def __rsub__(self, o):
        return Vector(tuple(b - a for a, b in zip(self.v, o.v if isinstance(o, Vector) else o)))

    def __mul__(self, o):
        if isinstance(o, Vector):
            return sum(a * b for a, b in zip(self.v, o.v))
        return Vector(tuple(a * o for a in self.v))

    __rmul__ = __mul__

    def __truediv__(self, o):
        return Vector(tuple(a / o for a in self.v))

    def __matmul__(self, o):
        return sum(a * b for a, b in zip(self.v, o.v))

    def __neg__(self):
        return Vector(tuple(-a for a in self.v))

    def __eq__(self, o):
        return isinstance(o, Vector) and self.v == o.v

    @property
    def x(self):
        return self.v[0]

    @property
    def y(self):
        return self.v[1]

    @property
    def z(self):
        return self.v[2]

    @property
    def length(self):
        return math.sqrt(sum(c * c for c in self.v))

    @property
    def length_squared(self):
        return sum(c * c for c in self.v)

    def copy(self):
        return Vector(self.v)

    def normalize(self):
        n = self.length or 1.0
        self.v = tuple(c / n for c in self.v)

    def normalized(self):
        n = self.length or 1.0
        return Vector(tuple(c / n for c in self.v))

    def dot(self, o):
        return sum(a * b for a, b in zip(self.v, o.v))

    def cross(self, o):
        a, b = self.v, o.v
        return Vector((a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]))

    def rotation_difference(self, other):
        """Quaternion rotating self onto other (shortest arc), like mathutils."""
        a = self.normalized().v
        b = other.normalized().v
        d = max(-1.0, min(1.0, sum(x * y for x, y in zip(a, b))))
        if d > 1.0 - 1e-9:
            return Quaternion()
        if d < -1.0 + 1e-9:
            # opposite: any axis orthogonal to a
            axis = Vector((1.0, 0.0, 0.0))
            if abs(a[0]) > 0.9:
                axis = Vector((0.0, 1.0, 0.0))
            n = axis.cross(Vector(a)).normalized().v
            return Quaternion((0.0, n[0], n[1], n[2]))
        c = math.sqrt((1.0 + d) * 0.5)
        s = math.sqrt((1.0 - d) * 0.5)
        cross = (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
        n = math.sqrt(cross[0]**2 + cross[1]**2 + cross[2]**2) or 1.0
        return Quaternion((c, s*cross[0]/n, s*cross[1]/n, s*cross[2]/n))

    def __repr__(self):
        return 'Vector' + str(self.v)


class Quaternion:
    __slots__ = ('q',)

    def __init__(self, seq=(1.0, 0.0, 0.0, 0.0), angle=None):
        if angle is not None:
            # Quaternion(axis_vector, angle)
            h = math.radians(angle) / 2.0
            s = math.sin(h)
            ax = seq.normalized().v
            self.q = (math.cos(h), ax[0]*s, ax[1]*s, ax[2]*s)
        else:
            self.q = tuple(float(c) for c in seq)

    def __mul__(self, o):
        if isinstance(o, Quaternion):
            return Quaternion(_qmul(self.q, o.q))
        if isinstance(o, Vector):
            return Vector(_qrot(self.q, o.v))
        raise TypeError

    __matmul__ = __mul__

    def copy(self):
        return Quaternion(self.q)

    def conjugated(self):
        w, x, y, z = self.q
        return Quaternion((w, -x, -y, -z))

    def invert(self):
        self.q = self.conjugated().q

    def inverted(self):
        return self.conjugated()

    def normalized(self):
        n = math.sqrt(sum(c*c for c in self.q)) or 1.0
        return Quaternion(tuple(c / n for c in self.q))

    def normalize(self):
        self.q = self.normalized().q

    @property
    def magnitude(self):
        return math.sqrt(sum(c*c for c in self.q))

    def to_matrix(self):
        m = Matrix.Identity(4)
        m._store_quat(self.q)
        return m

    def to_euler(self):
        w, x, y, z = self.q
        # ZYX intrinsic euler (matches Blender default XYZ-order usage closely enough
        # for diagnostics; the pipeline stores quaternions)
        ry = math.asin(max(-1.0, min(1.0, -2*(x*z - w*y))))
        rx = math.atan2(2*(y*z + w*x), 1 - 2*(x*x + y*y))
        rz = math.atan2(2*(x*y + w*z), 1 - 2*(y*y + z*z))
        return Euler((math.degrees(rx), math.degrees(ry), math.degrees(rz)))

    def __repr__(self):
        return 'Quaternion' + str(self.q)


class Euler:
    def __init__(self, angles, order='XYZ'):
        self.angles = tuple(float(c) for c in angles)
        self.order = order

    def __iter__(self):
        return iter(self.angles)

    def __getitem__(self, i):
        return self.angles[i]


def _qmul(a, b):
    w1, x1, y1, z1 = a
    w2, x2, y2, z2 = b
    return (w1*w2 - x1*x2 - y1*y2 - z1*z2,
            w1*x2 + x1*w2 + y1*z2 - z1*y2,
            w1*y2 - x1*z2 + y1*w2 + z1*x2,
            w1*z2 + x1*y2 - y1*x2 + z1*w2)


def _qrot(q, v):
    w, x, y, z = q
    qv = (0.0,) + tuple(v)
    return _qmul(_qmul(q, qv), (w, -x, -y, -z))[1:]


class Matrix:
    """4x4 (or 3x3) row-major matrix acting on column vectors."""
    __slots__ = ('rows', 'size')

    def __init__(self, rows=None, size=4):
        self.size = size
        if rows is None:
            n = size
            self.rows = [[1.0 if i == j else 0.0 for j in range(n)] for i in range(n)]
        else:
            self.rows = [list(float(c) for c in r) for r in rows]
            self.size = len(self.rows)

    @staticmethod
    def Identity(n=4):
        m = Matrix(size=n)
        m.rows = [[1.0 if i == j else 0.0 for j in range(n)] for i in range(n)]
        return m

    @staticmethod
    def Translation(v):
        m = Matrix.Identity(4)
        m.rows[0][3], m.rows[1][3], m.rows[2][3] = float(v[0]), float(v[1]), float(v[2])
        return m

    @staticmethod
    def Rotation(angle, size=4, axis='X'):
        r = math.radians(angle)
        c, s = math.cos(r), math.sin(r)
        if axis == 'X':
            r3 = [[1, 0, 0], [0, c, -s], [0, s, c]]
        elif axis == 'Y':
            r3 = [[c, 0, s], [0, 1, 0], [-s, 0, c]]
        else:
            r3 = [[c, -s, 0], [s, c, 0], [0, 0, 1]]
        m = Matrix.Identity(size)
        for i in range(3):
            for j in range(3):
                m.rows[i][j] = r3[i][j]
        return m

    @staticmethod
    def Scale(factor, size=4, axis=None):
        m = Matrix.Identity(size)
        if axis is None:
            for i in range(min(3, size)):
                m.rows[i][i] = float(factor)
        return m

    def copy(self):
        return Matrix([list(r) for r in self.rows])

    def __matmul__(self, o):
        a, b = self.rows, o.rows
        n, m, k = len(a), len(b), len(b[0])
        out = [[sum(a[i][t]*b[t][j] for t in range(m)) for j in range(k)] for i in range(n)]
        r = Matrix(out)
        return r

    def __mul__(self, o):
        if isinstance(o, (int, float)):
            return Matrix([[c*o for c in r] for r in self.rows])
        return self.__matmul__(o)

    def __getitem__(self, i):
        return self.rows[i]

    def __repr__(self):
        return 'Matrix(%r)' % (self.rows,)

    @property
    def translation(self):
        return Vector((self.rows[0][3], self.rows[1][3], self.rows[2][3]))

    @translation.setter
    def translation(self, v):
        self.rows[0][3], self.rows[1][3], self.rows[2][3] = float(v[0]), float(v[1]), float(v[2])

    def to_translation(self):
        return self.translation

    def to_3x3(self):
        return Matrix([r[:3] for r in self.rows[:3]])

    def to_matrix(self):
        return self.to_3x3()

    def to_4x4(self):
        if self.size == 4:
            return self.copy()
        m = Matrix.Identity(4)
        for i in range(3):
            for j in range(3):
                m.rows[i][j] = self.rows[i][j]
        return m

    def resized_4x4(self):
        return self.to_4x4()

    def inverted(self):
        return Matrix(_invert(self.rows))

    def invert(self):
        self.rows = _invert(self.rows)

    def _store_quat(self, q):
        w, x, y, z = q
        self.rows = [[1-2*(y*y+z*z), 2*(x*y-w*z), 2*(x*z+w*y), 0],
                     [2*(x*y+w*z), 1-2*(x*x+z*z), 2*(y*z-w*x), 0],
                     [2*(x*z-w*y), 2*(y*z+w*x), 1-2*(x*x+y*y), 0],
                     [0, 0, 0, 1]]

    def to_quaternion(self):
        m = [r[:3] for r in self.rows[:3]]
        # Shepperd's method, largest-diagonal branch for stability
        tr = m[0][0] + m[1][1] + m[2][2]
        if tr > 0:
            s = math.sqrt(tr + 1.0) * 2
            q = (0.25*s, (m[2][1]-m[1][2])/s, (m[0][2]-m[2][0])/s, (m[1][0]-m[0][1])/s)
        elif m[0][0] > m[1][1] and m[0][0] > m[2][2]:
            s = math.sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2
            q = ((m[2][1]-m[1][2])/s, 0.25*s, (m[0][1]+m[1][0])/s, (m[0][2]+m[2][0])/s)
        elif m[1][1] > m[2][2]:
            s = math.sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2
            q = ((m[0][2]-m[2][0])/s, (m[0][1]+m[1][0])/s, 0.25*s, (m[1][2]+m[2][1])/s)
        else:
            s = math.sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2
            q = ((m[1][0]-m[0][1])/s, (m[0][2]+m[2][0])/s, (m[1][2]+m[2][1])/s, 0.25*s)
        n = math.sqrt(sum(c*c for c in q)) or 1.0
        return Quaternion(tuple(c/n for c in q))


def _invert(rows):
    n = len(rows)
    a = [list(r) + [1.0 if i == j else 0.0 for j in range(n)] for i, r in enumerate(rows)]
    for col in range(n):
        piv = max(range(col, n), key=lambda r: abs(a[r][col]))
        if abs(a[piv][col]) < 1e-12:
            raise ValueError('singular matrix')
        a[col], a[piv] = a[piv], a[col]
        inv = 1.0 / a[col][col]
        a[col] = [c*inv for c in a[col]]
        for r in range(n):
            if r != col and a[r][col] != 0.0:
                f = a[r][col]
                a[r] = [c - f*d for c, d in zip(a[r], a[col])]
    return [row[n:] for row in a]
