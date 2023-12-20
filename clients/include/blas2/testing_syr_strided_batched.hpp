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
 * ************************************************************************ */

#pragma once

#include "bytes.hpp"
#include "cblas_interface.hpp"
#include "flops.hpp"
#include "near.hpp"
#include "norm.hpp"
#include "rocblas.hpp"
#include "rocblas_init.hpp"
#include "rocblas_math.hpp"
#include "rocblas_matrix.hpp"
#include "rocblas_random.hpp"
#include "rocblas_test.hpp"
#include "rocblas_vector.hpp"
#include "unit.hpp"
#include "utility.hpp"

template <typename T>
void testing_syr_strided_batched_bad_arg(const Arguments& arg)
{
    auto rocblas_syr_strided_batched_fn = arg.api == FORTRAN
                                              ? rocblas_syr_strided_batched<T, true>
                                              : rocblas_syr_strided_batched<T, false>;

    for(auto pointer_mode : {rocblas_pointer_mode_host, rocblas_pointer_mode_device})
    {
        rocblas_local_handle handle{arg};
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, pointer_mode));

        rocblas_fill   uplo        = rocblas_fill_upper;
        rocblas_int    N           = 100;
        rocblas_int    incx        = 1;
        rocblas_int    lda         = 100;
        rocblas_int    batch_count = 2;
        rocblas_stride stride_x    = 1;
        rocblas_stride stride_A    = 1;

        device_vector<T> alpha_d(1), zero_d(1);

        const T alpha_h(1), zero_h(0);

        const T* alpha = &alpha_h;
        const T* zero  = &zero_h;

        if(pointer_mode == rocblas_pointer_mode_device)
        {
            CHECK_HIP_ERROR(hipMemcpy(alpha_d, alpha, sizeof(*alpha), hipMemcpyHostToDevice));
            alpha = alpha_d;
            CHECK_HIP_ERROR(hipMemcpy(zero_d, zero, sizeof(*zero), hipMemcpyHostToDevice));
            zero = zero_d;
        }

        // Allocate device memory
        device_strided_batch_matrix<T> dA(N, N, lda, stride_A, batch_count);
        device_strided_batch_vector<T> dx(N, incx, stride_x, batch_count);

        // Check device memory allocation
        CHECK_DEVICE_ALLOCATION(dA.memcheck());
        CHECK_DEVICE_ALLOCATION(dx.memcheck());

        EXPECT_ROCBLAS_STATUS(
            rocblas_syr_strided_batched_fn(
                nullptr, uplo, N, alpha, dx, incx, stride_x, dA, lda, stride_A, batch_count),
            rocblas_status_invalid_handle);

        EXPECT_ROCBLAS_STATUS(rocblas_syr_strided_batched_fn(handle,
                                                             rocblas_fill_full,
                                                             N,
                                                             alpha,
                                                             dx,
                                                             incx,
                                                             stride_x,
                                                             dA,
                                                             lda,
                                                             stride_A,
                                                             batch_count),
                              rocblas_status_invalid_value);

        EXPECT_ROCBLAS_STATUS(
            rocblas_syr_strided_batched_fn(
                handle, uplo, N, nullptr, dx, incx, stride_x, dA, lda, stride_A, batch_count),
            rocblas_status_invalid_pointer);

        if(pointer_mode == rocblas_pointer_mode_host)
        {
            EXPECT_ROCBLAS_STATUS(rocblas_syr_strided_batched_fn(handle,
                                                                 uplo,
                                                                 N,
                                                                 alpha,
                                                                 nullptr,
                                                                 incx,
                                                                 stride_x,
                                                                 dA,
                                                                 lda,
                                                                 stride_A,
                                                                 batch_count),
                                  rocblas_status_invalid_pointer);

            EXPECT_ROCBLAS_STATUS(rocblas_syr_strided_batched_fn(handle,
                                                                 uplo,
                                                                 N,
                                                                 alpha,
                                                                 dx,
                                                                 incx,
                                                                 stride_x,
                                                                 nullptr,
                                                                 lda,
                                                                 stride_A,
                                                                 batch_count),
                                  rocblas_status_invalid_pointer);
        }

        // N==0 all pointers may be null
        EXPECT_ROCBLAS_STATUS(rocblas_syr_strided_batched_fn(handle,
                                                             uplo,
                                                             0,
                                                             nullptr,
                                                             nullptr,
                                                             incx,
                                                             stride_x,
                                                             nullptr,
                                                             lda,
                                                             stride_A,
                                                             batch_count),
                              rocblas_status_success);

        // alpha==0 all pointers may be null
        EXPECT_ROCBLAS_STATUS(rocblas_syr_strided_batched_fn(handle,
                                                             uplo,
                                                             N,
                                                             zero,
                                                             nullptr,
                                                             incx,
                                                             stride_x,
                                                             nullptr,
                                                             lda,
                                                             stride_A,
                                                             batch_count),
                              rocblas_status_success);

        // batch_count==0 all pointers may be null
        EXPECT_ROCBLAS_STATUS(
            rocblas_syr_strided_batched_fn(
                handle, uplo, N, nullptr, nullptr, incx, stride_x, nullptr, lda, stride_A, 0),
            rocblas_status_success);
    }
}

