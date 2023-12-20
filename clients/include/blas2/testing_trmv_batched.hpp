/* ************************************************************************
 * Copyright (C) 2018-2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell cop-
 * ies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IM-
 * PLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNE-
 * CTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *


 *
 * ************************************************************************ */

#pragma once

#include "bytes.hpp"
#include "cblas_interface.hpp"
#include "flops.hpp"
#include "near.hpp"
#include "norm.hpp"
#include "rocblas.hpp"
#include "rocblas_datatype2string.hpp"
#include "rocblas_init.hpp"
#include "rocblas_math.hpp"
#include "rocblas_matrix.hpp"
#include "rocblas_random.hpp"
#include "rocblas_test.hpp"
#include "rocblas_vector.hpp"
#include "unit.hpp"
#include "utility.hpp"

template <typename T>
void testing_trmv_batched_bad_arg(const Arguments& arg)
{
    auto rocblas_trmv_batched_fn
        = arg.api == FORTRAN ? rocblas_trmv_batched<T, true> : rocblas_trmv_batched<T, false>;

    const rocblas_int       N           = 100;
    const rocblas_int       lda         = 100;
    const rocblas_int       incx        = 1;
    const rocblas_int       batch_count = 1;
    const rocblas_operation transA      = rocblas_operation_none;
    const rocblas_fill      uplo        = rocblas_fill_lower;
    const rocblas_diagonal  diag        = rocblas_diagonal_non_unit;

    rocblas_local_handle handle{arg};

    // Allocate device memory
    device_batch_matrix<T> dA(N, N, lda, batch_count);
    device_batch_vector<T> dx(N, incx, batch_count);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dA.memcheck());
    CHECK_DEVICE_ALLOCATION(dx.memcheck());

    // Checks.
    EXPECT_ROCBLAS_STATUS(rocblas_trmv_batched_fn(handle,
                                                  rocblas_fill_full,
                                                  transA,
                                                  diag,
                                                  N,
                                                  dA.ptr_on_device(),
                                                  lda,
                                                  dx.ptr_on_device(),
                                                  incx,
                                                  batch_count),
                          rocblas_status_invalid_value);
    // arg_checks code shared so transA, diag tested only in non-batched

    EXPECT_ROCBLAS_STATUS(
        rocblas_trmv_batched_fn(
            handle, uplo, transA, diag, N, nullptr, lda, dx.ptr_on_device(), incx, batch_count),
        rocblas_status_invalid_pointer);

    EXPECT_ROCBLAS_STATUS(
        rocblas_trmv_batched_fn(
            handle, uplo, transA, diag, N, dA.ptr_on_device(), lda, nullptr, incx, batch_count),
        rocblas_status_invalid_pointer);

    EXPECT_ROCBLAS_STATUS(rocblas_trmv_batched_fn(nullptr,
                                                  uplo,
                                                  transA,
                                                  diag,
                                                  N,
                                                  dA.ptr_on_device(),
                                                  lda,
                                                  dx.ptr_on_device(),
                                                  incx,
                                                  batch_count),
                          rocblas_status_invalid_handle);
}

