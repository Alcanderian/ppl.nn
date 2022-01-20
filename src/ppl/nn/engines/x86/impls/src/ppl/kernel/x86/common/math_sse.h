// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef __ST_PPL_KERNEL_X86_COMMON_MATH_SSE_H_
#define __ST_PPL_KERNEL_X86_COMMON_MATH_SSE_H_

#include <nmmintrin.h>

namespace ppl { namespace kernel { namespace x86 {

// an approximation of sigmoid
// onnxruntime/core/mlas/lib/logistic.cpp
static inline __m128 _sse_sigmoid_ps(const __m128 var)
{
    __m128 value = var;
    value = _mm_max_ps(_mm_set1_ps(-18.0f), value);
    value = _mm_min_ps(_mm_set1_ps(18.0f), value);

    __m128 value_squared = _mm_mul_ps(value, value);

    __m128 p;
    p = _mm_mul_ps(value_squared, _mm_set1_ps(4.37031012579801e-11f));
    p = _mm_add_ps(p, _mm_set1_ps(1.15627324459942e-07f));
    p = _mm_mul_ps(p, value_squared);
    p = _mm_add_ps(p, _mm_set1_ps(6.08574864600143e-05f));
    p = _mm_mul_ps(p, value_squared);
    p = _mm_add_ps(p, _mm_set1_ps(8.51377133304701e-03f));
    p = _mm_mul_ps(p, value_squared);
    p = _mm_add_ps(p, _mm_set1_ps(2.48287947061529e-01f));
    p = _mm_mul_ps(p, value);

    __m128 q;
    q = _mm_mul_ps(value_squared, _mm_set1_ps(6.10247389755681e-13f));
    q = _mm_add_ps(q, _mm_set1_ps(5.76102136993427e-09f));
    q = _mm_mul_ps(q, value_squared);
    q = _mm_add_ps(q, _mm_set1_ps(6.29106785017040e-06f));
    q = _mm_mul_ps(q, value_squared);
    q = _mm_add_ps(q, _mm_set1_ps(1.70198817374094e-03f));
    q = _mm_mul_ps(q, value_squared);
    q = _mm_add_ps(q, _mm_set1_ps(1.16817656904453e-01f));
    q = _mm_mul_ps(q, value_squared);
    q = _mm_add_ps(q, _mm_set1_ps(9.93151921023180e-01f));

    __m128 dst = _mm_add_ps(_mm_div_ps(p, q), _mm_set1_ps(0.5f));
    return dst;
}

// an approximation of tanh
// onnxruntime/core/mlas/lib/tanh.cpp
static inline __m128 _sse_tanh_ps(const __m128 var)
{
    __m128 value = var;
    value = _mm_max_ps(_mm_set1_ps(-9.0f), value);
    value = _mm_min_ps(_mm_set1_ps(9.0f), value);

    __m128 value_squared = _mm_mul_ps(value, value);

    __m128 p;
    p = _mm_mul_ps(value_squared, _mm_set1_ps(-2.76076847742355e-16f));
    p = _mm_add_ps(p, _mm_set1_ps(2.00018790482477e-13f));
    p = _mm_mul_ps(p, value_squared);
    p = _mm_add_ps(p, _mm_set1_ps(-8.60467152213735e-11f));
    p = _mm_mul_ps(p, value_squared);
    p = _mm_add_ps(p, _mm_set1_ps(5.12229709037114e-08f));
    p = _mm_mul_ps(p, value_squared);
    p = _mm_add_ps(p, _mm_set1_ps(1.48572235717979e-05f));
    p = _mm_mul_ps(p, value_squared);
    p = _mm_add_ps(p, _mm_set1_ps(6.37261928875436e-04f));
    p = _mm_mul_ps(p, value_squared);
    p = _mm_add_ps(p, _mm_set1_ps(4.89352455891786e-03f));
    p = _mm_mul_ps(p, value);

    __m128 q;
    q = _mm_mul_ps(value_squared, _mm_set1_ps(1.19825839466702e-06f));
    q = _mm_add_ps(q, _mm_set1_ps(1.18534705686654e-04f));
    q = _mm_mul_ps(q, value_squared);
    q = _mm_add_ps(q, _mm_set1_ps(2.26843463243900e-03f));
    q = _mm_mul_ps(q, value_squared);
    q = _mm_add_ps(q, _mm_set1_ps(4.89352518554385e-03f));

    __m128 dst = _mm_div_ps(p, q);
    return dst;
}

// an approximation of exp
// https://github.com/reyoung/avx_mathfun/blob/master/avx_mathfun.h
static inline __m128 _sse_exp_ps(const __m128 __x)
{
    __m128 tmp = _mm_setzero_ps(), fx;
    __m128i imm0;
    __m128 one = _mm_set1_ps(1.0f);

    __m128 x = __x;
    x = _mm_min_ps(x, _mm_set1_ps(88.3762626647949f));
    x = _mm_max_ps(x, _mm_set1_ps(-88.3762626647949f));

    fx = _mm_mul_ps(x, _mm_set1_ps(1.44269504088896341));
    fx = _mm_add_ps(fx, _mm_set1_ps(0.5f));

    tmp = _mm_floor_ps(fx);

    __m128 mask = _mm_cmpgt_ps(tmp, fx);
    mask        = _mm_and_ps(mask, one);
    fx          = _mm_sub_ps(tmp, mask);

    tmp      = _mm_mul_ps(fx, _mm_set1_ps(0.693359375));
    __m128 z = _mm_mul_ps(fx, _mm_set1_ps(-2.12194440e-4));
    x        = _mm_sub_ps(x, tmp);
    x        = _mm_sub_ps(x, z);
    z        = _mm_mul_ps(x, x);

    __m128 y = _mm_set1_ps(1.9875691500E-4);
    y        = _mm_mul_ps(y, x);
    y        = _mm_add_ps(y, _mm_set1_ps(1.3981999507E-3));
    y        = _mm_mul_ps(y, x);
    y        = _mm_add_ps(y, _mm_set1_ps(8.3334519073E-3));
    y        = _mm_mul_ps(y, x);
    y        = _mm_add_ps(y, _mm_set1_ps(4.1665795894E-2));
    y        = _mm_mul_ps(y, x);
    y        = _mm_add_ps(y, _mm_set1_ps(1.6666665459E-1));
    y        = _mm_mul_ps(y, x);
    y        = _mm_add_ps(y, _mm_set1_ps(5.0000001201E-1));
    y        = _mm_mul_ps(y, z);
    y        = _mm_add_ps(y, x);
    y        = _mm_add_ps(y, one);

    imm0         = _mm_cvttps_epi32(fx);
    imm0         = _mm_add_epi32(imm0, _mm_set1_epi32(0x7f));
    imm0         = _mm_slli_epi32(imm0, 23);
    __m128 pow2n = _mm_castsi128_ps(imm0);
    y            = _mm_mul_ps(y, pow2n);
    return y;
}

// an approximation of exp
// onnxruntime/core/mlas/lib/erf.cpp, result aligned with std::erff
static inline __m128 _sse_erf_ps(const __m128 x) {
    __m128 neg_zero = _mm_set1_ps(-0.0f);
    __m128 sign_mask = _mm_and_ps(x, neg_zero);
    __m128 abs_value = _mm_andnot_ps(neg_zero, x);
    abs_value = _mm_min_ps(_mm_set1_ps(3.925f), abs_value);
    __m128 sq_value = _mm_mul_ps(abs_value, abs_value);

    __m128 r_small = _mm_set1_ps(-5.99104969e-4f);
    r_small = _mm_add_ps(_mm_mul_ps(r_small, sq_value), _mm_set1_ps(4.99339588e-3f));
    r_small = _mm_add_ps(_mm_mul_ps(r_small, sq_value), _mm_set1_ps(-2.67667342e-2f));
    r_small = _mm_add_ps(_mm_mul_ps(r_small, sq_value), _mm_set1_ps(1.12818025e-1f));
    r_small = _mm_add_ps(_mm_mul_ps(r_small, sq_value), _mm_set1_ps(-3.76124859e-1f));
    r_small = _mm_add_ps(_mm_mul_ps(r_small, sq_value), _mm_set1_ps(1.28379151e-1f));
    r_small = _mm_add_ps(_mm_mul_ps(r_small, abs_value), abs_value);
    __m128 split_mask = _mm_cmpgt_ps(abs_value, _mm_set1_ps(0.921875f));
    r_small = _mm_andnot_ps(split_mask, r_small);

    abs_value = _mm_and_ps(split_mask, abs_value); // clear smaller value into zero for bigger number calculation
    __m128 r_big = _mm_set1_ps(1.72948930e-5f);
    r_big = _mm_add_ps(_mm_mul_ps(r_big, abs_value), _mm_set1_ps(-3.83208680e-4f));
    r_big = _mm_add_ps(_mm_mul_ps(r_big, abs_value), _mm_set1_ps(3.88393435e-3f));
    r_big = _mm_add_ps(_mm_mul_ps(r_big, abs_value), _mm_set1_ps(-2.42545605e-2f));
    r_big = _mm_add_ps(_mm_mul_ps(r_big, abs_value), _mm_set1_ps(1.06777847e-1f));
    r_big = _mm_add_ps(_mm_mul_ps(r_big, abs_value), _mm_set1_ps(6.34846687e-1f));
    r_big = _mm_add_ps(_mm_mul_ps(r_big, abs_value), _mm_set1_ps(1.28717512e-1f));
    r_big = _mm_add_ps(_mm_mul_ps(r_big, abs_value), abs_value);

    // 1.0 - exp(-r_big), no need to do min()
    r_big = _mm_xor_ps(r_big, neg_zero); // -r_big
    __m128 y = _sse_exp_ps(r_big);
    y = _mm_sub_ps(_mm_set1_ps(1.0f), y);

    // merge two splits results
    y = _mm_or_ps(r_small, y);
    y = _mm_or_ps(y, sign_mask);

    return y;
}

}}}; // namespace ppl::kernel::x86

#endif