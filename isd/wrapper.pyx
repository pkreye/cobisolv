# cython: c_string_type=unicode, c_string_encoding=utf8

from libc.stdint cimport int64_t
from libc.stdint cimport uint32_t
from libcpp cimport bool

cdef extern from "cobidualres.c":        
    ctypedef struct nums:
        int Q[46][46]
        int energy
        int spins[46]
        int debug
    void solveQ(nums* obj)

def w_solveQ(nums obj):
    cdef nums *nums_ptr = &obj
    solveQ(nums_ptr)
    return obj