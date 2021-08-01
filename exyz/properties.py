from ._exyz import ffi, lib


class Properties:
    def __init__(self, ptr, count):
        self._ptr = ptr
        self._count = count

    @property
    def ptr(self):
        return self._ptr[0]

    @property
    def count(self):
        return self._count[0]

    def __del__(self):
        for i in range(self.count):
            lib.exyz_atom_property_free(self.ptr[i])

        lib.free(self.ptr)

    def to_dict(self):
        output = {}

        for i in range(self.count):
            key = ffi.string(self.ptr[i].key).decode("utf8")
            count = self.ptr[i].count
            kind = self.ptr[i].type
            if kind == lib.EXYZ_INTEGER:
                kind = "integer"
            elif kind == lib.EXYZ_REAL:
                kind = "real"
            elif kind == lib.EXYZ_BOOL:
                kind = "bool"
            elif kind == lib.EXYZ_STRING:
                kind = "string"

            output[key] = {
                "type": kind,
                "count": count,
            }

        return output
