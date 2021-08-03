#include <string>
#include <utility>

#include <catch.hpp>
#include <exyz.h>

static void free_data(
    exyz_atom_property_t* properties,
    size_t properties_count,
    exyz_info_t* info,
    size_t info_count
) {
    for (size_t i=0; i<info_count; i++) {
        exyz_info_free(info[i]);
    }
    free(info);
    info = nullptr;

    for (size_t i=0; i<properties_count; i++) {
        exyz_atom_property_free(properties[i]);
    }
    free(properties);
    properties = nullptr;
}


TEST_CASE("bool properties") {
    exyz_atom_property_t* properties = nullptr;
    size_t properties_count = 0;

    exyz_info_t* info = nullptr;
    size_t info_count = 0;

    SECTION("True") {
        std::string VALUES[] = { "T", "True", "TRUE", "true" };

        for (auto value: VALUES) {
            auto line = "Properties=species:S:1:pos:R:3 key=" + value;
            auto status = exyz_read_comment_line(
                line.data(), line.size(), &properties, &properties_count, &info, &info_count
            );
            REQUIRE(status == EXYZ_SUCCESS);

            REQUIRE(info_count == 1);
            CHECK(info[0].key == std::string("key"));
            REQUIRE(info[0].type == EXYZ_BOOL);
            CHECK(info[0].data.boolean == true);

            free_data(properties, properties_count, info, info_count);
        }
    }

    SECTION("False") {
        std::string VALUES[] = { "F", "False", "FALSE", "false" };

        for (auto value: VALUES) {
            std::string line = "Properties=species:S:1:pos:R:3 key=" + value;
            auto status = exyz_read_comment_line(
                line.data(), line.size(), &properties, &properties_count, &info, &info_count
            );
            REQUIRE(status == EXYZ_SUCCESS);

            REQUIRE(info_count == 1);
            CHECK(info[0].key == std::string("key"));
            REQUIRE(info[0].type == EXYZ_BOOL);
            CHECK(info[0].data.boolean == false);

            free_data(properties, properties_count, info, info_count);
        }
    }

    SECTION("string looking like boolean") {
        std::string VALUES[] = { "f", "t", "FaLsE", "TrUe" };

        for (auto value: VALUES) {
            auto line = "Properties=species:S:1:pos:R:3 key=" + value;
            auto status = exyz_read_comment_line(
                line.data(), line.size(), &properties, &properties_count, &info, &info_count
            );
            REQUIRE(status == EXYZ_SUCCESS);

            REQUIRE(info_count == 1);
            CHECK(info[0].key == std::string("key"));
            REQUIRE(info[0].type == EXYZ_STRING);
            CHECK(info[0].data.string == value);

            free_data(properties, properties_count, info, info_count);
        }
    }

    SECTION("Whitespace") {
        auto check_bool_with_whitespace = [&](std::string line){
            auto status = exyz_read_comment_line(
                line.data(), line.size(), &properties, &properties_count, &info, &info_count
            );
            REQUIRE(status == EXYZ_SUCCESS);

            REQUIRE(info_count == 1);
            CHECK(info[0].key == std::string("key"));
            REQUIRE(info[0].type == EXYZ_BOOL);
            CHECK(info[0].data.boolean == true);

            free_data(properties, properties_count, info, info_count);
        };

        check_bool_with_whitespace(
            "Properties=species:S:1:pos:R:3 key=T    "
        );

        check_bool_with_whitespace(
            "Properties=species:S:1:pos:R:3 key=\t  T"
        );

        check_bool_with_whitespace(
            "Properties=species:S:1:pos:R:3 key   =T"
        );

        check_bool_with_whitespace(
            "Properties=species:S:1:pos:R:3 \t   \t  key =  \t   T   \t \t"
        );
    }
}

