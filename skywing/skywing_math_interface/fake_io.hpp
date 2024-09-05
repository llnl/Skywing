#ifndef SKYWING_MATH_FAKEIO
#define SKYWING_MATH_FAKEIO

#include "skywing_mid/associative_vector.hpp"

using namespace skywing;

// In the real i/o this values will be based on the values defined in the
// processor. For now, we use these hardcoded types.
using index_t = uint32_t;
using scalar_t = double;

using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
using AssociativeMatrix = AssociativeVector<index_t, ClosedVector, false>;

// Defines the rows of the matrix
ClosedVector c0 = ClosedVector(
    {{0, 50}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0}, {8, 0}});
ClosedVector c1 = ClosedVector(
    {{0, 0}, {1, 20}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 1}, {7, 1}, {8, 0}});
ClosedVector c2 = ClosedVector(
    {{0, 0}, {1, 0}, {2, 100}, {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0}, {8, 0}});
ClosedVector c3 = ClosedVector(
    {{0, 0}, {1, 0}, {2, 0}, {3, 19}, {4, 0}, {5, 0}, {6, 0}, {7, 0}, {8, 0}});
ClosedVector c4 = ClosedVector(
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 10}, {5, 0}, {6, 0}, {7, 0}, {8, 0}});
ClosedVector c5 = ClosedVector(
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 10}, {6, 0}, {7, 0}, {8, 0}});
ClosedVector c6 = ClosedVector(
    {{0, 5}, {1, 0}, {2, 0}, {3, 0}, {4, 2}, {5, 0}, {6, 20}, {7, 0}, {8, 0}});
ClosedVector c7 = ClosedVector(
    {{0, 1}, {1, 0}, {2, 0}, {3, 0}, {4, 2}, {5, 0}, {6, 0}, {7, 30}, {8, 0}});
ClosedVector c8 = ClosedVector(
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 2}, {5, 0}, {6, 0}, {7, 0}, {8, 10}});

// Defines the right hand side of the system
ClosedVector b0 = ClosedVector({{0, 1}, {3, 1}, {6, 1}});
ClosedVector b1 = ClosedVector({{1, 1}, {4, 1}, {7, 1}});
ClosedVector b2 = ClosedVector({{2, 2}, {5, 2}, {8, 5}});



std::unordered_map<uint32_t, std::vector<uint32_t>> read_partition_from_file()
{
    return {{0, {0,3,6}},{ 1,{1,4,7}}, {2,{2,5,8}}};
}

// These functions are used for testing before i/o functionality is added
AssociativeMatrix read_matrix_from_file(size_t machine_num)
{
    if (machine_num == 0)
        return AssociativeMatrix({{0, c0}, {3, c3}, {6, c6}});
    if (machine_num == 1)
        return AssociativeMatrix({{1, c1}, {4, c4}, {7, c7}});
    else
        return AssociativeMatrix({{2, c2}, {5, c5}, {8, c8}});
}

ClosedVector read_rhs_from_file(size_t machine_num)
{
    if (machine_num == 0)
        return b0;
    if (machine_num == 1)
        return b1;
    else
        return b2;
}



#endif // SKYWING_MATH_FAKEIO