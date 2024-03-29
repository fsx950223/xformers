/*
 * Copyright (c) 2023, Advanced Micro Devices, Inc. All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <ck/ck.hpp>
#include <ck/utility/data_type.hpp>
#include <stdexcept>

#include "ck_bool_switch.h"
#include "ck_tiled_fmha_grouped_infer.h"

extern template void
run_grouped_infer_masktype_attnbias_dispatched<ck::half_t, 0, true>(GroupedForwardParams& param,
                                                                    hipStream_t stream);

extern template void
run_grouped_infer_masktype_attnbias_dispatched<ck::half_t, 0, false>(GroupedForwardParams& param,
                                                                     hipStream_t stream);

extern template void
run_grouped_infer_masktype_attnbias_dispatched<ck::half_t, 1, true>(GroupedForwardParams& param,
                                                                    hipStream_t stream);

extern template void
run_grouped_infer_masktype_attnbias_dispatched<ck::half_t, 1, false>(GroupedForwardParams& param,
                                                                     hipStream_t stream);

extern template void
run_grouped_infer_masktype_attnbias_dispatched<ck::half_t, 2, true>(GroupedForwardParams& param,
                                                                    hipStream_t stream);

extern template void
run_grouped_infer_masktype_attnbias_dispatched<ck::half_t, 2, false>(GroupedForwardParams& param,
                                                                     hipStream_t stream);

void grouped_infer_fp16(GroupedForwardParams& param, hipStream_t stream)
{
    BOOL_SWITCH_1(param.has_attn_bias, HAS_ATTN_BIAS, [&] {
        if(param.custom_mask_type == 0)
            run_grouped_infer_masktype_attnbias_dispatched<ck::half_t, 0, HAS_ATTN_BIAS>(param,
                                                                                         stream);
        else if(param.custom_mask_type == 1)
            run_grouped_infer_masktype_attnbias_dispatched<ck::half_t, 1, HAS_ATTN_BIAS>(param,
                                                                                         stream);
        else if(param.custom_mask_type == 2)
            run_grouped_infer_masktype_attnbias_dispatched<ck::half_t, 2, HAS_ATTN_BIAS>(param,
                                                                                         stream);
        else
            throw std::runtime_error("Invalid custom_mask_type value");
    });
};
