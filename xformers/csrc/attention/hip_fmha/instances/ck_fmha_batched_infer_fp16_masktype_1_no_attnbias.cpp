#include <ck/ck.hpp>

#include "ck_fmha_batched_infer.h"

template void run_batched_infer_masktype_attnbias_dispatched<
    ck::half_t,
    1,
    false>(BatchedForwardParams& param, hipStream_t stream);