#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <locale.h>

#include "exyz.h"

static exyz_status_t error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    return EXYZ_ERROR;
}

static void* alloc_one_more(void* ptr, size_t current, size_t size) {
    if (current == 0) {
        assert(ptr == NULL);
        return calloc(1, size);
    } else {
        ptr = realloc(ptr, (current + 1) * size);
        memset((char*)ptr + current * size, 0, size);
        return ptr;
    }
}

/******************************************************************************/
/*                        Parser building blocks                              */
/******************************************************************************/

typedef struct parser_context_t {
    char* string;
    size_t length;
    size_t current;
} parser_context_t;

static bool is_whitespace(char c) {
    return c == ' ' || c == '\t';
}

static bool is_end_of_value(char c, bool inside_array) {
    // possible end of values:
    // - whitespace
    // - end of input
    // - '"': end of array, old style array
    // - ',': next item, new style array
    // - ']': end of array, new style array
    return is_whitespace(c) || c == '\0' || (
        inside_array && (c == ',' || c == ']' || c == '"')
    );
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}


static bool is_ident_start_char(char c) {
    return is_alpha(c) || c == '_';
}

static bool is_ident_char(char c) {
    return is_ident_start_char(c) || is_digit(c);
}

static bool is_bare_string_char(char c) {
    return is_ident_char(c) || c == '@' || c == '`' || c == '!' || c == '#' ||
       c == '$' || c == '%' || c == '&' || c == '(' || c == ')' || c == '*' ||
       c == '+' || c == '-' || c == '.' || c == '/' || c == ':' || c == ';' ||
       c == '<' || c == '|' || c == '>' || c == '^' || c == '~' || c == '?' ||
       c == '\'';
}

static bool is_quoted_string_char(char c) {
    return is_bare_string_char(c) || c == ' ' || c == '\t' || c == '\\' ||
        c == '=' || c == ']' || c == '[' || c == '}' || c == '{' || c == ',';
}

static void skip_whitespaces(parser_context_t* ctx) {
    while (is_whitespace(ctx->string[ctx->current])) {
        ctx->current += 1;

        if (ctx->current == ctx->length) {
            break;
        }
    }
}

static exyz_status_t skip_bare_string(parser_context_t* ctx) {
    size_t size = 0;

    while (is_bare_string_char(ctx->string[ctx->current + size])) {
        size += 1;

        if (ctx->current + size == ctx->length) {
            break;
        }
    }

    if (size == 0) {
        return EXYZ_FAILED_READING;
    } else {
        ctx->current += size;
        return EXYZ_SUCCESS;
    }
}

static exyz_status_t skip_quoted_string(parser_context_t* ctx) {
    if (ctx->string[ctx->current] != '"') {
        return error("expected '\"' at the start of a quoted string");
    }

    size_t ctx_start = ctx->current;
    ctx->current += 1;

    bool found_end_of_string = false;
    while (ctx->current < ctx->length) {
        char c = ctx->string[ctx->current];
        if (is_quoted_string_char(c)) {
            ctx->current += 1;
        } else if (c == '"') {
            if (ctx->current > ctx_start && ctx->string[ctx->current - 1] == '\\') {
                // escaped quote '\"'
                ctx->current += 1;
            } else {
                found_end_of_string = true;
                ctx->current += 1;
                break;
            }
        } else {
            ctx->current = ctx_start;
            return error("invalid character inside quoted string: '%c'", c);
        }
    }

    if (!found_end_of_string) {
        ctx->current = ctx_start;
        return error("expected '\"' at the end of string, got end of input");
    }

    return EXYZ_SUCCESS;
}

static exyz_status_t skip_string(parser_context_t* ctx) {
    if (ctx->string[ctx->current] == '"') {
        return skip_quoted_string(ctx);
    } else {
        return skip_bare_string(ctx);
    }
}

/// copy `value` of size `length` to the the output following extended xyz
/// escape rules
static char* unescape_quoted_string(const char* value, size_t length) {
    char* output = calloc(length + 1, 1);
    if (output == NULL) {
        return output;
    }

    size_t position = 0;
    for (size_t i=0; i<length; i++) {

        if (value[i] == '\\') {
            if (i + 1 == length) {
                error("quoted string can not end with '\\'");
                free(output);
                return NULL;
            }

            i++;
            if (value[i] == 'n') {
                output[position] = '\n';
            } else {
                output[position] = value[i];
            }
        } else {
            output[position] = value[i];
        }
        position += 1;
    }

    assert(strlen(output) == position);

    return output;
}