TEST_CASE("string properties") {
    exyz_atom_property_t* properties = nullptr;
    size_t properties_count = 0;

    exyz_info_t* info = nullptr;
    size_t info_count = 0;

    std::string WEIRD_BARE_STRINGS[] = {
        "!#$%&'()*+-./0123456789:;<>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~",
        "a/b",
        "a@b",
        "TRuE",
        "1.3k7",
        "1.e7",
        "-2.75e",
        "+2.75e-",
        "+2.75e+",
        "0012.1e-6",
    };

    auto check_string_value = [&](std::string name, std::string expected) {
        REQUIRE(info_count == 1);
        CHECK(info[0].key == name);
        REQUIRE(info[0].type == EXYZ_STRING);
        CHECK(info[0].data.string == expected);
    };

    SECTION("Bare strings") {
        std::string line = "Properties=species:S:1:pos:R:3 key=string\t";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);
        check_string_value("key", "string");
        free_data(properties, properties_count, info, info_count);

        for (auto value: WEIRD_BARE_STRINGS) {
            auto line = "Properties=species:S:1:pos:R:3 key=" + value;
            auto status = exyz_read_comment_line(
                line.data(), line.size(), &properties, &properties_count, &info, &info_count
            );
            REQUIRE(status == EXYZ_SUCCESS);
            check_string_value("key", value);
            free_data(properties, properties_count, info, info_count);
        }
    }

    SECTION("Quoted strings") {
        std::string line = "Properties=species:S:1:pos:R:3 key=\"string\"    ";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);
        check_string_value("key", "string");
        free_data(properties, properties_count, info, info_count);

        for (auto value: WEIRD_BARE_STRINGS) {
            auto line = "Properties=species:S:1:pos:R:3 key=\"" + value + "\"";
            auto status = exyz_read_comment_line(
                line.data(), line.size(), &properties, &properties_count, &info, &info_count
            );
            REQUIRE(status == EXYZ_SUCCESS);
            check_string_value("key", value);
            free_data(properties, properties_count, info, info_count);
        }

        std::pair<std::string, std::string> WEIRD_QUOTED_STRINGS[] = {
            {
                "!\\\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",
                "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",
            },
            {
                "line one\\nline two", "line one\nline two",
            },
            {
                "\\\"a\\\"", "\"a\"",
            },
            {
                "a\\b", "ab",
            }
        };

        for (auto value: WEIRD_QUOTED_STRINGS) {
            auto line = "Properties=species:S:1:pos:R:3 key=\"" + value.first + "\"";
            auto status = exyz_read_comment_line(
                line.data(), line.size(), &properties, &properties_count, &info, &info_count
            );
            REQUIRE(status == EXYZ_SUCCESS);
            check_string_value("key", value.second);
            free_data(properties, properties_count, info, info_count);
        }
    }
}

TEST_CASE("Integer properties") {
    exyz_atom_property_t* properties = nullptr;
    size_t properties_count = 0;

    exyz_info_t* info = nullptr;
    size_t info_count = 0;

    SECTION("Normal values") {
        auto check_integer_value = [&](std::string name, int64_t expected) {
            REQUIRE(info_count == 1);
            CHECK(info[0].key == name);
            REQUIRE(info[0].type == EXYZ_INTEGER);
            CHECK(info[0].data.integer == expected);
        };

        std::string line = "Properties=species:S:1:pos:R:3 key=33\t";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);
        check_integer_value("key", 33);
        free_data(properties, properties_count, info, info_count);



        line = "Properties=species:S:1:pos:R:3 key=-42  ";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);
        check_integer_value("key", -42);
        free_data(properties, properties_count, info, info_count);



        line = "Properties=species:S:1:pos:R:3 key=+555555";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);
        check_integer_value("key", 555555);
        free_data(properties, properties_count, info, info_count);
    }

    SECTION("String looking like integers") {
        std::string VALUES[] = { "++44", "--33", "22ff", "-23S" };

        for (auto value: VALUES) {
            auto line = "Properties=species:S:1:pos:R:3 key=" + value;
            auto status = exyz_read_comment_line(
                line.data(), line.size(), &properties, &properties_count, &info, &info_count
            );
            REQUIRE(status == EXYZ_SUCCESS);

            REQUIRE(info_count == 1);
            CHECK(info[0].key == std::string("key"));
            REQUIRE(info[0].type == EXYZ_STRING);
            CHECK(info[0].data.string == value);

            free_data(properties, properties_count, info, info_count);
        }
    }
}


TEST_CASE("Real properties") {
    exyz_atom_property_t* properties = nullptr;
    size_t properties_count = 0;

    exyz_info_t* info = nullptr;
    size_t info_count = 0;

    SECTION("Normal values") {
        auto check_real_value = [&](std::string name, double expected) {
            REQUIRE(info_count == 1);
            CHECK(info[0].key == name);
            REQUIRE(info[0].type == EXYZ_REAL);
            CHECK(info[0].data.real == expected);
        };

        std::string line = "Properties=species:S:1:pos:R:3 key=33.3\t";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);
        check_real_value("key", 33.3);
        free_data(properties, properties_count, info, info_count);



        line = "Properties=species:S:1:pos:R:3 key=-42e-2  ";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);
        check_real_value("key", -42e-2);
        free_data(properties, properties_count, info, info_count);



        line = "Properties=species:S:1:pos:R:3 key=+55.5d+2";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);
        check_real_value("key", 55.5e2);
        free_data(properties, properties_count, info, info_count);
    }
}


