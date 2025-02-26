# cython: c_string_type=unicode, c_string_encoding=utf8

from libc.stdint cimport int64_t
from libc.stdint cimport uint32_t
from libc.stdlib cimport malloc
from libcpp cimport bool

cdef extern from "cobi.c":
    ctypedef struct CobiInput:
        int Q[46][46]
        int debug
    ctypedef struct CobiOutput:
        int problem_id
        int spins[46]
        int energy
        int core_id
    void solveQ(int device, CobiInput* obj, CobiOutput *result)
    void init_device(int device)
    void submit_problem(int device, CobiInput *obj)
    int read_result(int device, CobiOutput *result, bool wait_for_result)


def w_open_device(int device):
    cobi_open_device(device)

def w_close_device(int device):
    close(device)

def w_solveQ(int device, CobiInput obj, CobiOutput output_obj):
    cdef CobiInput *input_ptr = &obj
    cdef CobiOutput *output_ptr = &output_obj
    solveQ(device, input_ptr, output_ptr)
    return output_obj

def w_reset_device(int device):
    init_device(device)

def w_submit_problem(int device, CobiInput obj):
    cdef CobiInput *input_ptr = &obj
    submit_problem(device, input_ptr)

def w_read_result(int device):
    cdef CobiOutput result
    cdef int count = read_result(device, &result, False)
    if (count != 0):
        return result
    else:
        return None

def w_read_result_blocking(int device):
    cdef CobiOutput result
    read_result(device, &result, True)
    return result
