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


TEST_CASE("Atomic Properties specification") {
    exyz_atom_property_t* properties = nullptr;
    size_t properties_count = 0;

    exyz_info_t* info = nullptr;
    size_t info_count = 0;

    std::string line = "Properties=species:S:1:pos:R:3";
    auto status = exyz_read_comment_line(
        line.data(), line.size(), &properties, &properties_count, &info, &info_count
    );
    REQUIRE(status == EXYZ_SUCCESS);
    CHECK(info_count == 0);

    REQUIRE(properties_count == 2);
    CHECK(properties[0].key == std::string("species"));
    CHECK(properties[0].count == 1);
    CHECK(properties[0].type == EXYZ_STRING);

    CHECK(properties[1].key == std::string("pos"));
    CHECK(properties[1].count == 3);
    CHECK(properties[1].type == EXYZ_REAL);

    free_data(properties, properties_count, info, info_count);

    // quoted & whitespaces
    line = "\"Properties\"   =\t\t  \"species:S:1:pos:R:3\"\t    ";
    status = exyz_read_comment_line(
        line.data(), line.size(), &properties, &properties_count, &info, &info_count
    );
    REQUIRE(status == EXYZ_SUCCESS);
    CHECK(info_count == 0);

    REQUIRE(properties_count == 2);
    CHECK(properties[0].key == std::string("species"));
    CHECK(properties[0].count == 1);
    CHECK(properties[0].type == EXYZ_STRING);

    CHECK(properties[1].key == std::string("pos"));
    CHECK(properties[1].count == 3);
    CHECK(properties[1].type == EXYZ_REAL);

    free_data(properties, properties_count, info, info_count);


    // non standard
    line = "Properties=foo:I:5:species:S:1:bar:L:2:pos:R:3";
    status = exyz_read_comment_line(
        line.data(), line.size(), &properties, &properties_count, &info, &info_count
    );
    REQUIRE(status == EXYZ_SUCCESS);
    CHECK(info_count == 0);

    REQUIRE(properties_count == 4);
    CHECK(properties[0].key == std::string("foo"));
    CHECK(properties[0].count == 5);
    CHECK(properties[0].type == EXYZ_INTEGER);

    CHECK(properties[1].key == std::string("species"));
    CHECK(properties[1].count == 1);
    CHECK(properties[1].type == EXYZ_STRING);

    CHECK(properties[2].key == std::string("bar"));
    CHECK(properties[2].count == 2);
    CHECK(properties[2].type == EXYZ_BOOL);

    CHECK(properties[3].key == std::string("pos"));
    CHECK(properties[3].count == 3);
    CHECK(properties[3].type == EXYZ_REAL);

    free_data(properties, properties_count, info, info_count);
}


TEST_CASE("Atomic Properties errors") {
    // TODO

    // extraneous :

    // invalid ident

    // invalid type

    // invalid count
}