/// read a quoted string, and store it in `value`.
static exyz_status_t read_quoted_string(parser_context_t* ctx, char** value) {
    if (ctx->string[ctx->current] != '"') {
        return error("quoted strings must start with \"");
    }

    size_t start = ctx->current + 1;
    size_t size = 0;
    while (start + size <= ctx->length) {
        char current = ctx->string[start + size];
        if (is_quoted_string_char(current)) {
            size += 1;
        } else if (current == '"' && start + size > 0 && ctx->string[start + size - 1] == '\\') {
            size += 1;
        } else {
            break;
        }
    }

    if (ctx->string[start + size] != '"') {
        return error("quoted string must end with \"");
    }

    *value = unescape_quoted_string(ctx->string + start, size);

    if (*value == NULL) {
        return error("failed to allocate memory");
    }

    // size +2 since we also need to remove the two quotes
    ctx->current += size + 2;

    return EXYZ_SUCCESS;
}

/// read a bare string, and store it in `value`.
static exyz_status_t read_bare_string(parser_context_t* ctx, char** value) {
    size_t start = ctx->current;
    size_t size = 0;
    while (start + size < ctx->length && is_bare_string_char(ctx->string[start + size])) {
        size += 1;
    }

    if (size == 0) {
        return EXYZ_FAILED_READING;
    }

    *value = strndup(ctx->string + start, size);
    if (*value == NULL) {
        return error("failed to allocate memory");
    }

    ctx->current += size;

    return EXYZ_SUCCESS;
}

/// read an identifier, and store it in `value`.
static exyz_status_t read_ident(parser_context_t* ctx, char** value) {
    size_t start = ctx->current;
    size_t size = 0;

    if (!is_ident_char(ctx->string[start])) {
        return error("expected identifer start character, got %c", ctx->string[start]);
    }
    size += 1;

    while (start + size < ctx->length && is_ident_char(ctx->string[start + size])) {
        size += 1;
    }

    *value = strndup(ctx->string + start, size);
    if (*value == NULL) {
        return error("failed to allocate memory");
    }

    ctx->current += size;

    return EXYZ_SUCCESS;
}

/// read a string (either bare string or quoted string), and store it in `value`.
static exyz_status_t read_string(parser_context_t* ctx, char** value) {
    if (ctx->string[ctx->current] == '"') {
        return read_quoted_string(ctx, value);
    } else {
        return read_bare_string(ctx, value);
    }
}

/// read an integer value, and store it in `value`.
static exyz_status_t try_read_integer(parser_context_t* ctx, int64_t* value, bool inside_array) {
    size_t start = ctx->current;
    size_t size = 0;

    // allow optional initial +/-
    bool leading_sign = false;
    if (ctx->string[start] == '+' || ctx->string[start] == '-') {
        size += 1;
        leading_sign = true;
    }

    while (start + size < ctx->length && is_digit(ctx->string[start + size])) {
        size += 1;
    }

    if (size == 0 || (leading_sign && size == 1)) {
        return EXYZ_FAILED_READING;
    }

    char last = ctx->string[start + size];
    // integer can also end on ':' in Properties
    if (!(is_end_of_value(last, inside_array) || last == ':')) {
        return EXYZ_FAILED_READING;
    }

    errno = 0;
    char* end = NULL;
    *value = strtoll(ctx->string + start, &end, 10);

    if (errno == ERANGE || ctx->string + start + size != end) {
        return EXYZ_FAILED_READING;
    }

    ctx->current += size;
    return EXYZ_SUCCESS;
}

