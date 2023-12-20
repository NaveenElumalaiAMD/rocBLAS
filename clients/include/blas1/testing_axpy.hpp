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

#include "testing_common.hpp"

#include "blas1/rocblas_axpy.hpp"
#include "src64/blas1/rocblas_axpy_64.hpp"

/* ============================================================================================ */
template <typename T>
void testing_axpy_bad_arg(const Arguments& arg)
{
    auto rocblas_axpy_fn = arg.api == FORTRAN ? rocblas_axpy<T, true> : rocblas_axpy<T, false>;
    auto rocblas_axpy_fn_64
        = arg.api == FORTRAN_64 ? rocblas_axpy_64<T, true> : rocblas_axpy_64<T, false>;

    for(auto pointer_mode : {rocblas_pointer_mode_host, rocblas_pointer_mode_device})
    {
        rocblas_local_handle handle{arg};
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, pointer_mode));

        int64_t N    = 100;
        int64_t incx = 1;
        int64_t incy = 1;

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
        device_vector<T> dx(N, incx);
        device_vector<T> dy(N, incy);

        // Check device memory allocation
        CHECK_DEVICE_ALLOCATION(dx.memcheck());
        CHECK_DEVICE_ALLOCATION(dy.memcheck());

        DAPI_EXPECT(rocblas_status_invalid_handle,
                    rocblas_axpy_fn,
                    (nullptr, N, alpha, dx, incx, dy, incy));

        DAPI_EXPECT(rocblas_status_invalid_pointer,
                    rocblas_axpy_fn,
                    (handle, N, nullptr, dx, incx, dy, incy));

        if(pointer_mode == rocblas_pointer_mode_host)
        {
            DAPI_EXPECT(rocblas_status_invalid_pointer,
                        rocblas_axpy_fn,
                        (handle, N, alpha, nullptr, incx, dy, incy));

            DAPI_EXPECT(rocblas_status_invalid_pointer,
                        rocblas_axpy_fn,
                        (handle, N, alpha, dx, incx, nullptr, incy));
        }

        // If N == 0, then alpha, X and Y can be nullptr without error
        DAPI_EXPECT(rocblas_status_success,
                    rocblas_axpy_fn,
                    (handle, 0, nullptr, nullptr, incx, nullptr, incy));
        // If alpha == 0, then X and Y can be nullptr without error
        DAPI_EXPECT(rocblas_status_success,
                    rocblas_axpy_fn,
                    (handle, N, zero, nullptr, incx, nullptr, incy));
    }
}