template <typename T>
void testing_syr_strided_batched(const Arguments& arg)
{
    auto rocblas_syr_strided_batched_fn = arg.api == FORTRAN
                                              ? rocblas_syr_strided_batched<T, true>
                                              : rocblas_syr_strided_batched<T, false>;

    rocblas_int    N           = arg.N;
    rocblas_int    incx        = arg.incx;
    rocblas_int    lda         = arg.lda;
    T              h_alpha     = arg.get_alpha<T>();
    rocblas_fill   uplo        = char2rocblas_fill(arg.uplo);
    rocblas_stride stride_x    = arg.stride_x;
    rocblas_stride stride_A    = arg.stride_a;
    rocblas_int    batch_count = arg.batch_count;

    rocblas_local_handle handle{arg};

    // argument check before allocating invalid memory
    bool invalid_size = N < 0 || lda < N || lda < 1 || !incx || batch_count < 0;
    if(invalid_size || !N || !batch_count)
    {
        EXPECT_ROCBLAS_STATUS(rocblas_syr_strided_batched_fn(handle,
                                                             uplo,
                                                             N,
                                                             nullptr,
                                                             nullptr,
                                                             incx,
                                                             stride_x,
                                                             nullptr,
                                                             lda,
                                                             stride_A,
                                                             batch_count),
                              invalid_size ? rocblas_status_invalid_size : rocblas_status_success);
        return;
    }

    stride_A        = std::max(stride_A, rocblas_stride(size_t(lda) * N));
    size_t abs_incx = incx >= 0 ? incx : -incx;
    stride_x        = std::max(stride_x, rocblas_stride(size_t(N) * abs_incx));

    // Naming: `h` is in CPU (host) memory(eg hA), `d` is in GPU (device) memory (eg dA).
    // Allocate host memory
    host_strided_batch_matrix<T> hA(N, N, lda, stride_A, batch_count);
    host_strided_batch_matrix<T> hA_gold(N, N, lda, stride_A, batch_count);
    host_strided_batch_vector<T> hx(N, incx, stride_x, batch_count);
    host_vector<T>               halpha(1);
    halpha[0] = h_alpha;

    // Check host memory allocation
    CHECK_HIP_ERROR(hA.memcheck());
    CHECK_HIP_ERROR(hA_gold.memcheck());
    CHECK_HIP_ERROR(hx.memcheck());

    // Allocate device memory
    device_strided_batch_matrix<T> dA(N, N, lda, stride_A, batch_count);
    device_strided_batch_vector<T> dx(N, incx, stride_x, batch_count);
    device_vector<T>               d_alpha(1);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dA.memcheck());
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(d_alpha.memcheck());

    // Initialize data on host memory
    rocblas_init_matrix(
        hA, arg, rocblas_client_never_set_nan, rocblas_client_symmetric_matrix, true);
    rocblas_init_vector(hx, arg, rocblas_client_alpha_sets_nan, false, true);

    // copy matrix in hA_gold which will be output of CPU BLAS
    hA_gold.copy_from(hA);
    hA.copy_from(hA);

    // copy data from CPU to device
    CHECK_HIP_ERROR(dA.transfer_from(hA));
    CHECK_HIP_ERROR(dx.transfer_from(hx));

    double gpu_time_used, cpu_time_used;
    double rocblas_gflops, ref_gflops, rocblas_bandwidth;
    double rocblas_error_1;
    double rocblas_error_2;

    if(arg.unit_check || arg.norm_check)
    {
        if(arg.pointer_mode_host)
        {
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
            handle.pre_test(arg);
            CHECK_ROCBLAS_ERROR(rocblas_syr_strided_batched_fn(
                handle, uplo, N, &h_alpha, dx, incx, stride_x, dA, lda, stride_A, batch_count));
            handle.post_test(arg);

            // copy output from device to CPU
            CHECK_HIP_ERROR(hA.transfer_from(dA));
        }

        if(arg.pointer_mode_device)
        {
            // copy data from CPU to device
            CHECK_HIP_ERROR(dA.transfer_from(hA_gold));
            CHECK_HIP_ERROR(d_alpha.transfer_from(halpha));

            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
            handle.pre_test(arg);
            CHECK_ROCBLAS_ERROR(rocblas_syr_strided_batched_fn(
                handle, uplo, N, d_alpha, dx, incx, stride_x, dA, lda, stride_A, batch_count));
            handle.post_test(arg);
        }

        // CPU BLAS
        cpu_time_used = get_time_us_no_sync();
        for(int b = 0; b < batch_count; b++)
        {
            ref_syr<T>(uplo, N, h_alpha, hx[b], incx, hA_gold[b], lda);
        }
        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        if(arg.pointer_mode_host)
        {
            if(arg.unit_check)
            {
                if(std::is_same_v<T, float> || std::is_same_v<T, double>)
                {
                    unit_check_general<T>(N, N, lda, stride_A, hA_gold, hA, batch_count);
                }
                else
                {
                    const double tol = N * sum_error_tolerance<T>;
                    near_check_general<T>(N, N, lda, stride_A, hA_gold, hA, batch_count, tol);
                }
            }

            if(arg.norm_check)
            {
                rocblas_error_1
                    = norm_check_general<T>('F', N, N, lda, stride_A, hA_gold, hA, batch_count);
            }
        }

        if(arg.pointer_mode_device)
        {
            // copy output from device to CPU
            CHECK_HIP_ERROR(hA.transfer_from(dA));

            if(arg.unit_check)
            {
                if(std::is_same_v<T, float> || std::is_same_v<T, double>)
                {
                    unit_check_general<T>(N, N, lda, stride_A, hA_gold, hA, batch_count);
                }
                else
                {
                    const double tol = N * sum_error_tolerance<T>;
                    near_check_general<T>(N, N, lda, stride_A, hA_gold, hA, batch_count, tol);
                }
            }

            if(arg.norm_check)
            {
                rocblas_error_2
                    = norm_check_general<T>('F', N, N, lda, stride_A, hA_gold, hA, batch_count);
            }
        }
    }

    if(arg.timing)
    {
        int number_cold_calls = arg.cold_iters;
        int number_hot_calls  = arg.iters;
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));

        for(int iter = 0; iter < number_cold_calls; iter++)
        {
            rocblas_syr_strided_batched_fn(
                handle, uplo, N, &h_alpha, dx, incx, stride_x, dA, lda, stride_A, batch_count);
        }

        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));
        gpu_time_used = get_time_us_sync(stream); // in microseconds

        for(int iter = 0; iter < number_hot_calls; iter++)
        {
            rocblas_syr_strided_batched_fn(
                handle, uplo, N, &h_alpha, dx, incx, stride_x, dA, lda, stride_A, batch_count);
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        Arguments targ(arg);
        targ.stride_a = stride_A;
        targ.stride_x = stride_x;
        ArgumentModel<e_uplo, e_N, e_alpha, e_lda, e_stride_a, e_incx, e_stride_x, e_batch_count>{}
            .log_args<T>(rocblas_cout,
                         targ,
                         gpu_time_used,
                         syr_gflop_count<T>(N),
                         syr_gbyte_count<T>(N),
                         cpu_time_used,
                         rocblas_error_1,
                         rocblas_error_2);
    }
}