/// read a floating point value, and store it in `value`.
static exyz_status_t try_read_real(parser_context_t* ctx, double* value, bool inside_array) {
    size_t start = ctx->current;
    size_t size = 0;

    // allow optional initial +/-
    bool leading_sign = false;
    if (ctx->string[start] == '+' || ctx->string[start] == '-') {
        size += 1;
        leading_sign = true;
    }

    // then digits
    while (start + size < ctx->length && is_digit(ctx->string[start + size])) {
        size += 1;
    }

    // then if there is a decimal separator, more (required) digits
    if (ctx->string[start + size] == '.') {
        size += 1;

        bool fractional_digits = false;
        while (start + size < ctx->length && is_digit(ctx->string[start + size])) {
            size += 1;
            fractional_digits = true;
        }

        if (!fractional_digits) {
            return EXYZ_FAILED_READING;
        }
    }

    // then maybe an exponent
    char current = ctx->string[start + size];
    bool fortran_style_exponent = false;
    if (current == 'e' || current == 'E' || current == 'd' || current == 'D') {
        if (current == 'd' || current == 'D') {
            fortran_style_exponent = true;
        }

        size += 1;

        // in which case, optional sign and more (required) digits
        if (ctx->string[start] == '+' || ctx->string[start] == '-') {
            size += 1;
        }

        bool exponent_digits = false;
        while (start + size < ctx->length && is_digit(ctx->string[start + size])) {
            size += 1;
            exponent_digits = true;
        }

        if (!exponent_digits) {
            return EXYZ_FAILED_READING;
        }
    }

    if (size == 0 || (leading_sign && size == 1)) {
        return EXYZ_FAILED_READING;
    }

    char last = ctx->string[start + size];
    // number end on whitespace or next item in array, which can be:
    // - '"': end of array, old style array
    // - ',': next item, new style array
    // - ']': end of array, new style array
    if (!is_end_of_value(last, inside_array)) {
        return EXYZ_FAILED_READING;
    }

    // ok, we have what looks like a number, let's try to parse it
    char* number = ctx->string + start;
    if (fortran_style_exponent) {
        number = strndup(ctx->string + start, size);
        for (size_t i=0; i<size; i++) {
            if (number[i] == 'd' || number[i] == 'D') {
                number[i] = 'e';
            }
        }
    }

    errno = 0;
    char* end = NULL;
    *value = strtod(number, &end);

    if (fortran_style_exponent) {
        free(number);
    }

    if (errno == ERANGE || number + size != end) {
        return EXYZ_FAILED_READING;
    }

    ctx->current += size;
    return EXYZ_SUCCESS;
}

/// read a boolean value, and store it in `value`.
static exyz_status_t try_read_boolean(parser_context_t* ctx, bool* value, bool inside_array) {
    size_t start = ctx->current;
    if (ctx->string[start] == 'T' || ctx->string[start] == 't') {
        if (ctx->length - start >= 4) {
            if (strncmp(ctx->string + start, "true", 4) == 0 ||
                strncmp(ctx->string + start, "True", 4) == 0 ||
                strncmp(ctx->string + start, "TRUE", 4) == 0
            ) {
                *value = true;
                ctx->current += 4;
                return EXYZ_SUCCESS;
            }
        }

        if (ctx->string[start] == 'T' && (is_end_of_value(ctx->string[start + 1], inside_array))) {
            *value = true;
            ctx->current += 1;
            return EXYZ_SUCCESS;
        }
    } else if (ctx->string[start] == 'F' || ctx->string[start] == 'f') {
        if (ctx->length - start >= 5) {
            if (strncmp(ctx->string + start, "false", 5) == 0 ||
                strncmp(ctx->string + start, "False", 5) == 0 ||
                strncmp(ctx->string + start, "FALSE", 5) == 0
            ) {
                *value = false;
                ctx->current += 5;
                return EXYZ_SUCCESS;
            }
        }

        if (ctx->string[start] == 'F' && (is_end_of_value(ctx->string[start + 1], inside_array))) {
            *value = false;
            ctx->current += 1;
            return EXYZ_SUCCESS;
        }
    }

    return EXYZ_FAILED_READING;
}