TEST_CASE("Multiple values") {
    exyz_atom_property_t* properties = nullptr;
    size_t properties_count = 0;

    exyz_info_t* info = nullptr;
    size_t info_count = 0;

    std::string line = "Properties=species:S:1:pos:R:3 s=string b=T r=3.42 i=-33";
    auto status = exyz_read_comment_line(
        line.data(), line.size(), &properties, &properties_count, &info, &info_count
    );
    REQUIRE(status == EXYZ_SUCCESS);

    REQUIRE(info_count == 4);

    CHECK(info[0].key == std::string("s"));
    REQUIRE(info[0].type == EXYZ_STRING);
    CHECK(info[0].data.string == std::string("string"));

    CHECK(info[1].key == std::string("b"));
    REQUIRE(info[1].type == EXYZ_BOOL);
    CHECK(info[1].data.boolean == true);

    CHECK(info[2].key == std::string("r"));
    REQUIRE(info[2].type == EXYZ_REAL);
    CHECK(info[2].data.real == 3.42);

    CHECK(info[3].key == std::string("i"));
    REQUIRE(info[3].type == EXYZ_INTEGER);
    CHECK(info[3].data.integer == -33);

    free_data(properties, properties_count, info, info_count);
}

TEST_CASE("Array properties -- new style -- 2D") {
    exyz_atom_property_t* properties = nullptr;
    size_t properties_count = 0;

    exyz_info_t* info = nullptr;
    size_t info_count = 0;

    SECTION("integers") {
        std::string line = "Properties=species:S:1:pos:R:3 key=  [  [ 1  , 2 ]   ,  [\t3 , -4  ]  ]";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        exyz_array_t array = info[0].data.array;
        CHECK(array.type == EXYZ_INTEGER);
        REQUIRE(array.nrows == 2);
        REQUIRE(array.ncols == 2);
        CHECK(array.data.integer[0] == 1);
        CHECK(array.data.integer[1] == 2);
        CHECK(array.data.integer[2] == 3);
        CHECK(array.data.integer[3] == -4);

        free_data(properties, properties_count, info, info_count);
    }

    SECTION("real") {
        std::string line = "Properties=species:S:1:pos:R:3 key=[[1, 2], [3e3, 5.5]]";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        exyz_array_t array = info[0].data.array;
        CHECK(array.type == EXYZ_REAL);
        REQUIRE(array.nrows == 2);
        REQUIRE(array.ncols == 2);
        CHECK(array.data.real[0] == 1);
        CHECK(array.data.real[1] == 2);
        CHECK(array.data.real[2] == 3e3);
        CHECK(array.data.real[3] == 5.5);

        free_data(properties, properties_count, info, info_count);
    }

    SECTION("bool") {
        std::string line = "Properties=species:S:1:pos:R:3 key=[[False, TRUE, F], [T, F, F]]";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        exyz_array_t array = info[0].data.array;
        CHECK(array.type == EXYZ_BOOL);
        REQUIRE(array.nrows == 2);
        REQUIRE(array.ncols == 3);
        CHECK(array.data.boolean[0] == false);
        CHECK(array.data.boolean[1] == true);
        CHECK(array.data.boolean[2] == false);
        CHECK(array.data.boolean[3] == true);
        CHECK(array.data.boolean[4] == false);
        CHECK(array.data.boolean[5] == false);

        free_data(properties, properties_count, info, info_count);
    }

    SECTION("strings") {
        std::string line = "Properties=species:S:1:pos:R:3 key=[[3, 33.4, -4], [True, bar, \"string  \"]]";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        auto array = info[0].data.array;
        CHECK(array.type == EXYZ_STRING);
        REQUIRE(array.nrows == 2);
        REQUIRE(array.ncols == 3);
        CHECK(array.data.string[0] == std::string("3"));
        CHECK(array.data.string[1] == std::string("33.4"));
        CHECK(array.data.string[2] == std::string("-4"));
        CHECK(array.data.string[3] == std::string("True"));
        CHECK(array.data.string[4] == std::string("bar"));
        CHECK(array.data.string[5] == std::string("string  "));

        free_data(properties, properties_count, info, info_count);
    }

    SECTION("errors") {
        // TODO: missing comma

        // TODO: extraneous comma

        // TODO: not matching sizes in 2D array
    }
}

