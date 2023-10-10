#pragma once

#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <ATen/cuda/CUDAGeneratorImpl.h>
#include <torch/torch.h>

#include <ck/ck.hpp>
#include <ck/utility/data_type.hpp>
#include <ck/utility/sequence.hpp>

#define NEW_UNPACK (TORCH_VERSION_MAJOR * 10000 + TORCH_VERSION_MINOR * 100 + TORCH_VERSION_PATCH) > 11300

#define XFORMERS_CHECK(COND, ERR)          \
  if (!(COND)) {                           \
    std::ostringstream ostr;               \
    ostr << "'" #COND "' failed: " << ERR; \
    throw std::runtime_error(ostr.str());  \
  }

#define DISPATCH_TYPES(InDataType, func)                                 \
  {                                                                      \
    if (InDataType == at::ScalarType::Half) {                            \
      using scalar_t = ck::half_t;                                       \
      func();                                                            \
    } else if (InDataType == at::ScalarType::BFloat16) {                 \
      using scalar_t = ck::bhalf_t;                                      \
      func();                                                            \
    } else {                                                             \
      XFORMERS_CHECK(                                                    \
          false, "Only half & bf16 input type supported at the moment"); \
    }                                                                    \
  }

template <typename scalar_t>
struct CkToAtenDtype;

template <>
struct CkToAtenDtype<ck::half_t> {
  using scalar_t = ck::half_t;

  static constexpr __host__ at::ScalarType atScalarType() {
    return at::ScalarType::Half;
  }
};

template <>
struct CkToAtenDtype<ck::bhalf_t> {
  using scalar_t = ck::bhalf_t;

  static constexpr __host__ at::ScalarType atScalarType() {
    return at::ScalarType::BFloat16;
  }
};

template <>
struct CkToAtenDtype<float> {
  using scalar_t = float;

  static constexpr __host__ at::ScalarType atScalarType() {
    return at::ScalarType::Float;
  }
};

#define CHECK_NOSPARSE_CONTIGUOUS_CUDA(TENSOR)                            \
  XFORMERS_CHECK(TENSOR.is_cuda(), #TENSOR " must be a CUDA tensor");     \
  XFORMERS_CHECK(!TENSOR.is_sparse(), #TENSOR " must be a dense tensor"); \
  XFORMERS_CHECK(TENSOR.is_contiguous(), #TENSOR " must be contiguous");

#define CHECK_NOSPARSE_LASTCONTIGUOUS_CUDA(TENSOR)                        \
  XFORMERS_CHECK(TENSOR.is_cuda(), #TENSOR " must be a CUDA tensor");     \
  XFORMERS_CHECK(!TENSOR.is_sparse(), #TENSOR " must be a dense tensor"); \
  XFORMERS_CHECK(                                                         \
      TENSOR.stride(-1) == 1, #TENSOR ": last dimension must be contiguous");

static inline size_t get_size_in_bytes(size_t n, at::ScalarType dtype) {
  if (dtype == at::ScalarType::Float) {
    return n * 4;
  } else if (dtype == at::ScalarType::Half) {
    return n * 2;
  } else if (dtype == at::ScalarType::BFloat16) {
    return n * 2;
  } else if (dtype == at::ScalarType::Short) {
    return n * 2;
  } else if (dtype == at::ScalarType::Int) {
    return n * 4;
  } else if (dtype == at::ScalarType::Byte) {
    return n;
  }
  return 0;
}


static std::tuple<uint64_t, uint64_t> unpack(at::PhiloxCudaState arg) {
  if (arg.captured_) {
#if NEW_UNPACK
    return std::make_tuple(static_cast<uint64_t>(*arg.seed_.ptr), static_cast<uint64_t>(*(arg.offset_.ptr) + arg.offset_intragraph_));
#else
    return std::make_tuple(arg.seed_, static_cast<uint64_t>(*(arg.offset_.ptr) + arg.offset_intragraph_));
#endif
  } else {
#if NEW_UNPACK
    return std::make_tuple(arg.seed_.val, arg.offset_.val);
#else
    return std::make_tuple(arg.seed_, arg.offset_.val);
#endif
  }
}


/**
 * kernels expect 4D bias/bias.grad with shape
 * (batch_sz, n_heads, n_queries, n_keys). common bias shapes users may pass
 * are:
 * - (n_queries, n_keys)
 * - (batch_sz * n_heads, n_queries, n_keys)
 * - (batch_sz, n_heads, n_queries, n_keys)
 *
 * expand the bias as needed - be careful to only create a view with different
 * shape/strides, no copies allowed.
 */
inline at::Tensor get_bias_4d_view(
    const at::Tensor& bias,
    int batch_sz,
    int n_heads,
    int n_queries,
    int n_keys) {
  TORCH_CHECK(
      bias.size(-2) == n_queries,
      "bias.size(-2) != n_queries: ",
      bias.size(-2),
      " != ",
      n_queries);
  TORCH_CHECK(
      bias.size(-1) == n_keys,
      "bias.size(-1) != n_keys: ",
      bias.size(-1),
      " != ",
      n_keys);
  switch (bias.dim()) {
    case 2: // (n_queries, n_keys) - broadcast across all batches and heads
      return bias.unsqueeze(0).unsqueeze(0).expand(
          {batch_sz, n_heads, n_queries, n_keys});
    case 3: // (batch_sz * n_heads, n_queries, n_keys) - just reshape
      TORCH_CHECK(bias.size(0) == batch_sz * n_heads);
      return bias.view({batch_sz, n_heads, n_queries, n_keys});
    case 4: // (batch_sz, n_heads, n_queries, n_keys) - do nothing
      TORCH_CHECK(bias.size(0) == batch_sz);
      TORCH_CHECK(bias.size(1) == n_heads)
      return bias;
    default:
      TORCH_CHECK(false, "bias can only have ndims in {2, 3, 4}");
  }
}