template <typename T>
void testing_trmv_batched(const Arguments& arg)
{
    auto rocblas_trmv_batched_fn
        = arg.api == FORTRAN ? rocblas_trmv_batched<T, true> : rocblas_trmv_batched<T, false>;

    rocblas_int N = arg.N, lda = arg.lda, incx = arg.incx, batch_count = arg.batch_count;

    char char_uplo = arg.uplo, char_transA = arg.transA, char_diag = arg.diag;

    rocblas_fill      uplo   = char2rocblas_fill(char_uplo);
    rocblas_operation transA = char2rocblas_operation(char_transA);
    rocblas_diagonal  diag   = char2rocblas_diagonal(char_diag);

    rocblas_local_handle handle{arg};

    bool invalid_size = N < 0 || lda < N || lda < 1 || !incx || batch_count < 0;
    if(invalid_size || !N || !batch_count)
    {
        EXPECT_ROCBLAS_STATUS(
            rocblas_trmv_batched_fn(
                handle, uplo, transA, diag, N, nullptr, lda, nullptr, incx, batch_count),
            invalid_size ? rocblas_status_invalid_size : rocblas_status_success);

        return;
    }

    // Naming: `h` is in CPU (host) memory(eg hA), `d` is in GPU (device) memory (eg dA).
    // Allocate host memory
    host_batch_matrix<T> hA(N, N, lda, batch_count);
    host_batch_vector<T> hx(N, incx, batch_count);
    host_batch_vector<T> hres(N, incx, batch_count);

    // Check host memory allocation
    CHECK_HIP_ERROR(hA.memcheck());
    CHECK_HIP_ERROR(hx.memcheck());
    CHECK_HIP_ERROR(hres.memcheck());

    // Allocate device memory
    device_batch_matrix<T> dA(N, N, lda, batch_count);
    device_batch_vector<T> dx(N, incx, batch_count);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dA.memcheck());
    CHECK_DEVICE_ALLOCATION(dx.memcheck());

    // Initialize data on host memory
    rocblas_init_matrix(
        hA, arg, rocblas_client_never_set_nan, rocblas_client_triangular_matrix, true);
    rocblas_init_vector(hx, arg, rocblas_client_never_set_nan, false, true);

    // Copy data from CPU to device
    CHECK_HIP_ERROR(dA.transfer_from(hA));
    CHECK_HIP_ERROR(dx.transfer_from(hx));

    double gpu_time_used, cpu_time_used, rocblas_error;
    auto   dA_on_device = dA.ptr_on_device();
    auto   dx_on_device = dx.ptr_on_device();

    /* =====================================================================
     ROCBLAS
     =================================================================== */
    if(arg.unit_check || arg.norm_check)
    {
        handle.pre_test(arg);
        // GPU BLAS
        CHECK_ROCBLAS_ERROR(rocblas_trmv_batched_fn(
            handle, uplo, transA, diag, N, dA_on_device, lda, dx_on_device, incx, batch_count));
        handle.post_test(arg);

        // CPU BLAS
        {
            cpu_time_used = get_time_us_no_sync();
            for(rocblas_int batch_index = 0; batch_index < batch_count; ++batch_index)
            {
                ref_trmv<T>(uplo, transA, diag, N, hA[batch_index], lda, hx[batch_index], incx);
            }
            cpu_time_used = get_time_us_no_sync() - cpu_time_used;
        }

        // fetch GPU
        CHECK_HIP_ERROR(hres.transfer_from(dx));

        // Unit check.
        if(arg.unit_check)
        {
            unit_check_general<T>(1, N, incx, hx, hres, batch_count);
        }

        // Norm check.
        if(arg.norm_check)
        {
            rocblas_error = norm_check_general<T>('F', 1, N, incx, hx, hres, batch_count);
        }
    }

    if(arg.timing)
    {

        // Warmup
        {
            int number_cold_calls = arg.cold_iters;
            for(int iter = 0; iter < number_cold_calls; iter++)
            {
                rocblas_trmv_batched_fn(handle,
                                        uplo,
                                        transA,
                                        diag,
                                        N,
                                        dA_on_device,
                                        lda,
                                        dx_on_device,
                                        incx,
                                        batch_count);
            }
        }

        // Go !
        {
            hipStream_t stream;
            CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));
            gpu_time_used        = get_time_us_sync(stream); // in microseconds
            int number_hot_calls = arg.iters;
            for(int iter = 0; iter < number_hot_calls; iter++)
            {
                rocblas_trmv_batched_fn(handle,
                                        uplo,
                                        transA,
                                        diag,
                                        N,
                                        dA_on_device,
                                        lda,
                                        dx_on_device,
                                        incx,
                                        batch_count);
            }
            gpu_time_used = get_time_us_sync(stream) - gpu_time_used;
        }

        // Log performance
        ArgumentModel<e_uplo, e_transA, e_diag, e_N, e_lda, e_incx, e_batch_count>{}.log_args<T>(
            rocblas_cout,
            arg,
            gpu_time_used,
            trmv_gflop_count<T>(N),
            trmv_gbyte_count<T>(N),
            cpu_time_used,
            rocblas_error);
    }
}