TEST_CASE("Array properties -- new style -- 1D") {
    exyz_atom_property_t* properties = nullptr;
    size_t properties_count = 0;

    exyz_info_t* info = nullptr;
    size_t info_count = 0;

    SECTION("integers") {
        std::string line = "Properties=species:S:1:pos:R:3 key=  [    3, -4  , \t 5    ]";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        exyz_array_t array = info[0].data.array;
        CHECK(array.type == EXYZ_INTEGER);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 3);
        CHECK(array.data.integer[0] == 3);
        CHECK(array.data.integer[1] == -4);
        CHECK(array.data.integer[2] == 5);

        free_data(properties, properties_count, info, info_count);
    }

    SECTION("real") {
        std::string line = "Properties=species:S:1:pos:R:3 key=[3e3, 5.5]";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        exyz_array_t array = info[0].data.array;
        CHECK(array.type == EXYZ_REAL);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 2);
        CHECK(array.data.real[0] == 3e3);
        CHECK(array.data.real[1] == 5.5);

        free_data(properties, properties_count, info, info_count);

        // mixed data types
        line = "Properties=species:S:1:pos:R:3 key=[3, -4, 5.5]";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        array = info[0].data.array;
        CHECK(array.type == EXYZ_REAL);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 3);
        CHECK(array.data.real[0] == 3);
        CHECK(array.data.real[1] == -4);
        CHECK(array.data.real[2] == 5.5);

        free_data(properties, properties_count, info, info_count);
    }


    SECTION("bool") {
        std::string line = "Properties=species:S:1:pos:R:3 key=[False, TRUE, F]";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        exyz_array_t array = info[0].data.array;
        CHECK(array.type == EXYZ_BOOL);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 3);
        CHECK(array.data.boolean[0] == false);
        CHECK(array.data.boolean[1] == true);
        CHECK(array.data.boolean[2] == false);

        free_data(properties, properties_count, info, info_count);
    }

    SECTION("strings") {
        std::string line = "Properties=species:S:1:pos:R:3 key=[bar, baz]";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        exyz_array_t array = info[0].data.array;
        CHECK(array.type == EXYZ_STRING);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 2);
        CHECK(array.data.string[0] == std::string("bar"));
        CHECK(array.data.string[1] == std::string("baz"));

        free_data(properties, properties_count, info, info_count);

        // quoted & unquoted strings
        line = "Properties=species:S:1:pos:R:3 key=[bar, \"a long string \\\" \"]";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        array = info[0].data.array;
        CHECK(array.type == EXYZ_STRING);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 2);
        CHECK(array.data.string[0] == std::string("bar"));
        CHECK(array.data.string[1] == std::string("a long string \" "));

        free_data(properties, properties_count, info, info_count);

        // mixed data types
        line = "Properties=species:S:1:pos:R:3 key=[3, 33.4, True, bar, \"string\"]";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        array = info[0].data.array;
        CHECK(array.type == EXYZ_STRING);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 5);
        CHECK(array.data.string[0] == std::string("3"));
        CHECK(array.data.string[1] == std::string("33.4"));
        CHECK(array.data.string[2] == std::string("True"));
        CHECK(array.data.string[3] == std::string("bar"));
        CHECK(array.data.string[4] == std::string("string"));

        free_data(properties, properties_count, info, info_count);
    }


    SECTION("error") {
        // TODO: missing comma

        // TODO: extraneous comma

        // TODO: not matching sizes in 2D array
    }
}