static exyz_status_t get_array_value_type(
    parser_context_t* ctx,
    exyz_data_t* type
) {
    if (*type == EXYZ_INTEGER) {
        int64_t value= 0;
        exyz_status_t status = try_read_integer(ctx, &value, true);
        if (status == EXYZ_SUCCESS) {
            return EXYZ_SUCCESS;
        } else if (status == EXYZ_FAILED_READING) {
            // try reading a real for this value
            *type = EXYZ_REAL;
        } else {
            return status;
        }
    }

    if (*type == EXYZ_REAL) {
        double value = 0;
        exyz_status_t status = try_read_real(ctx, &value, true);
        if (status == EXYZ_SUCCESS) {
            return EXYZ_SUCCESS;
        } else if (status == EXYZ_FAILED_READING) {
            // try reading a bool for this value
            *type = EXYZ_BOOL;
        } else {
            return status;
        }
    }

    if (*type == EXYZ_BOOL) {
        bool value = false;
        exyz_status_t status = try_read_boolean(ctx, &value, true);
        if (status == EXYZ_SUCCESS) {
            return EXYZ_SUCCESS;
        } else if (status == EXYZ_FAILED_READING) {
            // try reading a string for this value
            *type = EXYZ_STRING;
        } else {
            return status;
        }
    }

    assert(*type == EXYZ_STRING);
    return skip_string(ctx);
}

static exyz_status_t read_array_value(
    parser_context_t* ctx,
    exyz_array_t array,
    size_t index
) {
    if (array.type == EXYZ_INTEGER) {
        return try_read_integer(ctx, array.data.integer + index, true);
    } else if (array.type == EXYZ_REAL) {
        return try_read_real(ctx, array.data.real + index, true);
    } else if (array.type == EXYZ_BOOL) {
        return try_read_boolean(ctx, array.data.boolean + index, true);
    } else {
        assert(array.type == EXYZ_STRING);
        return read_string(ctx, array.data.string + index);
    }
}

static exyz_status_t try_read_old_style_array_quote(parser_context_t* ctx, exyz_array_t* array) {
    if (ctx->string[ctx->current] != '"') {
        return error("old style array must start with \", got '%c'", ctx->string[ctx->current]);
    }

    // initialize array type to ensure we can free safely it in the error case
    // below
    array->type = EXYZ_INTEGER;

    size_t ctx_start = ctx->current;
    ctx->current += 1;

    exyz_status_t status = EXYZ_SUCCESS;
    size_t current_index = 0;

    // count the number of entries in the array and try to guess the type of the array
    size_t n_values = 0;
    bool found_array_end = false;
    exyz_data_t type = EXYZ_INTEGER;
    while (ctx->current < ctx->length) {
        skip_whitespaces(ctx);

        if (ctx->string[ctx->current] == '"') {
            found_array_end = true;
            break;
        }

        status = get_array_value_type(ctx, &type);
        if (status != EXYZ_SUCCESS) {
            goto error;
        }

        if (type == EXYZ_STRING) {
            // this is actually a quoted string
            status = EXYZ_FAILED_READING;
            goto error;
        }
        n_values += 1;

        char c = ctx->string[ctx->current];
        if (!(is_whitespace(c) || c == '"')) {
            status = error("values should be separated by space in old style array, got '%c'", ctx->string[ctx->current]);
            goto error;
        }
    }

    if (!found_array_end) {
        return error("expected '\"' to finish the array, found end of input");
    }

    // now that we know the type & size of the array, reset the parser and read
    // for real
    ctx->current = ctx_start + 1;
    if (type == EXYZ_INTEGER) {
        status = exyz_array_init_integer(array, 1, n_values);
    } else if (type == EXYZ_REAL) {
        status = exyz_array_init_real(array, 1, n_values);
    } else if (type == EXYZ_BOOL) {
        status = exyz_array_init_bool(array, 1, n_values);
    } else {
        status = error("invalid type inside quoted array");
    }

    if (status != EXYZ_SUCCESS) {
        goto error;
    }


    while (ctx->current < ctx->length) {
        skip_whitespaces(ctx);

        if (ctx->string[ctx->current] == '"') {
            ctx->current += 1;
            break;
        }

        status = read_array_value(ctx, *array, current_index);
        if (status != EXYZ_SUCCESS) {
            goto error;
        }
        current_index += 1;

        char c = ctx->string[ctx->current];
        if (!(is_whitespace(c) || c == '"')) {
            status = error("expected whitespace between array values, got '%c'", ctx->string[ctx->current]);
            goto error;
        }
    }

    return EXYZ_SUCCESS;
error:
    // reset parser state
    ctx->current = ctx_start;
    exyz_array_free(*array);

    return status;
}

