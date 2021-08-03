# Extended XYZ formal grammar and parser

This repository contains a [BNF-like formal grammar](extended-xyz.bnf) for the
extended XYZ format, created from the plain text specification at
https://github.com/libAtoms/extxyz.

It also contains a parser for this grammar implemented in C, and a Python
binding to the C parser.

**Installing the Python module**:

```bash
python setup.py install
# or
pip install .
```

Example usage:

```python
from exyz import parse_comment_line


line = (
    'Properties=species:S:1:pos:R:3 s=string s2="test \\" string" b=T r=3.42 i=-33 '
    + 'a_i=[3, 4, 5] a_r=[-22, 4.2] a_b="T T F" a_s={a b c}'
)
properties, info = parse_comment_line(line)

print("Atomic properties:")
print(properties.to_dict())
# output:
# {'species': {'type': 'string', 'count': 1}, 'pos': {'type': 'real', 'count': 3}}

print("\nFrame properties:")
print(info.to_dict())
# output:
# {'s': 'string', 's2': 'test " string', 'b': True, 'r': 3.42, 'i': -33,
# 'a_i': array([3, 4, 5]), 'a_r': array([-22. ,   4.2]),
# 'a_b': array([ True,  True, False]), 'a_s': array(['a', 'b', 'c'], dtype='<U1')}
```

**Installing the C library**:

TODO

**Running Python tests**:

TODO

**Running C tests**:

```bash
mkdir build && cd build
cmake -DEXYZ_BUILD_TESTS=ON -DEXYZ_SANITIZERS=ON ..
make
ctest
```
