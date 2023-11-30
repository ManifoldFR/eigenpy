import std_array


ints = std_array.get_arr_3_ints()
print(ints[0])
print(ints[1])
print(ints[2])

_ints_slice = ints[1:3]
print("Printing slice...")
for el in _ints_slice:
    print(el)

ref = [1, 2, 3]
assert len(ref[1:2]) == 1  # sanity check

assert len(_ints_slice) == 2, "Slice size should be 1, got %d" % len(_ints_slice)
assert _ints_slice[0] == 2
assert _ints_slice[1] == 3

# print(ints.tolist())

vecs = std_array.get_arr_3_vecs()
assert len(vecs) == 3
# print(vecs[0])
# print(vecs[1])
# print(vecs[2])

## Tests for the full-size slice

v2 = vecs[:]
assert isinstance(v2, std_array.StdVec_VectorXd)
assert len(v2) == 3
print(v2.tolist())
print(v2[0])