static exyz_status_t try_read_old_style_array_bracket(parser_context_t* ctx, exyz_array_t* array) {
    if (ctx->string[ctx->current] != '{') {
        return error("old style array must start with {, got '%c'", ctx->string[ctx->current]);
    }

    size_t ctx_start = ctx->current;
    ctx->current += 1;

    exyz_status_t status = EXYZ_SUCCESS;
    size_t current_index = 0;

    // count the number of entries in the array
    size_t n_values = 0;
    bool found_array_end = false;
    while (ctx->current < ctx->length) {
        skip_whitespaces(ctx);

        if (ctx->string[ctx->current] == '}') {
            found_array_end = true;
            break;
        } else {
            n_values += 1;
            status = skip_string(ctx);
            if (status != EXYZ_SUCCESS) {
                goto error;
            }

            char c = ctx->string[ctx->current];
            if (!(is_whitespace(c) || c == '}')) {
                status = error("values should be separated by space in old style array, got '%c'", ctx->string[ctx->current]);
                goto error;
            }
        }
    }

    if (!found_array_end) {
        return error("expected '}' to finish the array, found end of input");
    }

    // now that we know the size of the array, reset the parser and read for
    // real
    ctx->current = ctx_start + 1;
    status = exyz_array_init_string(array, 1, n_values);
    if (status != EXYZ_SUCCESS) {
        goto error;
    }

    while (ctx->current < ctx->length) {
        skip_whitespaces(ctx);

        if (ctx->string[ctx->current] == '}') {
            ctx->current += 1;
            break;
        } else {
            status = read_array_value(ctx, *array, current_index);
            if (status != EXYZ_SUCCESS) {
                goto error;
            }
            current_index += 1;
        }
    }

    return EXYZ_SUCCESS;
error:
    // reset parser state
    ctx->current = ctx_start;
    exyz_array_free(*array);

    return status;
}

static exyz_status_t try_read_new_style_array(parser_context_t* ctx, exyz_array_t* array) {
    if (ctx->string[ctx->current] != '[') {
        return error("array must start with [, got '%c'", ctx->string[ctx->current]);
    }

    // TODO: 2D arrays

    size_t ctx_start = ctx->current;
    ctx->current += 1;

    exyz_status_t status = EXYZ_SUCCESS;
    size_t current_index = 0;

    // count the number of entries in the array, and try to guess the type of
    // the array
    size_t n_rows = 1;
    size_t n_cols = 0;
    bool found_array_end = false;
    exyz_data_t type = EXYZ_INTEGER;
    while (ctx->current < ctx->length) {
        skip_whitespaces(ctx);

        status = get_array_value_type(ctx, &type);
        if (status != EXYZ_SUCCESS) {
            goto error;
        }
        n_cols += 1;

        skip_whitespaces(ctx);

        if (ctx->string[ctx->current] == ']') {
            found_array_end = true;
            break;
        }

        if (ctx->string[ctx->current] != ',') {
            status = error("expected comma in array between values, got '%c'", ctx->string[ctx->current]);
            goto error;
        }
        ctx->current += 1;
    }

    if (!found_array_end) {
        return error("expected ']' to finish the array, found end of input");
    }

    // reset parser
    ctx->current = ctx_start + 1;
    // allocate the array and read it
    if (type == EXYZ_INTEGER) {
        status = exyz_array_init_integer(array, n_rows, n_cols);
    } else if (type == EXYZ_REAL) {
        status = exyz_array_init_real(array, n_rows, n_cols);
    } else if (type == EXYZ_BOOL) {
        status = exyz_array_init_bool(array, n_rows, n_cols);
    } else {
        assert(type == EXYZ_STRING);
        status = exyz_array_init_string(array, n_rows, n_cols);
    }

    if (status != EXYZ_SUCCESS) {
        goto error;
    }


    while (ctx->current < ctx->length) {
        skip_whitespaces(ctx);

        status = read_array_value(ctx, *array, current_index);
        if (status != EXYZ_SUCCESS) {
            goto error;
        }
        current_index += 1;

        skip_whitespaces(ctx);

        if (ctx->string[ctx->current] == ']') {
            ctx->current += 1;
            break;
        }

        if (ctx->string[ctx->current] != ',') {
            status = error("expected comma in array, got '%c'", ctx->string[ctx->current]);
            goto error;
        }
        ctx->current += 1;
    }

    return EXYZ_SUCCESS;
error:
    // reset parser state
    ctx->current = ctx_start;
    exyz_array_free(*array);

    return status;
}

