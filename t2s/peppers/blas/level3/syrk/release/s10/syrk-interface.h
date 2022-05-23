#ifndef HALIDE__syrk___interface_h
#define HALIDE__syrk___interface_h
#include "AOT-OpenCL-Runtime.h"



#ifdef __cplusplus
extern "C" {
#endif

HALIDE_FUNCTION_ATTRS
int syrk(int32_t _OpA, float _Alpha, float _Beta, struct halide_buffer_t *_A_buffer, struct halide_buffer_t *_C_buffer, struct halide_buffer_t *_deserializer_buffer);

HALIDE_FUNCTION_ATTRS
int syrk_argv(void **args);

HALIDE_FUNCTION_ATTRS
const struct halide_filter_metadata_t *syrk_metadata();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
