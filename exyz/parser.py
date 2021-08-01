from ._exyz import ffi, lib

from .info import Info
from .properties import Properties


def parse_comment_line(line):
    line = line.encode(u"utf8")

    properties_ptr = ffi.new("exyz_atom_property_t**")
    properties_count = ffi.new("size_t*")

    info_ptr = ffi.new("exyz_info_t**")
    info_count = ffi.new("size_t*")

    status = lib.exyz_read_comment_line(
        line, len(line), properties_ptr, properties_count, info_ptr, info_count
    )

    if status != lib.EXYZ_SUCCESS:
        raise Exception("failed to parse comment line")

    properties = Properties(properties_ptr, properties_count)
    info = Info(info_ptr, info_count)

    return properties, info