static exyz_status_t try_read_array(parser_context_t* ctx, exyz_array_t* array) {
    char first = ctx->string[ctx->current];
    if (first == '[') {
        return try_read_new_style_array(ctx, array);
    } else if (first == '{') {
        return try_read_old_style_array_bracket(ctx, array);
    } else if (first == '"') {
        return try_read_old_style_array_quote(ctx, array);
    } else {
        return EXYZ_FAILED_READING;
    }
}

/// read a frame property name in the comment line up to the '=' sign, and return
/// the corresponding key in `key`
static exyz_status_t info_key(parser_context_t* ctx, char** key) {
    exyz_status_t status = read_string(ctx, key);
    if (status != EXYZ_SUCCESS) {
        return status;
    }

    skip_whitespaces(ctx);

    if (ctx->string[ctx->current] == '=') {
        ctx->current += 1;
        return EXIT_SUCCESS;
    } else {
        free(*key);
        return error("expected '=' after the frame property key in comment line, got '%c'", ctx->string[ctx->current]);
    }
}

/// read a frame property value, and store the data in `info`
static exyz_status_t info_value(parser_context_t* ctx, exyz_info_t* info) {
    exyz_array_t value_array = {
        .data.integer = NULL,
        .type = EXYZ_INTEGER,
        .nrows = 0,
        .ncols = 0,
    };
    exyz_status_t status = try_read_array(ctx, &value_array);
    if (status == EXYZ_SUCCESS) {
        info->type = EXYZ_ARRAY;
        info->data.array = value_array;
        return EXYZ_SUCCESS;
    } else if (status != EXYZ_FAILED_READING) {
        return status;
    }

    int64_t value_integer= 0;
    status = try_read_integer(ctx, &value_integer, false);
    if (status == EXYZ_SUCCESS) {
        info->type = EXYZ_INTEGER;
        info->data.integer = value_integer;
        return EXYZ_SUCCESS;
    } else if (status != EXYZ_FAILED_READING) {
        return status;
    }

    double value_float = 0;
    status = try_read_real(ctx, &value_float, false);
    if (status == EXYZ_SUCCESS) {
        info->type = EXYZ_REAL;
        info->data.real = value_float;
        return EXYZ_SUCCESS;
    } else if (status != EXYZ_FAILED_READING) {
        return status;
    }

    bool value_bool;
    status = try_read_boolean(ctx, &value_bool, false);
    if (status == EXYZ_SUCCESS) {
        info->type = EXYZ_BOOL;
        info->data.boolean = value_bool;
        return EXYZ_SUCCESS;
    } else if (status != EXYZ_FAILED_READING) {
        return status;
    }

    char* value_string;
    status = read_string(ctx, &value_string);
    if (status == EXYZ_SUCCESS) {
        info->type = EXYZ_STRING;
        info->data.string = value_string;
        return EXYZ_SUCCESS;
    } else {
        return status;
    }
}

static exyz_status_t skip_colon_in_properties(parser_context_t* ctx) {
    char c = ctx->string[ctx->current];
    if (c != ':') {
            return error("expected ':' in Properties specification, got %c", c);
    }
    ctx->current += 1;
    return EXYZ_SUCCESS;
}

