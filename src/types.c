#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "exyz.h"

static exyz_status_t error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    return EXYZ_ERROR;
}

static exyz_status_t exyz_info_init_key(exyz_info_t* info,  const char* name) {
    info->key = strdup(name);
    if (info->key == NULL) {
        return error("failed to allocate memory");
    }
    return EXYZ_SUCCESS;
}

exyz_status_t exyz_info_init_integer(exyz_info_t* info, const char* name, int64_t value) {
    exyz_status_t status = exyz_info_init_key(info, name);
    if (status != EXYZ_SUCCESS) {
        return status;
    }

    info->data.integer = value;
    info->type = EXYZ_INTEGER;

    return EXYZ_SUCCESS;
}


exyz_status_t exyz_info_init_real(exyz_info_t* info, const char* name, double value) {
    exyz_status_t status = exyz_info_init_key(info, name);
    if (status != EXYZ_SUCCESS) {
        return status;
    }

    info->data.real = value;
    info->type = EXYZ_REAL;

    return EXYZ_SUCCESS;
}


exyz_status_t exyz_info_init_string(exyz_info_t* info, const char* name, const char* value) {
    exyz_status_t status = exyz_info_init_key(info, name);
    if (status != EXYZ_SUCCESS) {
        return status;
    }

    size_t len = strlen(value);
    info->data.string = malloc(len);
    if (info->data.string == NULL) {
        return error("failed to allocate memory");
    }
    strlcpy(info->data.string, value, len);
    info->type = EXYZ_STRING;

    return EXYZ_SUCCESS;
}


exyz_status_t exyz_info_init_bool(exyz_info_t* info, const char* name, bool value) {
    exyz_status_t status = exyz_info_init_key(info, name);
    if (status != EXYZ_SUCCESS) {
        return status;
    }

    info->data.boolean = value;
    info->type = EXYZ_BOOL;

    return EXYZ_SUCCESS;
}

exyz_status_t exyz_info_free(exyz_info_t info) {
    free(info.key);

    if (info.type == EXYZ_ARRAY) {
        exyz_array_free(info.data.array);
    } else if (info.type == EXYZ_STRING) {
        free(info.data.string);
    }

    return EXYZ_SUCCESS;
}


/******************************************************************************/

exyz_status_t exyz_atom_property_free(exyz_atom_property_t property) {
    free(property.key);
    return EXYZ_SUCCESS;
}


/******************************************************************************/

exyz_status_t exyz_array_init_integer(exyz_array_t* array, size_t nrows, size_t ncols) {
    array->nrows = nrows;
    array->ncols = ncols;
    size_t count = nrows * ncols;
    assert(count != 0);

    array->type = EXYZ_INTEGER;
    array->data.integer = malloc(count * sizeof(int64_t));
    if (array->data.integer == NULL) {
        exyz_array_free(*array);
        return error("failed to allocate memory");
    }

    return EXYZ_SUCCESS;
}


exyz_status_t exyz_array_init_real(exyz_array_t* array, size_t nrows, size_t ncols) {
    array->nrows = nrows;
    array->ncols = ncols;
    size_t count = nrows * ncols;
    assert(count != 0);

    array->type = EXYZ_REAL;
    array->data.real = malloc(count * sizeof(double));
    if (array->data.real == NULL) {
        exyz_array_free(*array);
        return error("failed to allocate memory");
    }

    return EXYZ_SUCCESS;
}


exyz_status_t exyz_array_init_string(exyz_array_t* array, size_t nrows, size_t ncols) {
    array->nrows = nrows;
    array->ncols = ncols;
    size_t count = nrows * ncols;
    assert(count != 0);

    array->type = EXYZ_STRING;
    array->data.string = calloc(count, sizeof(char*));
    if (array->data.string == NULL) {
        exyz_array_free(*array);
        return error("failed to allocate memory");
    }

    return EXYZ_SUCCESS;
}


exyz_status_t exyz_array_init_bool(exyz_array_t* array, size_t nrows, size_t ncols) {
    array->nrows = nrows;
    array->ncols = ncols;
    size_t count = nrows * ncols;
    assert(count != 0);

    array->type = EXYZ_BOOL;
    array->data.boolean = malloc(count * sizeof(bool));
    if (array->data.boolean == NULL) {
        exyz_array_free(*array);
        return error("failed to allocate memory");
    }


    return EXYZ_SUCCESS;
}


exyz_status_t exyz_array_free(exyz_array_t array) {
    assert(array.type != EXYZ_ARRAY);
    if (array.type == EXYZ_INTEGER) {
        free(array.data.integer);
    } else if (array.type == EXYZ_REAL) {
        free(array.data.real);
    } else if (array.type == EXYZ_BOOL) {
        free(array.data.boolean);
    } else if (array.type == EXYZ_STRING) {
        size_t count = array.nrows * array.ncols;
        for (size_t i=0; i<count; i++) {
            free(array.data.string[i]);
        }
        free(array.data.string);
    }

    return EXYZ_SUCCESS;
}
