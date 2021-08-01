#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum exyz_status_t {
    EXYZ_SUCCESS = 0,
    EXYZ_ERROR,
    EXYZ_FAILED_READING,
} exyz_status_t;

typedef enum exyz_data_t {
    EXYZ_INTEGER = 'I',
    EXYZ_REAL = 'R',
    EXYZ_BOOL = 'L',
    EXYZ_STRING = 'S',
    EXYZ_ARRAY = 'A',
} exyz_data_t;

typedef struct exyz_array_t {
    union {
        int64_t* integer;
        double* real;
        char** string;
        bool* boolean;
    } data;
    enum exyz_data_t type;

    size_t nrows;
    size_t ncols;
} exyz_array_t;

exyz_status_t exyz_array_init_integer(exyz_array_t* array, size_t nrows, size_t ncols);
exyz_status_t exyz_array_init_real(exyz_array_t* array, size_t nrows, size_t ncols);
exyz_status_t exyz_array_init_string(exyz_array_t* array, size_t nrows, size_t ncols);
exyz_status_t exyz_array_init_bool(exyz_array_t* array, size_t nrows, size_t ncols);

exyz_status_t exyz_array_free(exyz_array_t array);

/// Frame properties
typedef struct exyz_info_t {
    char* key;

    union {
        int64_t integer;
        double real;
        char* string;
        bool boolean;
        exyz_array_t array;
    } data;
    enum exyz_data_t type;
} exyz_info_t;

exyz_status_t exyz_info_init_integer(exyz_info_t* info, const char* name, int64_t value);
exyz_status_t exyz_info_init_real(exyz_info_t* info, const char* name, double value);
exyz_status_t exyz_info_init_string(exyz_info_t* info, const char* name, const char* value);
exyz_status_t exyz_info_init_bool(exyz_info_t* info, const char* name, bool value);

exyz_status_t exyz_info_free(exyz_info_t info);

/// Data from the "Properties" entry in comment line
typedef struct exyz_atom_property_t {
    char* key;
    enum exyz_data_t type;
    size_t count;
} exyz_atom_property_t;

exyz_status_t exyz_atom_property_free(exyz_atom_property_t property);

/// Atom properties
typedef struct exyz_atom_array_t {
    char* key;
    exyz_array_t array;
} exyz_atom_array_t;


exyz_status_t exyz_read_comment_line(
    const char* line,
    size_t line_length,
    exyz_atom_property_t** properties,
    size_t* properties_count,
    exyz_info_t** info,
    size_t* info_count
);

exyz_status_t exyz_read(
    FILE* fp,
    size_t* n_atoms,
    exyz_info_t** info,
    size_t* info_count,
    exyz_atom_array_t** arrays,
    size_t* arrays_count
);

exyz_status_t exyz_write(
    FILE* fp,
    size_t* n_atoms,
    exyz_info_t* info,
    exyz_array_t* arrays
);

#ifdef __cplusplus
}
#endif