/// read the atom properties specification and store them as an array in `properties`
static exyz_status_t atoms_properties(
    parser_context_t* line_ctx,
    exyz_atom_property_t** properties,
    size_t* properties_count
) {
    assert(*properties == NULL);
    assert(*properties_count == 0);

    exyz_status_t status = EXYZ_SUCCESS;
    parser_context_t ctx = {
        .string = NULL,
        .length = 0,
        .current = 0
    };

    status = read_string(line_ctx, &ctx.string);
    if (status != EXYZ_SUCCESS) {
        return status;
    }
    ctx.length = strlen(ctx.string);

    char* current_key = NULL;
    while (ctx.current != ctx.length) {
        status = read_ident(&ctx, &current_key);
        if (status != EXYZ_SUCCESS) {
            goto error;
        }

        status = skip_colon_in_properties(&ctx);
        if (status != EXYZ_SUCCESS) {
            goto error;
        }

        char type = ctx.string[ctx.current];
        if (type != 'L' && type != 'S' && type != 'R' && type != 'I') {
            status = error("expected one of L/S/R/I in Properties specification, got %c", ctx.string[ctx.current]);
            goto error;
        }
        ctx.current += 1;

        status = skip_colon_in_properties(&ctx);
        if (status != EXYZ_SUCCESS) {
            goto error;
        }

        int64_t count = 0;
        status = try_read_integer(&ctx, &count, false);
        if (status != EXYZ_SUCCESS) {
            status = error("failed to read an integer in Properties specification");
            goto error;
        }

        if (count < 0) {
            status = error("invalid negative number in Properties specification (%ld)", count);
            goto error;
        }

        *properties = alloc_one_more(*properties, *properties_count, sizeof(exyz_atom_property_t));
        if (*properties == NULL) {
            status = error("failed to allocate memory");
            goto error;
        }

        exyz_atom_property_t* current_property = *properties + *properties_count;
        current_property->count = (size_t)count;
        current_property->type = (exyz_data_t)type;
        current_property->key = current_key;
        current_key = NULL;

        *properties_count += 1;

        // either we are at the end or there is another property
        if (ctx.current != ctx.length) {
            status = skip_colon_in_properties(&ctx);
            if (status != EXYZ_SUCCESS) {
                goto error;
            }
        }
    }

    free(ctx.string);
    return EXYZ_SUCCESS;

error:
    free(ctx.string);
    free(current_key);

    for (size_t i=0; i<*properties_count; i++) {
        exyz_atom_property_free((*properties)[i]);
    }
    free(*properties);
    *properties = NULL;
    *properties_count = 0;

    return status;
}

static exyz_status_t frame_properties(
    parser_context_t* ctx,
    exyz_atom_property_t** properties,
    size_t* properties_count,
    exyz_info_t** info,
    size_t* info_count
) {
    exyz_status_t status = EXYZ_SUCCESS;
    *properties = NULL;
    *properties_count = 0;
    *info = NULL;
    *info_count = 0;

    while (ctx->current != ctx->length) {
        skip_whitespaces(ctx);
        if (ctx->current == ctx->length) {
            // no more property to read
            break;
        }

        char* key = NULL;
        status = info_key(ctx, &key);
        if (status != EXYZ_SUCCESS) {
            goto error;
        }

        skip_whitespaces(ctx);
        if (strcmp(key, "Properties") == 0) {
            free(key);

            status = atoms_properties(ctx, properties, properties_count);
            if (status != EXYZ_SUCCESS) {
                goto error;
            }
        } else {
            *info = alloc_one_more(*info, *info_count, sizeof(exyz_info_t));
            if (info == NULL) {
                free(key);
                status = error("failed to allocate memory");
                goto error;
            }

            *info_count += 1;
            exyz_info_t* current_info = *info + *info_count - 1;
            current_info->key = key;

            status = info_value(ctx, current_info);
            if (status != EXYZ_SUCCESS) {
                goto error;
            }

            // check that key=value items are separated by whitespace
            if (ctx->current != ctx->length) {
                char current = ctx->string[ctx->current];
                if (!is_whitespace(current)) {
                    return error("key=value pairs should be separated by whitespace, got '%c'", current);
                }
            }
        }
    }

    return EXYZ_SUCCESS;

error:
    for (size_t i=0; i<*properties_count; i++) {
        exyz_atom_property_free((*properties)[i]);
    }
    free(*properties);
    *properties = NULL;
    *properties_count = 0;

    for (size_t i=0; i<*info_count; i++) {
        exyz_info_free((*info)[i]);
    }
    free(*info);
    *info = NULL;
    *info_count = 0;

    return status;
}

