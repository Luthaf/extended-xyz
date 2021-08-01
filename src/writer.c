#include <stdarg.h>

#include "exyz.h"

static exyz_status_t error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    return EXYZ_ERROR;
}

exyz_status_t exyz_write(FILE* fd, size_t* n_atoms, exyz_info_t* info, exyz_array_t* arrays) {
    return error("unimplemented");
}