TEST_CASE("Array properties -- old style with quote") {
    exyz_atom_property_t* properties = nullptr;
    size_t properties_count = 0;

    exyz_info_t* info = nullptr;
    size_t info_count = 0;

    SECTION("integers") {
        std::string line = "Properties=species:S:1:pos:R:3 key= \"   3 -4   \t 5    \"";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        exyz_array_t array = info[0].data.array;
        CHECK(array.type == EXYZ_INTEGER);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 3);
        CHECK(array.data.integer[0] == 3);
        CHECK(array.data.integer[1] == -4);
        CHECK(array.data.integer[2] == 5);

        free_data(properties, properties_count, info, info_count);

        // single element array
        line = "Properties=species:S:1:pos:R:3 key= \"\t 5    \"";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_INTEGER);
        CHECK(info[0].data.integer == 5);
    }

    SECTION("real") {
        std::string line = "Properties=species:S:1:pos:R:3 key=\"3e3 5.5\"";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        exyz_array_t array = info[0].data.array;
        CHECK(array.type == EXYZ_REAL);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 2);
        CHECK(array.data.real[0] == 3e3);
        CHECK(array.data.real[1] == 5.5);

        free_data(properties, properties_count, info, info_count);

        // mixed data types
        line = "Properties=species:S:1:pos:R:3 key=\"3 -4 5.5\"";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        array = info[0].data.array;
        CHECK(array.type == EXYZ_REAL);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 3);
        CHECK(array.data.real[0] == 3);
        CHECK(array.data.real[1] == -4);
        CHECK(array.data.real[2] == 5.5);

        free_data(properties, properties_count, info, info_count);

        // single element array
        line = "Properties=species:S:1:pos:R:3 key= \"\t 5.5    \"";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_REAL);
        CHECK(info[0].data.real == 5.5);
    }


    SECTION("bool") {
        std::string line = "Properties=species:S:1:pos:R:3 key=\"False TRUE F\"";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        exyz_array_t array = info[0].data.array;
        CHECK(array.type == EXYZ_BOOL);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 3);
        CHECK(array.data.boolean[0] == false);
        CHECK(array.data.boolean[1] == true);
        CHECK(array.data.boolean[2] == false);

        free_data(properties, properties_count, info, info_count);

        // single element array
        line = "Properties=species:S:1:pos:R:3 key= \"\t T    \"";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_BOOL);
        CHECK(info[0].data.boolean == true);
    }

    SECTION("errors") {
        // TODO: missing end of array
    }
}

TEST_CASE("Array properties -- old style with brackets") {
    exyz_atom_property_t* properties = nullptr;
    size_t properties_count = 0;

    exyz_info_t* info = nullptr;
    size_t info_count = 0;

    SECTION("strings") {
        std::string line = "Properties=species:S:1:pos:R:3 key={\t   bar  \t  baz\t  }";
        auto status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        exyz_array_t array = info[0].data.array;
        CHECK(array.type == EXYZ_STRING);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 2);
        CHECK(array.data.string[0] == std::string("bar"));
        CHECK(array.data.string[1] == std::string("baz"));

        free_data(properties, properties_count, info, info_count);

        // quoted & unquoted strings
        line = "Properties=species:S:1:pos:R:3 key={bar \"a long string \\\" \"}";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        array = info[0].data.array;
        CHECK(array.type == EXYZ_STRING);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 2);
        CHECK(array.data.string[0] == std::string("bar"));
        CHECK(array.data.string[1] == std::string("a long string \" "));

        free_data(properties, properties_count, info, info_count);

        // mixed data types
        line = "Properties=species:S:1:pos:R:3 key={3 33.4 True bar \"string\"}";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 1);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_ARRAY);

        array = info[0].data.array;
        CHECK(array.type == EXYZ_STRING);
        REQUIRE(array.nrows == 1);
        REQUIRE(array.ncols == 5);
        CHECK(array.data.string[0] == std::string("3"));
        CHECK(array.data.string[1] == std::string("33.4"));
        CHECK(array.data.string[2] == std::string("True"));
        CHECK(array.data.string[3] == std::string("bar"));
        CHECK(array.data.string[4] == std::string("string"));

        free_data(properties, properties_count, info, info_count);

        // single element array
        line = "Properties=species:S:1:pos:R:3 key= { Foo} key2={\"bar  \"}";
        status = exyz_read_comment_line(
            line.data(), line.size(), &properties, &properties_count, &info, &info_count
        );
        REQUIRE(status == EXYZ_SUCCESS);

        REQUIRE(info_count == 2);
        CHECK(info[0].key == std::string("key"));
        REQUIRE(info[0].type == EXYZ_STRING);
        CHECK(info[0].data.string == std::string("Foo"));

        CHECK(info[1].key == std::string("key2"));
        REQUIRE(info[1].type == EXYZ_STRING);
        CHECK(info[1].data.string == std::string("bar  "));
    }

    SECTION("errors") {
        // TODO: missing end of array
    }
}
