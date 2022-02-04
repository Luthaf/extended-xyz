import logging
import sys
import json

import lark

lark.logger.setLevel(logging.DEBUG)

with open("extended-xyz-comment.lark") as fd:
    grammar = fd.read()


class CleanupTransformer(lark.Transformer):
    def unsigned_integer(self, value):
        tree = lark.Tree("unsigned_integer", value)
        tokens = tree.scan_values(lambda v: isinstance(v, lark.Token))
        return int("".join(tokens))

    def integer(self, value):
        if len(value) == 2:
            if value[0].value == "-":
                return -value[1]
            elif value[0].value == "+":
                return value[1]
            else:
                raise Exception("should be unreachable")
        elif len(value) == 1:
            return value[0]
        else:
            raise Exception("should be unreachable")

    def float(self, value):
        tree = lark.Tree("float", value)
        tokens = tree.scan_values(lambda v: isinstance(v, (lark.Token, int)))
        value = "".join(map(str, tokens))
        value = value.replace("D", "e")
        value = value.replace("d", "e")
        return float(value)

    def bare_string(self, value):
        tree = lark.Tree("bare_string", value)
        tokens = tree.scan_values(lambda v: isinstance(v, lark.Token))
        return "".join(tokens)

    def escaped_chars(self, value):
        escaped = value[1]
        if isinstance(escaped, lark.Token):
            assert escaped.value == '"'
            # keep it escaped for now for nicer printing below
            # TODO: replace this by `return '"'`
            return '\\"'
        elif isinstance(escaped, lark.Tree):
            value = escaped.children[0].value
            if value == "n":
                # keep it escaped for now for nicer printing below
                # TODO: replace this by `return "\n"`
                return "\\n"
            else:
                return value
        else:
            raise Exception("should be unreachable")

    def quoted_string(self, value):
        tree = lark.Tree("quoted_string", value)
        tokens = tree.scan_values(lambda v: isinstance(v, (str, lark.Token)))
        return '"' + "".join(tokens) + '"'

    def string(self, value):
        return value[0]

    def identifier(self, value):
        tree = lark.Tree("identifier", value)
        tokens = tree.scan_values(lambda v: isinstance(v, lark.Token))
        return "".join(tokens)

    def logical(self, value):
        tree = lark.Tree("logical", value)
        tokens = tree.scan_values(lambda v: isinstance(v, lark.Token))
        first = "".join(tokens).lower()[0]
        if first == "f":
            return False
        elif first == "t":
            return True
        else:
            raise Exception("unexpected logical literal")

    def whitespaces(self, value):
        return lark.visitors.Discard

    def new_style_array1d(self, value):
        return value[0].children

    def new_style_array2d(self, value):
        return value

    def new_style_array(self, value):
        return value[0]

    def ws_separated_strings(self, value):
        return value

    def ws_separated_logicals(self, value):
        return value

    def ws_separated_integers(self, value):
        return value

    def ws_separated_floats(self, value):
        return value

    def old_style_array(self, value):
        data = value[0]
        if len(data) == 1:
            return data[0]
        else:
            return data

    def array(self, value):
        return value[0]

    def atom_property_type(self, value):
        kind = value[0].value
        if kind == "S":
            return "str"
        elif kind == "I":
            return "int"
        elif kind == "L":
            return "bool"
        elif kind == "R":
            return "float"
        else:
            raise Exception("should be unreachable")


class ExtendedXyzAsDict(lark.Visitor):
    def visit(self, tree):
        self._exyz_dict = {}
        self._exyz_properties_seen = False
        super().visit(tree)
        return self._exyz_dict

    def generic_frame_property(self, value):
        key = value.children[0].children[0]
        value = value.children[1].children[0]

        if key in self._exyz_dict:
            raise Exception("can not have the same property multiple time")

        self._exyz_dict[key] = value

    def atom_properties_list(self, value):
        if self._exyz_properties_seen:
            raise Exception("can not have multiple 'Properties' in comment line")

        self._exyz_properties_seen = True
        properties = []
        for prop in value.children:
            properties.append(
                {
                    "name": prop.children[0],
                    "type": prop.children[1],
                    "count": prop.children[2],
                }
            )

        self._exyz_dict["Properties"] = properties


with open(sys.argv[1]) as fd:
    data = fd.read().strip()

parser = lark.Lark(grammar, parser="earley", start="comment_line")
tree = parser.parse(data)
tree = CleanupTransformer().transform(tree)
print(tree.pretty())

print(json.dumps(ExtendedXyzAsDict().visit(tree), indent=2))