template <typename T>
void testing_axpy(const Arguments& arg)
{
    auto rocblas_axpy_fn = arg.api == FORTRAN ? rocblas_axpy<T, true> : rocblas_axpy<T, false>;
    auto rocblas_axpy_fn_64
        = arg.api == FORTRAN_64 ? rocblas_axpy_64<T, true> : rocblas_axpy_64<T, false>;

    int64_t              N       = arg.N;
    int64_t              incx    = arg.incx;
    int64_t              incy    = arg.incy;
    T                    h_alpha = arg.get_alpha<T>();
    bool                 HMM     = arg.HMM;
    rocblas_local_handle handle{arg};

    // argument sanity check before allocating invalid memory
    if(N <= 0)
    {
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
        DAPI_CHECK(rocblas_axpy_fn, (handle, N, nullptr, nullptr, incx, nullptr, incy));
        return;
    }

    // Naming: `h` is in CPU (host) memory(eg hx), `d` is in GPU (device) memory (eg dx).
    // Allocate host memory
    host_vector<T> hx(N, incx);
    host_vector<T> hy(N, incy);
    host_vector<T> hy_gold(N, incy);

    // Allocate device memory
    device_vector<T> dx(N, incx, HMM);
    device_vector<T> dy(N, incy, HMM);
    device_vector<T> d_alpha(1, 1, HMM);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(dy.memcheck());
    CHECK_DEVICE_ALLOCATION(d_alpha.memcheck());

    // Initialize data on host memory
    rocblas_init_vector(hx, arg, rocblas_client_alpha_sets_nan, true);
    rocblas_init_vector(hy, arg, rocblas_client_alpha_sets_nan, false, true);

    // copy vector is easy in STL; hy_gold = hx: save a copy in hy_gold which will be output of CPU
    // BLAS
    hy_gold = hy;

    // copy data from CPU to device
    CHECK_HIP_ERROR(dx.transfer_from(hx));
    CHECK_HIP_ERROR(dy.transfer_from(hy));

    double cpu_time_used;
    double rocblas_error_1 = 0.0;
    double rocblas_error_2 = 0.0;

    if(arg.unit_check || arg.norm_check)
    {
        if(arg.pointer_mode_host)
        {
            CHECK_HIP_ERROR(dy.transfer_from(hy));

            // ROCBLAS pointer mode host
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));

            handle.pre_test(arg);
            if(arg.api != INTERNAL)
            {
                DAPI_CHECK(rocblas_axpy_fn, (handle, N, &h_alpha, dx, incx, dy, incy));
            }
            else
            {
                // only checking offsets not alpha stride
                rocblas_stride offset_x = arg.lda;
                rocblas_stride offset_y = arg.ldb;
                DAPI_CHECK(rocblas_internal_axpy_template,
                           (handle,
                            N,
                            &h_alpha,
                            0,
                            dx + offset_x,
                            -offset_x,
                            incx,
                            arg.stride_x,
                            dy + offset_y,
                            -offset_y,
                            incy,
                            arg.stride_y,
                            1));
            }
            handle.post_test(arg);

            // copy output from device to CPU
            CHECK_HIP_ERROR(hy.transfer_from(dy));
        }

        if(arg.pointer_mode_device)
        {
            // ROCBLAS pointer mode device
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));

            CHECK_HIP_ERROR(dy.transfer_from(hy_gold)); // hy_gold not computed yet so still hy
            CHECK_HIP_ERROR(hipMemcpy(d_alpha, &h_alpha, sizeof(T), hipMemcpyHostToDevice));

            handle.pre_test(arg);
            DAPI_CHECK(rocblas_axpy_fn, (handle, N, d_alpha, dx, incx, dy, incy));
            handle.post_test(arg);
        }

        // check host mode results

        // CPU BLAS
        cpu_time_used = get_time_us_no_sync();

        ref_axpy<T>(N, h_alpha, hx, incx, hy_gold, incy);

        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        if(arg.pointer_mode_host)
        {
            if(arg.unit_check)
            {
                unit_check_general<T>(1, N, incy, hy_gold, hy);
            }

            if(arg.norm_check)
            {
                rocblas_error_1 = norm_check_general<T>('F', 1, N, incy, hy_gold, hy);
            }
        }

        if(arg.pointer_mode_device)
        {
            // check device mode results
            CHECK_HIP_ERROR(hy.transfer_from(dy));

            if(arg.unit_check)
            {
                unit_check_general<T>(1, N, incy, hy_gold, hy);
            }

            if(arg.norm_check)
            {
                rocblas_error_2 = norm_check_general<T>('F', 1, N, incy, hy_gold, hy);
            }
        }
    }

    if(arg.timing)
    {
        double gpu_time_used;
        int    number_cold_calls = arg.cold_iters;
        int    total_calls       = number_cold_calls + arg.iters;

        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));

        for(int iter = 0; iter < total_calls; iter++)
        {
            if(iter == number_cold_calls)
                gpu_time_used = get_time_us_sync(stream);

            DAPI_DISPATCH(rocblas_axpy_fn, (handle, N, &h_alpha, dx, incx, dy, incy));
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        ArgumentModel<e_N, e_alpha, e_incx, e_incy>{}.log_args<T>(rocblas_cout,
                                                                  arg,
                                                                  gpu_time_used,
                                                                  axpy_gflop_count<T>(N),
                                                                  axpy_gbyte_count<T>(N),
                                                                  cpu_time_used,
                                                                  rocblas_error_1,
                                                                  rocblas_error_2);
    }
}
