"""Minimal PCF bitmap-font reader: enough to pull 8-pixel-tall glyph cells."""
import gzip, struct

PCF_PROPERTIES = 1 << 0
PCF_METRICS = 1 << 2
PCF_BITMAPS = 1 << 3
PCF_BDF_ENCODINGS = 1 << 5

PCF_GLYPH_PAD_MASK = 3
PCF_BYTE_MASK = 1 << 2
PCF_BIT_MASK = 1 << 3
PCF_COMPRESSED_METRICS = 1 << 8


class Reader:
    def __init__(self, buf, off, big):
        self.b, self.o, self.big = buf, off, big

    def i32(self):
        v = struct.unpack_from('>i' if self.big else '<i', self.b, self.o)[0]
        self.o += 4
        return v

    def i16(self):
        v = struct.unpack_from('>h' if self.big else '<h', self.b, self.o)[0]
        self.o += 2
        return v

    def u8(self):
        v = self.b[self.o]
        self.o += 1
        return v


class Pcf:
    def __init__(self, path):
        buf = gzip.open(path, 'rb').read() if path.endswith('.gz') else open(path, 'rb').read()
        self.buf = buf
        assert buf[:4] == b'\x01fcp', 'not a PCF file'
        n = struct.unpack_from('<i', buf, 4)[0]
        self.tables = {}
        for i in range(n):
            t, fmt, size, off = struct.unpack_from('<4i', buf, 8 + 16 * i)
            self.tables[t] = (fmt, size, off)
        self.props = self._properties()
        self.metrics = self._metrics()
        self.bitmaps = self._bitmaps()
        self.encoding = self._encodings()

    def _reader(self, table):
        fmt, size, off = self.tables[table]
        fmt2 = struct.unpack_from('<i', self.buf, off)[0]
        return Reader(self.buf, off + 4, bool(fmt2 & PCF_BYTE_MASK)), fmt2

    def _properties(self):
        if PCF_PROPERTIES not in self.tables:
            return {}
        r, fmt = self._reader(PCF_PROPERTIES)
        nprops = r.i32()
        entries = [(r.i32(), r.u8(), r.i32()) for _ in range(nprops)]
        r.o += (4 - nprops * 9 % 4) % 4
        slen = r.i32()
        strings = self.buf[r.o:r.o + slen]

        def s(at):
            end = strings.index(b'\0', at)
            return strings[at:end].decode('latin-1')

        out = {}
        for name_off, is_str, value in entries:
            out[s(name_off)] = s(value) if is_str else value
        return out

    def _metrics(self):
        r, fmt = self._reader(PCF_METRICS)
        out = []
        if fmt & PCF_COMPRESSED_METRICS:
            count = r.i16()
            for _ in range(count):
                out.append(tuple(r.u8() - 0x80 for _ in range(5)))
        else:
            count = r.i32()
            for _ in range(count):
                m = tuple(r.i16() for _ in range(5))
                r.i16()  # attributes
                out.append(m)
        return out  # (lsb, rsb, width, ascent, descent)

    def _bitmaps(self):
        r, fmt = self._reader(PCF_BITMAPS)
        count = r.i32()
        offsets = [r.i32() for _ in range(count)]
        sizes = [r.i32() for _ in range(4)]
        data_off = r.o
        data = self.buf[data_off:data_off + sizes[fmt & PCF_GLYPH_PAD_MASK]]
        self.row_pad = 1 << (fmt & PCF_GLYPH_PAD_MASK)
        self.msb_bits = bool(fmt & PCF_BIT_MASK)
        return [(offsets[i], data) for i in range(count)]

    def _encodings(self):
        r, fmt = self._reader(PCF_BDF_ENCODINGS)
        min2, max2, min1, max1, default = (r.i16() for _ in range(5))
        enc = {}
        for b1 in range(min1, max1 + 1):
            for b2 in range(min2, max2 + 1):
                idx = r.i16()
                if idx != -1 and idx != 0xffff:
                    enc[b1 * 256 + b2 if max1 > 0 else b2] = idx
        return enc

    def glyph(self, code):
        """Return (rows, lsb, width, ascent, descent); rows = list of ints, MSB = leftmost pixel."""
        gi = self.encoding.get(code)
        if gi is None:
            return None
        lsb, rsb, width, ascent, descent = self.metrics[gi]
        off, data = self.bitmaps[gi]
        h = ascent + descent
        stride = self.row_pad
        w = rsb - lsb
        rows = []
        for y in range(h):
            row = 0
            base = off + y * stride
            for byte in range(stride):
                v = data[base + byte]
                if not self.msb_bits:
                    v = int(f'{v:08b}'[::-1], 2)
                row = (row << 8) | v
            rows.append(row >> (stride * 8 - max(w, 1)) if w > 0 else 0)
        return rows, lsb, width, ascent, descent