/******************************************************************************/
/*                               I/O functions                                */
/******************************************************************************/


static exyz_status_t read_frame(FILE *fp, size_t* n_atoms, char** buffer, size_t* buffer_size) {
    int64_t start = ftell(fp);

    char line[80] = {0};
    size_t n_read = fread(line, 1, 80, fp);
    if (n_read == 0) {
        return error("failed to read a line");
    }

    int count = sscanf(line, "%zu", n_atoms);
    if (count == 0) {
        return error("failed to parse the number of atoms");
    }

    for (size_t i=0; i<sizeof(line); i++) {
        if (line[i] == '\n') {
            start += i;
            if (!fseek(fp, start, SEEK_SET)) {
                return error("failed to fseek");
            }
            break;
        }
    }

    // find the end of the current frame
    size_t expected_lines = (*n_atoms + 1);
    size_t n_lines = 0;
    while (n_lines != expected_lines) {
        if (feof(fp)) {
            return error("not enough lines in file for XYZ format");
        }

        // TODO: will this work on windows? And on unix with \r\n line ending?
        if (fgetc(fp) == '\n') {
            n_lines += 1;
        }
    }

    if (!fseek(fp, start, SEEK_SET)) {
        return error("failed to fseek");
    }

    size_t frame_size = (size_t)(ftell(fp) - start);
    *buffer = calloc(frame_size + 1, 1);

    n_read = fread(buffer, 1, frame_size, fp);
    if (n_read != frame_size) {
        free(*buffer);
        *buffer = NULL;
        return error("failed to read a whole frame");
    }

    return EXYZ_SUCCESS;
}

/******************************************************************************/
/*                     Public functions implementation                        */
/******************************************************************************/

exyz_status_t exyz_read_comment_line(
    const char* line,
    size_t line_length,
    exyz_atom_property_t** properties,
    size_t* properties_count,
    exyz_info_t** info,
    size_t* info_count
) {
    exyz_status_t status = EXYZ_SUCCESS;

    *properties_count = 0;
    *info_count = 0;

    assert(strlen(line) >= line_length);
    parser_context_t ctx = {
        .string = strdup(line),
        .length = line_length,
        .current = 0,
    };

    if (ctx.string == NULL) {
        return error("failed to allocate memory");
    }

    for (size_t i=0; i<ctx.length; i++) {
        if (ctx.string[i] == '\n' || ctx.string[i] == '\r') {
            free(ctx.string);
            return error("got a new line character inside the comment line");
        }
    }

    // Force strtod to use the C locale instead of whatever is in the user
    // environemnt. We store & reset the existing locale to not change the user
    // environemnt.
    const char* old_locale = setlocale(LC_ALL, NULL);
    setlocale(LC_ALL, "C");

    status = frame_properties(&ctx, properties, properties_count, info, info_count);

    free(ctx.string);
    setlocale(LC_ALL, old_locale);
    return status;
}

exyz_status_t exyz_read(
    FILE* fp,
    size_t* n_atoms,
    exyz_info_t** info,
    size_t* info_count,
    exyz_atom_array_t** arrays,
    size_t* arrays_count
) {
    char* frame = NULL;
    size_t frame_size = 0;
    exyz_status_t status = read_frame(fp, n_atoms, &frame, &frame_size);
    if (status != EXYZ_SUCCESS) {
        return status;
    }

    // separate the comment line and the atoms line by replacing the first
    // \n in frame with a null character
    char* comment = NULL;
    size_t comment_length = 0;
    char* atoms = NULL;
    for (size_t i=0; i<frame_size; i++) {
        if (frame[i] == '\n') {
            frame[i] = '\0';

            assert(i + 1 < frame_size);
            atoms = frame + (i + 1);
            comment = frame;
            comment_length = i;
            break;
        }
    }

    exyz_atom_property_t* properties = NULL;
    size_t properties_count = 0;
    status = exyz_read_comment_line(comment, comment_length, &properties, &properties_count, info, info_count);
    if (status != EXYZ_SUCCESS) {
        goto cleanup;
    }

    status = error("reading atoms is not yet implemented");
    goto cleanup;

cleanup:
    free(frame);
    free(properties);
    return status;
}
