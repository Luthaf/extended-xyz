import numpy as np

from ._exyz import ffi, lib


class Info:
    def __init__(self, ptr, count):
        self._ptr = ptr
        self._count = count

    def __del__(self):
        for i in range(self.count):
            lib.exyz_info_free(self.ptr[i])

        lib.free(self.ptr)

    @property
    def ptr(self):
        return self._ptr[0]

    @property
    def count(self):
        return self._count[0]

    def to_dict(self):
        output = {}

        for i in range(self.count):
            key = ffi.string(self.ptr[i].key).decode("utf8")

            kind = self.ptr[i].type
            if kind == lib.EXYZ_INTEGER:
                value = self.ptr[i].data.integer
            elif kind == lib.EXYZ_REAL:
                value = self.ptr[i].data.real
            elif kind == lib.EXYZ_BOOL:
                value = self.ptr[i].data.boolean
            elif kind == lib.EXYZ_STRING:
                value = ffi.string(self.ptr[i].data.string).decode("utf8")
            else:
                assert kind == lib.EXYZ_ARRAY
                value = _read_array(self.ptr[i].data.array)

            output[key] = value

        return output


def _read_array(array):
    if array.type == lib.EXYZ_STRING:
        np_array = np.zeros((array.nrows, array.ncols), dtype=np.str)
        index = 0
        for i in range(array.nrows):
            for j in range(array.ncols):
                np_array[i, j] = ffi.string(array.data.string[index]).decode("utf8")
                index += 1

        if array.nrows == 1:
            return np_array.reshape(-1)
        else:
            return np_array

    if array.type == lib.EXYZ_INTEGER:
        sizeof = ffi.sizeof("int64_t")
        ptr = array.data.integer
        dtype = np.int64
    elif array.type == lib.EXYZ_REAL:
        sizeof = ffi.sizeof("double")
        ptr = array.data.real
        dtype = np.float64
    elif array.type == lib.EXYZ_BOOL:
        sizeof = ffi.sizeof("bool")
        ptr = array.data.boolean
        dtype = np.bool8

    buffer = ffi.buffer(ptr, array.nrows * array.ncols * sizeof)
    np_array = np.frombuffer(buffer, dtype=dtype)
    if array.nrows != 1:
        np_array = np_array.reshape(array.nrows, array.ncols)

    return np.copy(np_array)
