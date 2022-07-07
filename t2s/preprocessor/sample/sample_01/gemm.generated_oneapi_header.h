#include <iostream>
#include <math.h>
#include <float.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

extern "C" {
int64_t halide_current_time_ns(void *ctx);
void halide_profiler_pipeline_end(void *, void *);
}

#ifdef _WIN32
__declspec(dllimport) float __cdecl roundf(float);
__declspec(dllimport) double __cdecl round(double);
#else
inline float asinh_f32(float x) {return asinhf(x);}
inline float acosh_f32(float x) {return acoshf(x);}
inline float atanh_f32(float x) {return atanhf(x);}
inline double asinh_f64(double x) {return asinh(x);}
inline double acosh_f64(double x) {return acosh(x);}
inline double atanh_f64(double x) {return atanh(x);}
#endif
inline float sqrt_f32(float x) {return sqrtf(x);}
inline float sin_f32(float x) {return sinf(x);}
inline float asin_f32(float x) {return asinf(x);}
inline float cos_f32(float x) {return cosf(x);}
inline float acos_f32(float x) {return acosf(x);}
inline float tan_f32(float x) {return tanf(x);}
inline float atan_f32(float x) {return atanf(x);}
inline float atan2_f32(float x, float y) {return atan2f(x, y);}
inline float sinh_f32(float x) {return sinhf(x);}
inline float cosh_f32(float x) {return coshf(x);}
inline float tanh_f32(float x) {return tanhf(x);}
inline float hypot_f32(float x, float y) {return hypotf(x, y);}
inline float exp_f32(float x) {return expf(x);}
inline float log_f32(float x) {return logf(x);}
inline float pow_f32(float x, float y) {return powf(x, y);}
inline float floor_f32(float x) {return floorf(x);}
inline float ceil_f32(float x) {return ceilf(x);}
inline float round_f32(float x) {return roundf(x);}

inline double sqrt_f64(double x) {return sqrt(x);}
inline double sin_f64(double x) {return sin(x);}
inline double asin_f64(double x) {return asin(x);}
inline double cos_f64(double x) {return cos(x);}
inline double acos_f64(double x) {return acos(x);}
inline double tan_f64(double x) {return tan(x);}
inline double atan_f64(double x) {return atan(x);}
inline double atan2_f64(double x, double y) {return atan2(x, y);}
inline double sinh_f64(double x) {return sinh(x);}
inline double cosh_f64(double x) {return cosh(x);}
inline double tanh_f64(double x) {return tanh(x);}
inline double hypot_f64(double x, double y) {return hypot(x, y);}
inline double exp_f64(double x) {return exp(x);}
inline double log_f64(double x) {return log(x);}
inline double pow_f64(double x, double y) {return pow(x, y);}
inline double floor_f64(double x) {return floor(x);}
inline double ceil_f64(double x) {return ceil(x);}
inline double round_f64(double x) {return round(x);}

inline float nan_f32() {return NAN;}
inline float neg_inf_f32() {return -INFINITY;}
inline float inf_f32() {return INFINITY;}
inline bool is_nan_f32(float x) {return isnan(x);}
inline bool is_nan_f64(double x) {return isnan(x);}
inline bool is_inf_f32(float x) {return isinf(x);}
inline bool is_inf_f64(double x) {return isinf(x);}
inline bool is_finite_f32(float x) {return isfinite(x);}
inline bool is_finite_f64(double x) {return isfinite(x);}

template<typename A, typename B>
inline A reinterpret(const B &b) {
    #if __cplusplus >= 201103L
    static_assert(sizeof(A) == sizeof(B), "type size mismatch");
    #endif
    A a;
    memcpy(&a, &b, sizeof(a));
    return a;
}
inline float float_from_bits(uint32_t bits) {
    return reinterpret<float, uint32_t>(bits);
}

template<typename T>
inline int halide_popcount(T a) {
    int bits_set = 0;
    while (a != 0) {
        bits_set += a & 1;
        a >>= 1;
    }
    return bits_set;
}

template<typename T>
inline int halide_count_leading_zeros(T a) {
    int leading_zeros = 0;
    int bit = sizeof(a) * 8 - 1;
    while (bit >= 0 && (a & (((T)1) << bit)) == 0) {
        leading_zeros++;
        bit--;
    }
    return leading_zeros;
}

template<typename T>
inline int halide_count_trailing_zeros(T a) {
    int trailing_zeros = 0;
    constexpr int bits = sizeof(a) * 8;
    int bit = 0;
    while (bit < bits && (a & (((T)1) << bit)) == 0) {
        trailing_zeros++;
        bit++;
    }
    return trailing_zeros;
}

template<typename T>
inline T halide_cpp_max(const T &a, const T &b) {return (a > b) ? a : b;}

template<typename T>
inline T halide_cpp_min(const T &a, const T &b) {return (a < b) ? a : b;}

template<typename A, typename B>
const B &return_second(const A &a, const B &b) {
    (void) a;
    return b;
}

template<typename A, typename B>
inline auto quiet_div(const A &a, const B &b) -> decltype(a / b) {
    return b == 0 ? static_cast<decltype(a / b)>(0) : (a / b);
}

template<typename A, typename B>
inline auto quiet_mod(const A &a, const B &b) -> decltype(a % b) {
    return b == 0 ? static_cast<decltype(a % b)>(0) : (a % b);
}

namespace {
class HalideFreeHelper {
    typedef void (*FreeFunction)(void *user_context, void *p);
    void * user_context;
    void *p;
    FreeFunction free_function;
public:
    HalideFreeHelper(void *user_context, void *p, FreeFunction free_function)
        : user_context(user_context), p(p), free_function(free_function) {}
    ~HalideFreeHelper() { free(); }
    void free() {
        if (p) {
            // TODO: do all free_functions guarantee to ignore a nullptr?
            free_function(user_context, p);
            p = nullptr;
        }
    }
};
} // namespace
#ifndef HALIDE_HALIDERUNTIME_H
#define HALIDE_HALIDERUNTIME_H

#ifndef COMPILING_HALIDE_RUNTIME
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#else
#include "runtime_internal.h"
#endif

#ifdef __cplusplus
// Forward declare type to allow naming typed handles.
// See Type.h for documentation.
template<typename T> struct halide_handle_traits;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Note that you should not use "inline" along with HALIDE_ALWAYS_INLINE;
// it is not necessary, and may produce warnings for some build configurations.
#ifdef _MSC_VER
#define HALIDE_ALWAYS_INLINE __forceinline
#define HALIDE_NEVER_INLINE __declspec(noinline)
#else
#define HALIDE_ALWAYS_INLINE __attribute__((always_inline)) inline
#define HALIDE_NEVER_INLINE __attribute__((noinline))
#endif

#ifndef HALIDE_MUST_USE_RESULT
#ifdef __has_attribute
#if __has_attribute(nodiscard)
// C++17 or later
#define HALIDE_MUST_USE_RESULT [[nodiscard]]
#elif __has_attribute(warn_unused_result)
// Clang/GCC
#define HALIDE_MUST_USE_RESULT __attribute__((warn_unused_result))
#else
#define HALIDE_MUST_USE_RESULT
#endif
#else
#define HALIDE_MUST_USE_RESULT
#endif
#endif

/** \file
 *
 * This file declares the routines used by Halide internally in its
 * runtime. On platforms that support weak linking, these can be
 * replaced with user-defined versions by defining an extern "C"
 * function with the same name and signature.
 *
 * When doing Just In Time (JIT) compilation methods on the Func being
 * compiled must be called instead. The corresponding methods are
 * documented below.
 *
 * All of these functions take a "void *user_context" parameter as their
 * first argument; if the Halide kernel that calls back to any of these
 * functions has been compiled with the UserContext feature set on its Target,
 * then the value of that pointer passed from the code that calls the
 * Halide kernel is piped through to the function.
 *
 * Some of these are also useful to call when using the default
 * implementation. E.g. halide_shutdown_thread_pool.
 *
 * Note that even on platforms with weak linking, some linker setups
 * may not respect the override you provide. E.g. if the override is
 * in a shared library and the halide object files are linked directly
 * into the output, the builtin versions of the runtime functions will
 * be called. See your linker documentation for more details. On
 * Linux, LD_DYNAMIC_WEAK=1 may help.
 *
 */

// Forward-declare to suppress warnings if compiling as C.
struct halide_buffer_t;
struct buffer_t;

/** Print a message to stderr. Main use is to support tracing
 * functionality, print, and print_when calls. Also called by the default
 * halide_error.  This function can be replaced in JITed code by using
 * halide_custom_print and providing an implementation of halide_print
 * in AOT code. See Func::set_custom_print.
 */
// @{
extern void halide_print(void *user_context, const char *);
extern void halide_default_print(void *user_context, const char *);
typedef void (*halide_print_t)(void *, const char *);
extern halide_print_t halide_set_custom_print(halide_print_t print);
// @}

/** Halide calls this function on runtime errors (for example bounds
 * checking failures). This function can be replaced in JITed code by
 * using Func::set_error_handler, or in AOT code by calling
 * halide_set_error_handler. In AOT code on platforms that support
 * weak linking (i.e. not Windows), you can also override it by simply
 * defining your own halide_error.
 */
// @{
extern void halide_error(void *user_context, const char *);
extern void halide_default_error(void *user_context, const char *);
typedef void (*halide_error_handler_t)(void *, const char *);
extern halide_error_handler_t halide_set_error_handler(halide_error_handler_t handler);
// @}

/** Cross-platform mutex. Must be initialized with zero and implementation
 * must treat zero as an unlocked mutex with no waiters, etc.
 */
struct halide_mutex {
    uintptr_t _private[1];
};

/** Cross platform condition variable. Must be initialized to 0. */
struct halide_cond {
    uintptr_t _private[1];
};

/** A basic set of mutex and condition variable functions, which call
 * platform specific code for mutual exclusion. Equivalent to posix
 * calls. */
//@{
extern void halide_mutex_lock(struct halide_mutex *mutex);
extern void halide_mutex_unlock(struct halide_mutex *mutex);
extern void halide_cond_signal(struct halide_cond *cond);
extern void halide_cond_broadcast(struct halide_cond *cond);
extern void halide_cond_wait(struct halide_cond *cond, struct halide_mutex *mutex);
//@}

/** Functions for constructing/destroying/locking/unlocking arrays of mutexes. */
struct halide_mutex_array;
//@{
extern struct halide_mutex_array* halide_mutex_array_create(int sz);
extern void halide_mutex_array_destroy(void *user_context, void *array);
extern int halide_mutex_array_lock(struct halide_mutex_array *array, int entry);
extern int halide_mutex_array_unlock(struct halide_mutex_array *array, int entry);
//@}

/** Define halide_do_par_for to replace the default thread pool
 * implementation. halide_shutdown_thread_pool can also be called to
 * release resources used by the default thread pool on platforms
 * where it makes sense. (E.g. On Mac OS, Grand Central Dispatch is
 * used so %Halide does not own the threads backing the pool and they
 * cannot be released.)  See Func::set_custom_do_task and
 * Func::set_custom_do_par_for. Should return zero if all the jobs
 * return zero, or an arbitrarily chosen return value from one of the
 * jobs otherwise.
 */
//@{
typedef int (*halide_task_t)(void *user_context, int task_number, uint8_t *closure);
extern int halide_do_par_for(void *user_context,
                             halide_task_t task,
                             int min, int size, uint8_t *closure);
extern void halide_shutdown_thread_pool();
//@}

/** Set a custom method for performing a parallel for loop. Returns
 * the old do_par_for handler. */
typedef int (*halide_do_par_for_t)(void *, halide_task_t, int, int, uint8_t*);
extern halide_do_par_for_t halide_set_custom_do_par_for(halide_do_par_for_t do_par_for);

/** An opaque struct representing a semaphore. Used by the task system for async tasks. */
struct halide_semaphore_t {
    uint64_t _private[2];
};

/** A struct representing a semaphore and a number of items that must
 * be acquired from it. Used in halide_parallel_task_t below. */
struct halide_semaphore_acquire_t {
    struct halide_semaphore_t *semaphore;
    int count;
};
extern int halide_semaphore_init(struct halide_semaphore_t *, int n);
extern int halide_semaphore_release(struct halide_semaphore_t *, int n);
extern bool halide_semaphore_try_acquire(struct halide_semaphore_t *, int n);
typedef int (*halide_semaphore_init_t)(struct halide_semaphore_t *, int);
typedef int (*halide_semaphore_release_t)(struct halide_semaphore_t *, int);
typedef bool (*halide_semaphore_try_acquire_t)(struct halide_semaphore_t *, int);


/** A task representing a serial for loop evaluated over some range.
 * Note that task_parent is a pass through argument that should be
 * passed to any dependent taks that are invokved using halide_do_parallel_tasks
 * underneath this call. */
typedef int (*halide_loop_task_t)(void *user_context, int min, int extent,
                                  uint8_t *closure, void *task_parent);

/** A parallel task to be passed to halide_do_parallel_tasks. This
 * task may recursively call halide_do_parallel_tasks, and there may
 * be complex dependencies between seemingly unrelated tasks expressed
 * using semaphores. If you are using a custom task system, care must
 * be taken to avoid potential deadlock. This can be done by carefully
 * respecting the static metadata at the end of the task struct.*/
struct halide_parallel_task_t {
    // The function to call. It takes a user context, a min and
    // extent, a closure, and a task system pass through argument.
    halide_loop_task_t fn;

    // The closure to pass it
    uint8_t *closure;

    // The name of the function to be called. For debugging purposes only.
    const char *name;

    // An array of semaphores that must be acquired before the
    // function is called. Must be reacquired for every call made.
    struct halide_semaphore_acquire_t *semaphores;
    int num_semaphores;

    // The entire range the function should be called over. This range
    // may be sliced up and the function called multiple times.
    int min, extent;

    // A parallel task provides several pieces of metadata to prevent
    // unbounded resource usage or deadlock.

    // The first is the minimum number of execution contexts (call
    // stacks or threads) necessary for the function to run to
    // completion. This may be greater than one when there is nested
    // parallelism with internal producer-consumer relationships
    // (calling the function recursively spawns and blocks on parallel
    // sub-tasks that communicate with each other via semaphores). If
    // a parallel runtime calls the function when fewer than this many
    // threads are idle, it may need to create more threads to
    // complete the task, or else risk deadlock due to committing all
    // threads to tasks that cannot complete without more.
    //
    // FIXME: Note that extern stages are assumed to only require a
    // single thread to complete. If the extern stage is itself a
    // Halide pipeline, this may be an underestimate.
    int min_threads;

    // The calls to the function should be in serial order from min to min+extent-1, with only
    // one executing at a time. If false, any order is fine, and
    // concurrency is fine.
    bool serial;
};

/** Enqueue some number of the tasks described above and wait for them
 * to complete. While waiting, the calling threads assists with either
 * the tasks enqueued, or other non-blocking tasks in the task
 * system. Note that task_parent should be NULL for top-level calls
 * and the pass through argument if this call is being made from
 * another task. */
extern int halide_do_parallel_tasks(void *user_context, int num_tasks,
                                    struct halide_parallel_task_t *tasks,
                                    void *task_parent);

/** If you use the default do_par_for, you can still set a custom
 * handler to perform each individual task. Returns the old handler. */
//@{
typedef int (*halide_do_task_t)(void *, halide_task_t, int, uint8_t *);
extern halide_do_task_t halide_set_custom_do_task(halide_do_task_t do_task);
extern int halide_do_task(void *user_context, halide_task_t f, int idx,
                          uint8_t *closure);
//@}

/** The version of do_task called for loop tasks. By default calls the
 * loop task with the same arguments. */
// @{
  typedef int (*halide_do_loop_task_t)(void *, halide_loop_task_t, int, int, uint8_t *, void *);
extern halide_do_loop_task_t halide_set_custom_do_loop_task(halide_do_loop_task_t do_task);
extern int halide_do_loop_task(void *user_context, halide_loop_task_t f, int min, int extent,
                               uint8_t *closure, void *task_parent);
//@}

/** Provide an entire custom tasking runtime via function
 * pointers. Note that do_task and semaphore_try_acquire are only ever
 * called by halide_default_do_par_for and
 * halide_default_do_parallel_tasks, so it's only necessary to provide
 * those if you are mixing in the default implementations of
 * do_par_for and do_parallel_tasks. */
// @{
typedef int (*halide_do_parallel_tasks_t)(void *, int, struct halide_parallel_task_t *,
                                          void *task_parent);
extern void halide_set_custom_parallel_runtime(
    halide_do_par_for_t,
    halide_do_task_t,
    halide_do_loop_task_t,
    halide_do_parallel_tasks_t,
    halide_semaphore_init_t,
    halide_semaphore_try_acquire_t,
    halide_semaphore_release_t
    );
// @}

/** The default versions of the parallel runtime functions. */
// @{
extern int halide_default_do_par_for(void *user_context,
                                     halide_task_t task,
                                     int min, int size, uint8_t *closure);
extern int halide_default_do_parallel_tasks(void *user_context,
                                            int num_tasks,
                                            struct halide_parallel_task_t *tasks,
                                            void *task_parent);
extern int halide_default_do_task(void *user_context, halide_task_t f, int idx,
                                  uint8_t *closure);
extern int halide_default_do_loop_task(void *user_context, halide_loop_task_t f,
                                       int min, int extent,
                                       uint8_t *closure, void *task_parent);
extern int halide_default_semaphore_init(struct halide_semaphore_t *, int n);
extern int halide_default_semaphore_release(struct halide_semaphore_t *, int n);
extern bool halide_default_semaphore_try_acquire(struct halide_semaphore_t *, int n);
// @}

struct halide_thread;

/** Spawn a thread. Returns a handle to the thread for the purposes of
 * joining it. The thread must be joined in order to clean up any
 * resources associated with it. */
extern struct halide_thread *halide_spawn_thread(void (*f)(void *), void *closure);

/** Join a thread. */
extern void halide_join_thread(struct halide_thread *);

/** Set the number of threads used by Halide's thread pool. Returns
 * the old number.
 *
 * n < 0  : error condition
 * n == 0 : use a reasonable system default (typically, number of cpus online).
 * n == 1 : use exactly one thread; this will always enforce serial execution
 * n > 1  : use a pool of exactly n threads.
 *
 * (Note that this is only guaranteed when using the default implementations
 * of halide_do_par_for(); custom implementations may completely ignore values
 * passed to halide_set_num_threads().)
 */
extern int halide_set_num_threads(int n);

/** Halide calls these functions to allocate and free memory. To
 * replace in AOT code, use the halide_set_custom_malloc and
 * halide_set_custom_free, or (on platforms that support weak
 * linking), simply define these functions yourself. In JIT-compiled
 * code use Func::set_custom_allocator.
 *
 * If you override them, and find yourself wanting to call the default
 * implementation from within your override, use
 * halide_default_malloc/free.
 *
 * Note that halide_malloc must return a pointer aligned to the
 * maximum meaningful alignment for the platform for the purpose of
 * vector loads and stores. The default implementation uses 32-byte
 * alignment, which is safe for arm and x86. Additionally, it must be
 * safe to read at least 8 bytes before the start and beyond the
 * end.
 */
//@{
extern void *halide_malloc(void *user_context, size_t x);
extern void halide_free(void *user_context, void *ptr);
extern void *halide_default_malloc(void *user_context, size_t x);
extern void halide_default_free(void *user_context, void *ptr);
typedef void *(*halide_malloc_t)(void *, size_t);
typedef void (*halide_free_t)(void *, void *);
extern halide_malloc_t halide_set_custom_malloc(halide_malloc_t user_malloc);
extern halide_free_t halide_set_custom_free(halide_free_t user_free);
//@}

/** Halide calls these functions to interact with the underlying
 * system runtime functions. To replace in AOT code on platforms that
 * support weak linking, define these functions yourself, or use
 * the halide_set_custom_load_library() and halide_set_custom_get_library_symbol()
 * functions. In JIT-compiled code, use JITSharedRuntime::set_default_handlers().
 *
 * halide_load_library and halide_get_library_symbol are equivalent to
 * dlopen and dlsym. halide_get_symbol(sym) is equivalent to
 * dlsym(RTLD_DEFAULT, sym).
 */
//@{
extern void *halide_get_symbol(const char *name);
extern void *halide_load_library(const char *name);
extern void *halide_get_library_symbol(void *lib, const char *name);
extern void *halide_default_get_symbol(const char *name);
extern void *halide_default_load_library(const char *name);
extern void *halide_default_get_library_symbol(void *lib, const char *name);
typedef void *(*halide_get_symbol_t)(const char *name);
typedef void *(*halide_load_library_t)(const char *name);
typedef void *(*halide_get_library_symbol_t)(void *lib, const char *name);
extern halide_get_symbol_t halide_set_custom_get_symbol(halide_get_symbol_t user_get_symbol);
extern halide_load_library_t halide_set_custom_load_library(halide_load_library_t user_load_library);
extern halide_get_library_symbol_t halide_set_custom_get_library_symbol(halide_get_library_symbol_t user_get_library_symbol);
//@}

/** Called when debug_to_file is used inside %Halide code.  See
 * Func::debug_to_file for how this is called
 *
 * Cannot be replaced in JITted code at present.
 */
extern int32_t halide_debug_to_file(void *user_context, const char *filename,
                                    int32_t type_code,
                                    struct halide_buffer_t *buf);

/** Types in the halide type system. They can be ints, unsigned ints,
 * or floats (of various bit-widths), or a handle (which is always 64-bits).
 * Note that the int/uint/float values do not imply a specific bit width
 * (the bit width is expected to be encoded in a separate value).
 */
typedef enum halide_type_code_t
#if __cplusplus >= 201103L
: uint8_t
#endif
{
    halide_type_int = 0,   //!< signed integers
    halide_type_uint = 1,  //!< unsigned integers
    halide_type_float = 2, //!< IEEE floating point numbers
    halide_type_handle = 3, //!< opaque pointer type (void *)
    halide_type_bfloat = 4, //!< floating point numbers in the bfloat format
} halide_type_code_t;

// Note that while __attribute__ can go before or after the declaration,
// __declspec apparently is only allowed before.
#ifndef HALIDE_ATTRIBUTE_ALIGN
    #ifdef _MSC_VER
        #define HALIDE_ATTRIBUTE_ALIGN(x) __declspec(align(x))
    #else
        #define HALIDE_ATTRIBUTE_ALIGN(x) __attribute__((aligned(x)))
    #endif
#endif

/** A runtime tag for a type in the halide type system. Can be ints,
 * unsigned ints, or floats of various bit-widths (the 'bits'
 * field). Can also be vectors of the same (by setting the 'lanes'
 * field to something larger than one). This struct should be
 * exactly 32-bits in size. */
struct halide_type_t {
    /** The basic type code: signed integer, unsigned integer, or floating point. */
#if __cplusplus >= 201103L
    HALIDE_ATTRIBUTE_ALIGN(1) halide_type_code_t code; // halide_type_code_t
#else
    HALIDE_ATTRIBUTE_ALIGN(1) uint8_t code; // halide_type_code_t
#endif

    /** The number of bits of precision of a single scalar value of this type. */
    HALIDE_ATTRIBUTE_ALIGN(1) uint8_t bits;

    /** How many elements in a vector. This is 1 for scalar types. */
    HALIDE_ATTRIBUTE_ALIGN(2) uint16_t lanes;

#ifdef __cplusplus
    /** Construct a runtime representation of a Halide type from:
     * code: The fundamental type from an enum.
     * bits: The bit size of one element.
     * lanes: The number of vector elements in the type. */
    HALIDE_ALWAYS_INLINE halide_type_t(halide_type_code_t code, uint8_t bits, uint16_t lanes = 1)
        : code(code), bits(bits), lanes(lanes) {
    }

    /** Default constructor is required e.g. to declare halide_trace_event
     * instances. */
    HALIDE_ALWAYS_INLINE halide_type_t() : code((halide_type_code_t)0), bits(0), lanes(0) {}

    HALIDE_ALWAYS_INLINE halide_type_t with_lanes(uint16_t new_lanes) const {
        return halide_type_t((halide_type_code_t) code, bits, new_lanes);
    }

    /** Compare two types for equality. */
    HALIDE_ALWAYS_INLINE bool operator==(const halide_type_t &other) const {
        return as_u32() == other.as_u32();
    }

    HALIDE_ALWAYS_INLINE bool operator!=(const halide_type_t &other) const {
        return !(*this == other);
    }

    HALIDE_ALWAYS_INLINE bool operator<(const halide_type_t &other) const {
        return as_u32() < other.as_u32();
    }

    /** Size in bytes for a single element, even if width is not 1, of this type. */
    HALIDE_ALWAYS_INLINE int bytes() const { return (bits + 7) / 8; }

    HALIDE_ALWAYS_INLINE uint32_t as_u32() const {
        uint32_t u;
        memcpy(&u, this, sizeof(u));
        return u;
    }
#endif
};

enum halide_trace_event_code_t {halide_trace_load = 0,
                                halide_trace_store = 1,
                                halide_trace_begin_realization = 2,
                                halide_trace_end_realization = 3,
                                halide_trace_produce = 4,
                                halide_trace_end_produce = 5,
                                halide_trace_consume = 6,
                                halide_trace_end_consume = 7,
                                halide_trace_begin_pipeline = 8,
                                halide_trace_end_pipeline = 9,
                                halide_trace_tag = 10 };

struct halide_trace_event_t {
    /** The name of the Func or Pipeline that this event refers to */
    const char *func;

    /** If the event type is a load or a store, this points to the
     * value being loaded or stored. Use the type field to safely cast
     * this to a concrete pointer type and retrieve it. For other
     * events this is null. */
    void *value;

    /** For loads and stores, an array which contains the location
     * being accessed. For vector loads or stores it is an array of
     * vectors of coordinates (the vector dimension is innermost).
     *
     * For realization or production-related events, this will contain
     * the mins and extents of the region being accessed, in the order
     * min0, extent0, min1, extent1, ...
     *
     * For pipeline-related events, this will be null.
     */
    int32_t *coordinates;

    /** For halide_trace_tag, this points to a read-only null-terminated string
     * of arbitrary text. For all other events, this will be null.
     */
    const char *trace_tag;

    /** If the event type is a load or a store, this is the type of
     * the data. Otherwise, the value is meaningless. */
    struct halide_type_t type;

    /** The type of event */
    enum halide_trace_event_code_t event;

    /* The ID of the parent event (see below for an explanation of
     * event ancestry). */
    int32_t parent_id;

    /** If this was a load or store of a Tuple-valued Func, this is
     * which tuple element was accessed. */
    int32_t value_index;

    /** The length of the coordinates array */
    int32_t dimensions;

#ifdef __cplusplus
    // If we don't explicitly mark the default ctor as inline,
    // certain build configurations can fail (notably iOS)
    HALIDE_ALWAYS_INLINE halide_trace_event_t() {}
#endif
};

/** Called when Funcs are marked as trace_load, trace_store, or
 * trace_realization. See Func::set_custom_trace. The default
 * implementation either prints events via halide_print, or if
 * HL_TRACE_FILE is defined, dumps the trace to that file in a
 * sequence of trace packets. The header for a trace packet is defined
 * below. If the trace is going to be large, you may want to make the
 * file a named pipe, and then read from that pipe into gzip.
 *
 * halide_trace returns a unique ID which will be passed to future
 * events that "belong" to the earlier event as the parent id. The
 * ownership hierarchy looks like:
 *
 * begin_pipeline
 * +--trace_tag (if any)
 * +--trace_tag (if any)
 * ...
 * +--begin_realization
 * |  +--produce
 * |  |  +--load/store
 * |  |  +--end_produce
 * |  +--consume
 * |  |  +--load
 * |  |  +--end_consume
 * |  +--end_realization
 * +--end_pipeline
 *
 * Threading means that ownership cannot be inferred from the ordering
 * of events. There can be many active realizations of a given
 * function, or many active productions for a single
 * realization. Within a single production, the ordering of events is
 * meaningful.
 *
 * Note that all trace_tag events (if any) will occur just after the begin_pipeline
 * event, but before any begin_realization events. All trace_tags for a given Func
 * will be emitted in the order added.
 */
// @}
extern int32_t halide_trace(void *user_context, const struct halide_trace_event_t *event);
extern int32_t halide_default_trace(void *user_context, const struct halide_trace_event_t *event);
typedef int32_t (*halide_trace_t)(void *user_context, const struct halide_trace_event_t *);
extern halide_trace_t halide_set_custom_trace(halide_trace_t trace);
// @}

/** The header of a packet in a binary trace. All fields are 32-bit. */
struct halide_trace_packet_t {
    /** The total size of this packet in bytes. Always a multiple of
     * four. Equivalently, the number of bytes until the next
     * packet. */
    uint32_t size;

    /** The id of this packet (for the purpose of parent_id). */
    int32_t id;

    /** The remaining fields are equivalent to those in halide_trace_event_t */
    // @{
    struct halide_type_t type;
    enum halide_trace_event_code_t event;
    int32_t parent_id;
    int32_t value_index;
    int32_t dimensions;
    // @}

    #ifdef __cplusplus
    // If we don't explicitly mark the default ctor as inline,
    // certain build configurations can fail (notably iOS)
    HALIDE_ALWAYS_INLINE halide_trace_packet_t() {}

    /** Get the coordinates array, assuming this packet is laid out in
     * memory as it was written. The coordinates array comes
     * immediately after the packet header. */
    HALIDE_ALWAYS_INLINE const int *coordinates() const {
        return (const int *)(this + 1);
    }

    HALIDE_ALWAYS_INLINE int *coordinates() {
        return (int *)(this + 1);
    }

    /** Get the value, assuming this packet is laid out in memory as
     * it was written. The packet comes immediately after the coordinates
     * array. */
    HALIDE_ALWAYS_INLINE const void *value() const {
        return (const void *)(coordinates() + dimensions);
    }

    HALIDE_ALWAYS_INLINE void *value() {
        return (void *)(coordinates() + dimensions);
    }

    /** Get the func name, assuming this packet is laid out in memory
     * as it was written. It comes after the value. */
    HALIDE_ALWAYS_INLINE const char *func() const {
        return (const char *)value() + type.lanes * type.bytes();
    }

    HALIDE_ALWAYS_INLINE char *func() {
        return (char *)value() + type.lanes * type.bytes();
    }

    /** Get the trace_tag (if any), assuming this packet is laid out in memory
     * as it was written. It comes after the func name. If there is no trace_tag,
     * this will return a pointer to an empty string. */
    HALIDE_ALWAYS_INLINE const char *trace_tag() const {
        const char *f = func();
        // strlen may not be available here
        while (*f++) {
            // nothing
        }
        return f;
    }

    HALIDE_ALWAYS_INLINE char *trace_tag() {
        char *f = func();
        // strlen may not be available here
        while (*f++) {
            // nothing
        }
        return f;
    }
    #endif
};



/** Set the file descriptor that Halide should write binary trace
 * events to. If called with 0 as the argument, Halide outputs trace
 * information to stdout in a human-readable format. If never called,
 * Halide checks the for existence of an environment variable called
 * HL_TRACE_FILE and opens that file. If HL_TRACE_FILE is not defined,
 * it outputs trace information to stdout in a human-readable
 * format. */
extern void halide_set_trace_file(int fd);

/** Halide calls this to retrieve the file descriptor to write binary
 * trace events to. The default implementation returns the value set
 * by halide_set_trace_file. Implement it yourself if you wish to use
 * a custom file descriptor per user_context. Return zero from your
 * implementation to tell Halide to print human-readable trace
 * information to stdout. */
extern int halide_get_trace_file(void *user_context);

/** If tracing is writing to a file. This call closes that file
 * (flushing the trace). Returns zero on success. */
extern int halide_shutdown_trace();

/** All Halide GPU or device backend implementations provide an
 * interface to be used with halide_device_malloc, etc. This is
 * accessed via the functions below.
 */

/** An opaque struct containing per-GPU API implementations of the
 * device functions. */
struct halide_device_interface_impl_t;

/** Each GPU API provides a halide_device_interface_t struct pointing
 * to the code that manages device allocations. You can access these
 * functions directly from the struct member function pointers, or by
 * calling the functions declared below. Note that the global
 * functions are not available when using Halide as a JIT compiler.
 * If you are using raw halide_buffer_t in that context you must use
 * the function pointers in the device_interface struct.
 *
 * The function pointers below are currently the same for every GPU
 * API; only the impl field varies. These top-level functions do the
 * bookkeeping that is common across all GPU APIs, and then dispatch
 * to more API-specific functions via another set of function pointers
 * hidden inside the impl field.
 */
struct halide_device_interface_t {
    int (*device_malloc)(void *user_context, struct halide_buffer_t *buf,
                         const struct halide_device_interface_t *device_interface);
    int (*device_free)(void *user_context, struct halide_buffer_t *buf);
    int (*device_sync)(void *user_context, struct halide_buffer_t *buf);
    void (*device_release)(void *user_context,
                          const struct halide_device_interface_t *device_interface);
    int (*copy_to_host)(void *user_context, struct halide_buffer_t *buf);
    int (*copy_to_device)(void *user_context, struct halide_buffer_t *buf,
                          const struct halide_device_interface_t *device_interface);
    int (*device_and_host_malloc)(void *user_context, struct halide_buffer_t *buf,
                                  const struct halide_device_interface_t *device_interface);
    int (*device_and_host_free)(void *user_context, struct halide_buffer_t *buf);
    int (*buffer_copy)(void *user_context, struct halide_buffer_t *src,
                       const struct halide_device_interface_t *dst_device_interface, struct halide_buffer_t *dst);
    int (*device_crop)(void *user_context, const struct halide_buffer_t *src,
                       struct halide_buffer_t *dst);
    int (*device_slice)(void *user_context, const struct halide_buffer_t *src,
                        int slice_dim, int slice_pos, struct halide_buffer_t *dst);
    int (*device_release_crop)(void *user_context, struct halide_buffer_t *buf);
    int (*wrap_native)(void *user_context, struct halide_buffer_t *buf, uint64_t handle,
                       const struct halide_device_interface_t *device_interface);
    int (*detach_native)(void *user_context, struct halide_buffer_t *buf);
    int (*compute_capability)(void *user_context, int *major, int *minor);
    const struct halide_device_interface_impl_t *impl;
};

/** Release all data associated with the given device interface, in
 * particular all resources (memory, texture, context handles)
 * allocated by Halide. Must be called explicitly when using AOT
 * compilation. This is *not* thread-safe with respect to actively
 * running Halide code. Ensure all pipelines are finished before
 * calling this. */
extern void halide_device_release(void *user_context,
                                  const struct halide_device_interface_t *device_interface);

/** Copy image data from device memory to host memory. This must be called
 * explicitly to copy back the results of a GPU-based filter. */
extern int halide_copy_to_host(void *user_context, struct halide_buffer_t *buf);

/** Copy image data from host memory to device memory. This should not
 * be called directly; Halide handles copying to the device
 * automatically.  If interface is NULL and the buf has a non-zero dev
 * field, the device associated with the dev handle will be
 * used. Otherwise if the dev field is 0 and interface is NULL, an
 * error is returned. */
extern int halide_copy_to_device(void *user_context, struct halide_buffer_t *buf,
                                 const struct halide_device_interface_t *device_interface);

/** Copy data from one buffer to another. The buffers may have
 * different shapes and sizes, but the destination buffer's shape must
 * be contained within the source buffer's shape. That is, for each
 * dimension, the min on the destination buffer must be greater than
 * or equal to the min on the source buffer, and min+extent on the
 * destination buffer must be less that or equal to min+extent on the
 * source buffer. The source data is pulled from either device or
 * host memory on the source, depending on the dirty flags. host is
 * preferred if both are valid. The dst_device_interface parameter
 * controls the destination memory space. NULL means host memory. */
extern int halide_buffer_copy(void *user_context, struct halide_buffer_t *src,
                              const struct halide_device_interface_t *dst_device_interface,
                              struct halide_buffer_t *dst);

/** Give the destination buffer a device allocation which is an alias
 * for the same coordinate range in the source buffer. Modifies the
 * device, device_interface, and the device_dirty flag only. Only
 * supported by some device APIs (others will return
 * halide_error_code_device_crop_unsupported). Call
 * halide_device_release_crop instead of halide_device_free to clean
 * up resources associated with the cropped view. Do not free the
 * device allocation on the source buffer while the destination buffer
 * still lives. Note that the two buffers do not share dirty flags, so
 * care must be taken to update them together as needed. Note that src
 * and dst are required to have the same number of dimensions.
 *
 * Note also that (in theory) device interfaces which support cropping may
 * still not support cropping a crop (instead, create a new crop of the parent
 * buffer); in practice, no known implementation has this limitation, although
 * it is possible that some future implementations may require it. */
extern int halide_device_crop(void *user_context,
                              const struct halide_buffer_t *src,
                              struct halide_buffer_t *dst);

/** Give the destination buffer a device allocation which is an alias
 * for a similar coordinate range in the source buffer, but with one dimension
 * sliced away in the dst. Modifies the device, device_interface, and the
 * device_dirty flag only. Only supported by some device APIs (others will return
 * halide_error_code_device_crop_unsupported). Call
 * halide_device_release_crop instead of halide_device_free to clean
 * up resources associated with the sliced view. Do not free the
 * device allocation on the source buffer while the destination buffer
 * still lives. Note that the two buffers do not share dirty flags, so
 * care must be taken to update them together as needed. Note that the dst buffer
 * must have exactly one fewer dimension than the src buffer, and that slice_dim
 * and slice_pos must be valid within src. */
extern int halide_device_slice(void *user_context,
                               const struct halide_buffer_t *src,
                               int slice_dim, int slice_pos,
                               struct halide_buffer_t *dst);

/** Release any resources associated with a cropped/sliced view of another
 * buffer. */
extern int halide_device_release_crop(void *user_context,
                                      struct halide_buffer_t *buf);

/** Wait for current GPU operations to complete. Calling this explicitly
 * should rarely be necessary, except maybe for profiling. */
extern int halide_device_sync(void *user_context, struct halide_buffer_t *buf);

/** Allocate device memory to back a halide_buffer_t. */
extern int halide_device_malloc(void *user_context, struct halide_buffer_t *buf,
                                const struct halide_device_interface_t *device_interface);

/** Free device memory. */
extern int halide_device_free(void *user_context, struct halide_buffer_t *buf);

/** Wrap or detach a native device handle, setting the device field
 * and device_interface field as appropriate for the given GPU
 * API. The meaning of the opaque handle is specific to the device
 * interface, so if you know the device interface in use, call the
 * more specific functions in the runtime headers for your specific
 * device API instead (e.g. HalideRuntimeCuda.h). */
// @{
extern int halide_device_wrap_native(void *user_context,
                                     struct halide_buffer_t *buf,
                                     uint64_t handle,
                                     const struct halide_device_interface_t *device_interface);
extern int halide_device_detach_native(void *user_context, struct halide_buffer_t *buf);
// @}

/** Versions of the above functions that accept legacy buffer_t structs. */
// @{
extern int halide_copy_to_host_legacy(void *user_context, struct buffer_t *buf);
extern int halide_copy_to_device_legacy(void *user_context, struct buffer_t *buf,
                                 const struct halide_device_interface_t *device_interface);
extern int halide_device_sync_legacy(void *user_context, struct buffer_t *buf);
extern int halide_device_malloc_legacy(void *user_context, struct buffer_t *buf,
                                const struct halide_device_interface_t *device_interface);
extern int halide_device_free_legacy(void *user_context, struct buffer_t *buf);
// @}

/** Selects which gpu device to use. 0 is usually the display
 * device. If never called, Halide uses the environment variable
 * HL_GPU_DEVICE. If that variable is unset, Halide uses the last
 * device. Set this to -1 to use the last device. */
extern void halide_set_gpu_device(int n);

/** Halide calls this to get the desired halide gpu device
 * setting. Implement this yourself to use a different gpu device per
 * user_context. The default implementation returns the value set by
 * halide_set_gpu_device, or the environment variable
 * HL_GPU_DEVICE. */
extern int halide_get_gpu_device(void *user_context);

/** Set the soft maximum amount of memory, in bytes, that the LRU
 *  cache will use to memoize Func results.  This is not a strict
 *  maximum in that concurrency and simultaneous use of memoized
 *  reults larger than the cache size can both cause it to
 *  temporariliy be larger than the size specified here.
 */
extern void halide_memoization_cache_set_size(int64_t size);

/** Given a cache key for a memoized result, currently constructed
 *  from the Func name and top-level Func name plus the arguments of
 *  the computation, determine if the result is in the cache and
 *  return it if so. (The internals of the cache key should be
 *  considered opaque by this function.) If this routine returns true,
 *  it is a cache miss. Otherwise, it will return false and the
 *  buffers passed in will be filled, via copying, with memoized
 *  data. The last argument is a list if halide_buffer_t pointers which
 *  represents the outputs of the memoized Func. If the Func does not
 *  return a Tuple, there will only be one halide_buffer_t in the list. The
 *  tuple_count parameters determines the length of the list.
 *
 * The return values are:
 * -1: Signals an error.
 *  0: Success and cache hit.
 *  1: Success and cache miss.
 */
extern int halide_memoization_cache_lookup(void *user_context, const uint8_t *cache_key, int32_t size,
                                           struct halide_buffer_t *realized_bounds,
                                           int32_t tuple_count, struct halide_buffer_t **tuple_buffers);

/** Given a cache key for a memoized result, currently constructed
 *  from the Func name and top-level Func name plus the arguments of
 *  the computation, store the result in the cache for futre access by
 *  halide_memoization_cache_lookup. (The internals of the cache key
 *  should be considered opaque by this function.) Data is copied out
 *  from the inputs and inputs are unmodified. The last argument is a
 *  list if halide_buffer_t pointers which represents the outputs of the
 *  memoized Func. If the Func does not return a Tuple, there will
 *  only be one halide_buffer_t in the list. The tuple_count parameters
 *  determines the length of the list.
 *
 * If there is a memory allocation failure, the store does not store
 * the data into the cache.
 */
extern int halide_memoization_cache_store(void *user_context, const uint8_t *cache_key, int32_t size,
                                          struct halide_buffer_t *realized_bounds,
                                          int32_t tuple_count,
                                          struct halide_buffer_t **tuple_buffers);

/** If halide_memoization_cache_lookup succeeds,
 * halide_memoization_cache_release must be called to signal the
 * storage is no longer being used by the caller. It will be passed
 * the host pointer of one the buffers returned by
 * halide_memoization_cache_lookup. That is
 * halide_memoization_cache_release will be called multiple times for
 * the case where halide_memoization_cache_lookup is handling multiple
 * buffers.  (This corresponds to memoizing a Tuple in Halide.) Note
 * that the host pointer must be sufficient to get to all information
 * the relase operation needs. The default Halide cache impleemntation
 * accomplishes this by storing extra data before the start of the user
 * modifiable host storage.
 *
 * This call is like free and does not have a failure return.
  */
extern void halide_memoization_cache_release(void *user_context, void *host);

/** Free all memory and resources associated with the memoization cache.
 * Must be called at a time when no other threads are accessing the cache.
 */
extern void halide_memoization_cache_cleanup();

/** Annotate that a given range of memory has been initialized;
 * only used when Target::MSAN is enabled.
 *
 * The default implementation uses the LLVM-provided AnnotateMemoryIsInitialized() function.
 */
extern int halide_msan_annotate_memory_is_initialized(void *user_context, const void *ptr, uint64_t len);

/** Mark the data pointed to by the buffer_t as initialized (but *not* the buffer_t itself),
 * using halide_msan_annotate_memory_is_initialized() for marking.
 *
 * The default implementation takes pains to only mark the active memory ranges
 * (skipping padding), and sorting into ranges to always mark the smallest number of
 * ranges, in monotonically increasing memory order.
 *
 * Most client code should never need to replace the default implementation.
 */
extern int halide_msan_annotate_buffer_is_initialized(void *user_context, struct halide_buffer_t *buffer);
extern void halide_msan_annotate_buffer_is_initialized_as_destructor(void *user_context, void *buffer);

/** The error codes that may be returned by a Halide pipeline. */
enum halide_error_code_t {
    /** There was no error. This is the value returned by Halide on success. */
    halide_error_code_success = 0,

    /** An uncategorized error occurred. Refer to the string passed to halide_error. */
    halide_error_code_generic_error = -1,

    /** A Func was given an explicit bound via Func::bound, but this
     * was not large enough to encompass the region that is used of
     * the Func by the rest of the pipeline. */
    halide_error_code_explicit_bounds_too_small = -2,

    /** The elem_size field of a halide_buffer_t does not match the size in
     * bytes of the type of that ImageParam. Probable type mismatch. */
    halide_error_code_bad_type = -3,

    /** A pipeline would access memory outside of the halide_buffer_t passed
     * in. */
    halide_error_code_access_out_of_bounds = -4,

    /** A halide_buffer_t was given that spans more than 2GB of memory. */
    halide_error_code_buffer_allocation_too_large = -5,

    /** A halide_buffer_t was given with extents that multiply to a number
     * greater than 2^31-1 */
    halide_error_code_buffer_extents_too_large = -6,

    /** Applying explicit constraints on the size of an input or
     * output buffer shrank the size of that buffer below what will be
     * accessed by the pipeline. */
    halide_error_code_constraints_make_required_region_smaller = -7,

    /** A constraint on a size or stride of an input or output buffer
     * was not met by the halide_buffer_t passed in. */
    halide_error_code_constraint_violated = -8,

    /** A scalar parameter passed in was smaller than its minimum
     * declared value. */
    halide_error_code_param_too_small = -9,

    /** A scalar parameter passed in was greater than its minimum
     * declared value. */
    halide_error_code_param_too_large = -10,

    /** A call to halide_malloc returned NULL. */
    halide_error_code_out_of_memory = -11,

    /** A halide_buffer_t pointer passed in was NULL. */
    halide_error_code_buffer_argument_is_null = -12,

    /** debug_to_file failed to open or write to the specified
     * file. */
    halide_error_code_debug_to_file_failed = -13,

    /** The Halide runtime encountered an error while trying to copy
     * from device to host. Turn on -debug in your target string to
     * see more details. */
    halide_error_code_copy_to_host_failed = -14,

    /** The Halide runtime encountered an error while trying to copy
     * from host to device. Turn on -debug in your target string to
     * see more details. */
    halide_error_code_copy_to_device_failed = -15,

    /** The Halide runtime encountered an error while trying to
     * allocate memory on device. Turn on -debug in your target string
     * to see more details. */
    halide_error_code_device_malloc_failed = -16,

    /** The Halide runtime encountered an error while trying to
     * synchronize with a device. Turn on -debug in your target string
     * to see more details. */
    halide_error_code_device_sync_failed = -17,

    /** The Halide runtime encountered an error while trying to free a
     * device allocation. Turn on -debug in your target string to see
     * more details. */
    halide_error_code_device_free_failed = -18,

    /** Buffer has a non-zero device but no device interface, which
     * violates a Halide invariant. */
    halide_error_code_no_device_interface = -19,

    /** An error occurred when attempting to initialize the Matlab
     * runtime. */
    halide_error_code_matlab_init_failed = -20,

    /** The type of an mxArray did not match the expected type. */
    halide_error_code_matlab_bad_param_type = -21,

    /** There is a bug in the Halide compiler. */
    halide_error_code_internal_error = -22,

    /** The Halide runtime encountered an error while trying to launch
     * a GPU kernel. Turn on -debug in your target string to see more
     * details. */
    halide_error_code_device_run_failed = -23,

    /** The Halide runtime encountered a host pointer that violated
     * the alignment set for it by way of a call to
     * set_host_alignment */
    halide_error_code_unaligned_host_ptr = -24,

    /** A fold_storage directive was used on a dimension that is not
     * accessed in a monotonically increasing or decreasing fashion. */
    halide_error_code_bad_fold = -25,

    /** A fold_storage directive was used with a fold factor that was
     * too small to store all the values of a producer needed by the
     * consumer. */
    halide_error_code_fold_factor_too_small = -26,

    /** User-specified require() expression was not satisfied. */
    halide_error_code_requirement_failed = -27,

    /** At least one of the buffer's extents are negative. */
    halide_error_code_buffer_extents_negative = -28,

    /** A compiled pipeline was passed the old deprecated buffer_t
     * struct, and it could not be upgraded to a halide_buffer_t. */
    halide_error_code_failed_to_upgrade_buffer_t = -29,

    /** A compiled pipeline was passed the old deprecated buffer_t
     * struct in bounds inference mode, but the returned information
     * can't be expressed in the old buffer_t. */
    halide_error_code_failed_to_downgrade_buffer_t = -30,

    /** A specialize_fail() schedule branch was selected at runtime. */
    halide_error_code_specialize_fail = -31,

    /** The Halide runtime encountered an error while trying to wrap a
     * native device handle.  Turn on -debug in your target string to
     * see more details. */
    halide_error_code_device_wrap_native_failed = -32,

    /** The Halide runtime encountered an error while trying to detach
     * a native device handle.  Turn on -debug in your target string
     * to see more details. */
    halide_error_code_device_detach_native_failed = -33,

    /** The host field on an input or output was null, the device
     * field was not zero, and the pipeline tries to use the buffer on
     * the host. You may be passing a GPU-only buffer to a pipeline
     * which is scheduled to use it on the CPU. */
    halide_error_code_host_is_null = -34,

    /** A folded buffer was passed to an extern stage, but the region
     * touched wraps around the fold boundary. */
    halide_error_code_bad_extern_fold = -35,

    /** Buffer has a non-null device_interface but device is 0, which
     * violates a Halide invariant. */
    halide_error_code_device_interface_no_device= -36,

    /** Buffer has both host and device dirty bits set, which violates
     * a Halide invariant. */
    halide_error_code_host_and_device_dirty = -37,

    /** The halide_buffer_t * passed to a halide runtime routine is
     * nullptr and this is not allowed. */
    halide_error_code_buffer_is_null = -38,

    /** The Halide runtime encountered an error while trying to copy
     * from one buffer to another. Turn on -debug in your target
     * string to see more details. */
    halide_error_code_device_buffer_copy_failed = -39,

    /** Attempted to make cropped/sliced alias of a buffer with a device
     * field, but the device_interface does not support cropping. */
    halide_error_code_device_crop_unsupported = -40,

    /** Cropping/slicing a buffer failed for some other reason. Turn on -debug
     * in your target string. */
    halide_error_code_device_crop_failed = -41,

    /** An operation on a buffer required an allocation on a
     * particular device interface, but a device allocation already
     * existed on a different device interface. Free the old one
     * first. */
    halide_error_code_incompatible_device_interface = -42,

    /** The dimensions field of a halide_buffer_t does not match the dimensions of that ImageParam. */
    halide_error_code_bad_dimensions = -43,

    /** An expression that would perform an integer division or modulo
     * by zero was evaluated. */
    halide_error_code_integer_division_by_zero = -44,

};

/** Halide calls the functions below on various error conditions. The
 * default implementations construct an error message, call
 * halide_error, then return the matching error code above. On
 * platforms that support weak linking, you can override these to
 * catch the errors individually. */

/** A call into an extern stage for the purposes of bounds inference
 * failed. Returns the error code given by the extern stage. */
extern int halide_error_bounds_inference_call_failed(void *user_context, const char *extern_stage_name, int result);

/** A call to an extern stage failed. Returned the error code given by
 * the extern stage. */
extern int halide_error_extern_stage_failed(void *user_context, const char *extern_stage_name, int result);

/** Various other error conditions. See the enum above for a
 * description of each. */
// @{
extern int halide_error_explicit_bounds_too_small(void *user_context, const char *func_name, const char *var_name,
                                                      int min_bound, int max_bound, int min_required, int max_required);
extern int halide_error_bad_type(void *user_context, const char *func_name,
                                 uint32_t type_given, uint32_t correct_type); // N.B. The last two args are the bit representation of a halide_type_t
extern int halide_error_bad_dimensions(void *user_context, const char *func_name,
                                       int32_t dimensions_given, int32_t correct_dimensions);
extern int halide_error_access_out_of_bounds(void *user_context, const char *func_name,
                                             int dimension, int min_touched, int max_touched,
                                             int min_valid, int max_valid);
extern int halide_error_buffer_allocation_too_large(void *user_context, const char *buffer_name,
                                                    uint64_t allocation_size, uint64_t max_size);
extern int halide_error_buffer_extents_negative(void *user_context, const char *buffer_name, int dimension, int extent);
extern int halide_error_buffer_extents_too_large(void *user_context, const char *buffer_name,
                                                 int64_t actual_size, int64_t max_size);
extern int halide_error_constraints_make_required_region_smaller(void *user_context, const char *buffer_name,
                                                                 int dimension,
                                                                 int constrained_min, int constrained_extent,
                                                                 int required_min, int required_extent);
extern int halide_error_constraint_violated(void *user_context, const char *var, int val,
                                            const char *constrained_var, int constrained_val);
extern int halide_error_param_too_small_i64(void *user_context, const char *param_name,
                                            int64_t val, int64_t min_val);
extern int halide_error_param_too_small_u64(void *user_context, const char *param_name,
                                            uint64_t val, uint64_t min_val);
extern int halide_error_param_too_small_f64(void *user_context, const char *param_name,
                                            double val, double min_val);
extern int halide_error_param_too_large_i64(void *user_context, const char *param_name,
                                            int64_t val, int64_t max_val);
extern int halide_error_param_too_large_u64(void *user_context, const char *param_name,
                                            uint64_t val, uint64_t max_val);
extern int halide_error_param_too_large_f64(void *user_context, const char *param_name,
                                            double val, double max_val);
extern int halide_error_out_of_memory(void *user_context);
extern int halide_error_buffer_argument_is_null(void *user_context, const char *buffer_name);
extern int halide_error_debug_to_file_failed(void *user_context, const char *func,
                                             const char *filename, int error_code);
extern int halide_error_unaligned_host_ptr(void *user_context, const char *func_name, int alignment);
extern int halide_error_host_is_null(void *user_context, const char *func_name);
extern int halide_error_failed_to_upgrade_buffer_t(void *user_context,
                                                   const char *input_name,
                                                   const char *reason);
extern int halide_error_failed_to_downgrade_buffer_t(void *user_context,
                                                     const char *input_name,
                                                     const char *reason);
extern int halide_error_bad_fold(void *user_context, const char *func_name, const char *var_name,
                                 const char *loop_name);
extern int halide_error_bad_extern_fold(void *user_context, const char *func_name,
                                        int dim, int min, int extent, int valid_min, int fold_factor);

extern int halide_error_fold_factor_too_small(void *user_context, const char *func_name, const char *var_name,
                                              int fold_factor, const char *loop_name, int required_extent);
extern int halide_error_requirement_failed(void *user_context, const char *condition, const char *message);
extern int halide_error_specialize_fail(void *user_context, const char *message);
extern int halide_error_no_device_interface(void *user_context);
extern int halide_error_device_interface_no_device(void *user_context);
extern int halide_error_host_and_device_dirty(void *user_context);
extern int halide_error_buffer_is_null(void *user_context, const char *routine);
extern int halide_error_integer_division_by_zero(void *user_context);
// @}

/** Optional features a compilation Target can have.
 * Be sure to keep this in sync with the Feature enum in Target.h and the implementation of
 * get_runtime_compatible_target in Target.cpp if you add a new feature.
 */
typedef enum halide_target_feature_t {
    halide_target_feature_jit = 0,  ///< Generate code that will run immediately inside the calling process.
    halide_target_feature_debug,  ///< Turn on debug info and output for runtime code.
    halide_target_feature_no_asserts,  ///< Disable all runtime checks, for slightly tighter code.
    halide_target_feature_no_bounds_query, ///< Disable the bounds querying functionality.

    halide_target_feature_sse41,  ///< Use SSE 4.1 and earlier instructions. Only relevant on x86.
    halide_target_feature_avx,  ///< Use AVX 1 instructions. Only relevant on x86.
    halide_target_feature_avx2,  ///< Use AVX 2 instructions. Only relevant on x86.
    halide_target_feature_fma,  ///< Enable x86 FMA instruction
    halide_target_feature_fma4,  ///< Enable x86 (AMD) FMA4 instruction set
    halide_target_feature_f16c,  ///< Enable x86 16-bit float support

    halide_target_feature_armv7s,  ///< Generate code for ARMv7s. Only relevant for 32-bit ARM.
    halide_target_feature_no_neon,  ///< Avoid using NEON instructions. Only relevant for 32-bit ARM.

    halide_target_feature_vsx,  ///< Use VSX instructions. Only relevant on POWERPC.
    halide_target_feature_power_arch_2_07,  ///< Use POWER ISA 2.07 new instructions. Only relevant on POWERPC.

    halide_target_feature_cuda,  ///< Enable the CUDA runtime. Defaults to compute capability 2.0 (Fermi)
    halide_target_feature_cuda_capability30,  ///< Enable CUDA compute capability 3.0 (Kepler)
    halide_target_feature_cuda_capability32,  ///< Enable CUDA compute capability 3.2 (Tegra K1)
    halide_target_feature_cuda_capability35,  ///< Enable CUDA compute capability 3.5 (Kepler)
    halide_target_feature_cuda_capability50,  ///< Enable CUDA compute capability 5.0 (Maxwell)

    halide_target_feature_opencl,  ///< Enable the OpenCL runtime.
    halide_target_size_opencl, ///<Temporary testing target placeholder marked by size>
    halide_target_feature_cl_doubles,  ///< Enable double support on OpenCL targets
    halide_target_feature_cl_atomic64, ///< Enable 64-bit atomics operations on OpenCL targets

    halide_target_feature_opengl,  ///< Enable the OpenGL runtime.
    halide_target_feature_openglcompute, ///< Enable OpenGL Compute runtime.

    halide_target_feature_user_context,  ///< Generated code takes a user_context pointer as first argument

    halide_target_feature_matlab,  ///< Generate a mexFunction compatible with Matlab mex libraries. See tools/mex_halide.m.

    halide_target_feature_profile, ///< Launch a sampling profiler alongside the Halide pipeline that monitors and reports the runtime used by each Func
    halide_target_feature_no_runtime, ///< Do not include a copy of the Halide runtime in any generated object file or assembly

    halide_target_feature_metal, ///< Enable the (Apple) Metal runtime.
    halide_target_feature_mingw, ///< For Windows compile to MinGW toolset rather then Visual Studio

    halide_target_feature_c_plus_plus_mangling, ///< Generate C++ mangled names for result function, et al

    halide_target_feature_large_buffers, ///< Enable 64-bit buffer indexing to support buffers > 2GB. Ignored if bits != 64.

    halide_target_feature_hvx_64, ///< Enable HVX 64 byte mode.
    halide_target_feature_hvx_128, ///< Enable HVX 128 byte mode.
    halide_target_feature_hvx_v62, ///< Enable Hexagon v62 architecture.
    halide_target_feature_fuzz_float_stores, ///< On every floating point store, set the last bit of the mantissa to zero. Pipelines for which the output is very different with this feature enabled may also produce very different output on different processors.
    halide_target_feature_soft_float_abi, ///< Enable soft float ABI. This only enables the soft float ABI calling convention, which does not necessarily use soft floats.
    halide_target_feature_msan, ///< Enable hooks for MSAN support.
    halide_target_feature_avx512, ///< Enable the base AVX512 subset supported by all AVX512 architectures. The specific feature sets are AVX-512F and AVX512-CD. See https://en.wikipedia.org/wiki/AVX-512 for a description of each AVX subset.
    halide_target_feature_avx512_knl, ///< Enable the AVX512 features supported by Knight's Landing chips, such as the Xeon Phi x200. This includes the base AVX512 set, and also AVX512-CD and AVX512-ER.
    halide_target_feature_avx512_skylake, ///< Enable the AVX512 features supported by Skylake Xeon server processors. This adds AVX512-VL, AVX512-BW, and AVX512-DQ to the base set. The main difference from the base AVX512 set is better support for small integer ops. Note that this does not include the Knight's Landing features. Note also that these features are not available on Skylake desktop and mobile processors.
    halide_target_feature_avx512_cannonlake, ///< Enable the AVX512 features expected to be supported by future Cannonlake processors. This includes all of the Skylake features, plus AVX512-IFMA and AVX512-VBMI.
    halide_target_feature_hvx_use_shared_object, ///< Deprecated
    halide_target_feature_trace_loads, ///< Trace all loads done by the pipeline. Equivalent to calling Func::trace_loads on every non-inlined Func.
    halide_target_feature_trace_stores, ///< Trace all stores done by the pipeline. Equivalent to calling Func::trace_stores on every non-inlined Func.
    halide_target_feature_trace_realizations, ///< Trace all realizations done by the pipeline. Equivalent to calling Func::trace_realizations on every non-inlined Func.
    halide_target_feature_trace_pipeline, ///< Trace the pipeline.
    halide_target_feature_cuda_capability61,  ///< Enable CUDA compute capability 6.1 (Pascal)
    halide_target_feature_hvx_v65, ///< Enable Hexagon v65 architecture.
    halide_target_feature_hvx_v66, ///< Enable Hexagon v66 architecture.
    halide_target_feature_cl_half,  ///< Enable half support on OpenCL targets
    halide_target_feature_strict_float, ///< Turn off all non-IEEE floating-point optimization. Currently applies only to LLVM targets.
    halide_target_feature_legacy_buffer_wrappers,  ///< Emit legacy wrapper code for buffer_t (vs halide_buffer_t) when AOT-compiled.
    halide_target_feature_tsan, ///< Enable hooks for TSAN support.
    halide_target_feature_asan, ///< Enable hooks for ASAN support.
    halide_target_feature_d3d12compute, ///< Enable Direct3D 12 Compute runtime.
    halide_target_feature_check_unsafe_promises, ///< Insert assertions for promises.
    halide_target_feature_hexagon_dma, ///< Enable Hexagon DMA buffers.
    halide_target_feature_embed_bitcode,  ///< Emulate clang -fembed-bitcode flag.
    halide_target_feature_enable_llvm_loop_opt,  ///< Enable loop vectorization + unrolling in LLVM. Overrides halide_target_feature_disable_llvm_loop_opt. (Ignored for non-LLVM targets.)
    halide_target_feature_disable_llvm_loop_opt,  ///< Disable loop vectorization + unrolling in LLVM. (Ignored for non-LLVM targets.)
    halide_target_feature_wasm_simd128,  ///< Enable +simd128 instructions for WebAssembly codegen.
    halide_target_feature_wasm_signext,  ///< Enable +sign-ext instructions for WebAssembly codegen.
    halide_target_feature_sve, ///< Enable ARM Scalable Vector Extensions
    halide_target_feature_sve2, ///< Enable ARM Scalable Vector Extensions v2
    halide_target_feature_egl,            ///< Force use of EGL support.
    halide_target_feature_intel_fpga, ///< Enable Intel FPGAs
    halide_target_feature_one_api, ///< Enable Intel OneAPI dpcpp program generation
    halide_target_feature_intel_gpu, ///< Enable Intel Graphics
    halide_target_feature_enable_synthesis, ///< Enable synthesizing binaries. Currently used only for Intel FPGAs.
    halide_target_feature_end ///< A sentinel. Every target is considered to have this feature, and setting this feature does nothing.
} halide_target_feature_t;

/** This function is called internally by Halide in some situations to determine
 * if the current execution environment can support the given set of
 * halide_target_feature_t flags. The implementation must do the following:
 *
 * -- If there are flags set in features that the function knows *cannot* be supported, return 0.
 * -- Otherwise, return 1.
 * -- Note that any flags set in features that the function doesn't know how to test should be ignored;
 * this implies that a return value of 1 means "not known to be bad" rather than "known to be good".
 *
 * In other words: a return value of 0 means "It is not safe to use code compiled with these features",
 * while a return value of 1 means "It is not obviously unsafe to use code compiled with these features".
 *
 * The default implementation simply calls halide_default_can_use_target_features.
 *
 * Note that `features` points to an array of `count` uint64_t; this array must contain enough
 * bits to represent all the currently known features. Any excess bits must be set to zero.
 */
// @{
extern int halide_can_use_target_features(int count, const uint64_t *features);
typedef int (*halide_can_use_target_features_t)(int count, const uint64_t *features);
extern halide_can_use_target_features_t halide_set_custom_can_use_target_features(halide_can_use_target_features_t);
// @}

/**
 * This is the default implementation of halide_can_use_target_features; it is provided
 * for convenience of user code that may wish to extend halide_can_use_target_features
 * but continue providing existing support, e.g.
 *
 *     int halide_can_use_target_features(int count, const uint64_t *features) {
 *          if (features[halide_target_somefeature >> 6] & (1LL << (halide_target_somefeature & 63))) {
 *              if (!can_use_somefeature()) {
 *                  return 0;
 *              }
 *          }
 *          return halide_default_can_use_target_features(count, features);
 *     }
 */
extern int halide_default_can_use_target_features(int count, const uint64_t *features);


typedef struct halide_dimension_t {
    int32_t min, extent, stride;

    // Per-dimension flags. None are defined yet (This is reserved for future use).
    uint32_t flags;

#ifdef __cplusplus
    HALIDE_ALWAYS_INLINE halide_dimension_t() : min(0), extent(0), stride(0), flags(0) {}
    HALIDE_ALWAYS_INLINE halide_dimension_t(int32_t m, int32_t e, int32_t s, uint32_t f = 0) :
        min(m), extent(e), stride(s), flags(f) {}

    HALIDE_ALWAYS_INLINE bool operator==(const halide_dimension_t &other) const {
        return (min == other.min) &&
            (extent == other.extent) &&
            (stride == other.stride) &&
            (flags == other.flags);
    }

    HALIDE_ALWAYS_INLINE bool operator!=(const halide_dimension_t &other) const {
        return !(*this == other);
    }
#endif
} halide_dimension_t;

#ifdef __cplusplus
} // extern "C"
#endif

typedef enum {halide_buffer_flag_host_dirty = 1,
              halide_buffer_flag_device_dirty = 2} halide_buffer_flags;

/**
 * The raw representation of an image passed around by generated
 * Halide code. It includes some stuff to track whether the image is
 * not actually in main memory, but instead on a device (like a
 * GPU). For a more convenient C++ wrapper, use Halide::Buffer<T>. */
typedef struct halide_buffer_t {
    /** A device-handle for e.g. GPU memory used to back this buffer. */
    uint64_t device;

    /** The interface used to interpret the above handle. */
    const struct halide_device_interface_t *device_interface;

    /** A pointer to the start of the data in main memory. In terms of
     * the Halide coordinate system, this is the address of the min
     * coordinates (defined below). */
    uint8_t* host;

    /** flags with various meanings. */
    uint64_t flags;

    /** The type of each buffer element. */
    struct halide_type_t type;

    /** The dimensionality of the buffer. */
    int32_t dimensions;

    /** The shape of the buffer. Halide does not own this array - you
     * must manage the memory for it yourself. */
    halide_dimension_t *dim;

    /** Pads the buffer up to a multiple of 8 bytes */
    void *padding;

#ifdef __cplusplus
    /** Convenience methods for accessing the flags */
    // @{
    HALIDE_ALWAYS_INLINE bool get_flag(halide_buffer_flags flag) const {
        return (flags & flag) != 0;
    }

    HALIDE_ALWAYS_INLINE void set_flag(halide_buffer_flags flag, bool value) {
        if (value) {
            flags |= flag;
        } else {
            flags &= ~flag;
        }
    }

    HALIDE_ALWAYS_INLINE bool host_dirty() const {
        return get_flag(halide_buffer_flag_host_dirty);
    }

    HALIDE_ALWAYS_INLINE bool device_dirty() const {
        return get_flag(halide_buffer_flag_device_dirty);
    }

    HALIDE_ALWAYS_INLINE void set_host_dirty(bool v = true) {
        set_flag(halide_buffer_flag_host_dirty, v);
    }

    HALIDE_ALWAYS_INLINE void set_device_dirty(bool v = true) {
        set_flag(halide_buffer_flag_device_dirty, v);
    }
    // @}

    /** The total number of elements this buffer represents. Equal to
     * the product of the extents */
    HALIDE_ALWAYS_INLINE size_t number_of_elements() const {
        size_t s = 1;
        for (int i = 0; i < dimensions; i++) {
            s *= dim[i].extent;
        }
        return s;
    }

    /** A pointer to the element with the lowest address. If all
     * strides are positive, equal to the host pointer. */
    HALIDE_ALWAYS_INLINE uint8_t *begin() const {
        ptrdiff_t index = 0;
        for (int i = 0; i < dimensions; i++) {
            if (dim[i].stride < 0) {
                index += dim[i].stride * (dim[i].extent - 1);
            }
        }
        return host + index * type.bytes();
    }

    /** A pointer to one beyond the element with the highest address. */
    HALIDE_ALWAYS_INLINE uint8_t *end() const {
        ptrdiff_t index = 0;
        for (int i = 0; i < dimensions; i++) {
            if (dim[i].stride > 0) {
                index += dim[i].stride * (dim[i].extent - 1);
            }
        }
        index += 1;
        return host + index * type.bytes();
    }

    /** The total number of bytes spanned by the data in memory. */
    HALIDE_ALWAYS_INLINE size_t size_in_bytes() const {
        return (size_t)(end() - begin());
    }

    /** A pointer to the element at the given location. */
    HALIDE_ALWAYS_INLINE uint8_t *address_of(const int *pos) const {
        ptrdiff_t index = 0;
        for (int i = 0; i < dimensions; i++) {
            index += dim[i].stride * (pos[i] - dim[i].min);
        }
        return host + index * type.bytes();
    }

    /** Attempt to call device_sync for the buffer. If the buffer
     * has no device_interface (or no device_sync), this is a quiet no-op.
     * Calling this explicitly should rarely be necessary, except for profiling. */
    HALIDE_ALWAYS_INLINE int device_sync(void *ctx = NULL) {
        if (device_interface && device_interface->device_sync) {
            return device_interface->device_sync(ctx, this);
        }
        return 0;
    }

    /** Check if an input buffer passed extern stage is a querying
     * bounds. Compared to doing the host pointer check directly,
     * this both adds clarity to code and will facilitate moving to
     * another representation for bounds query arguments. */
    HALIDE_ALWAYS_INLINE bool is_bounds_query() const {
        return host == NULL && device == 0;
    }

#endif
} halide_buffer_t;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HALIDE_ATTRIBUTE_DEPRECATED
#ifdef HALIDE_ALLOW_DEPRECATED
#define HALIDE_ATTRIBUTE_DEPRECATED(x)
#else
#ifdef _MSC_VER
#define HALIDE_ATTRIBUTE_DEPRECATED(x) __declspec(deprecated(x))
#else
#define HALIDE_ATTRIBUTE_DEPRECATED(x) __attribute__((deprecated(x)))
#endif
#endif
#endif

/** The old buffer_t, included for compatibility with old code. Don't
 * use it. */
#ifndef BUFFER_T_DEFINED
#define BUFFER_T_DEFINED
typedef struct buffer_t {
    uint64_t dev;
    uint8_t* host;
    int32_t extent[4];
    int32_t stride[4];
    int32_t min[4];
    int32_t elem_size;
    HALIDE_ATTRIBUTE_ALIGN(1) bool host_dirty;
    HALIDE_ATTRIBUTE_ALIGN(1) bool dev_dirty;
    HALIDE_ATTRIBUTE_ALIGN(1) uint8_t _padding[10 - sizeof(void *)];
} buffer_t;
#endif // BUFFER_T_DEFINED

/** Copies host pointer, mins, extents, strides, and device state from
 * an old-style buffer_t into a new-style halide_buffer_t. If bounds_query_only is nonzero,
 * the copy is only done if the old_buf has null host and dev (ie, a bounds query is being
 * performed); otherwise new_buf is left untouched. (This is used for input buffers to avoid
 * benign data races.) The dimensions and type fields of the new buffer_t should already be
 * set. Returns an error code if the upgrade could not be performed. */
extern int halide_upgrade_buffer_t(void *user_context, const char *name,
                                   const buffer_t *old_buf, halide_buffer_t *new_buf,
                                   int bounds_query_only);

/** Copies the host pointer, mins, extents, strides, and device state
 * from a halide_buffer_t to a buffer_t. Also sets elem_size. Useful
 * for backporting the results of bounds inference. */
extern int halide_downgrade_buffer_t(void *user_context, const char *name,
                                     const halide_buffer_t *new_buf, buffer_t *old_buf);

/** Copies the dirty flags and device allocation state from a new
 * buffer_t back to a legacy buffer_t. */
extern int halide_downgrade_buffer_t_device_fields(void *user_context, const char *name,
                                                   const halide_buffer_t *new_buf, buffer_t *old_buf);

/** halide_scalar_value_t is a simple union able to represent all the well-known
 * scalar values in a filter argument. Note that it isn't tagged with a type;
 * you must ensure you know the proper type before accessing. Most user
 * code will never need to create instances of this struct; its primary use
 * is to hold def/min/max values in a halide_filter_argument_t. (Note that
 * this is conceptually just a union; it's wrapped in a struct to ensure
 * that it doesn't get anonymized by LLVM.)
 */
struct halide_scalar_value_t {
    union {
        bool b;
        int8_t i8;
        int16_t i16;
        int32_t i32;
        int64_t i64;
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
        float f32;
        double f64;
        void *handle;
    } u;
    #ifdef __cplusplus
    HALIDE_ALWAYS_INLINE halide_scalar_value_t() {u.u64 = 0;}
    #endif
};

enum halide_argument_kind_t {
    halide_argument_kind_input_scalar = 0,
    halide_argument_kind_input_buffer = 1,
    halide_argument_kind_output_buffer = 2
};

/*
    These structs must be robust across different compilers and settings; when
    modifying them, strive for the following rules:

    1) All fields are explicitly sized. I.e. must use int32_t and not "int"
    2) All fields must land on an alignment boundary that is the same as their size
    3) Explicit padding is added to make that so
    4) The sizeof the struct is padded out to a multiple of the largest natural size thing in the struct
    5) don't forget that 32 and 64 bit pointers are different sizes
*/

/**
 * Obsolete version of halide_filter_argument_t; only present in
 * code that wrote halide_filter_metadata_t version 0.
 */
struct halide_filter_argument_t_v0 {
    const char *name;
    int32_t kind;
    int32_t dimensions;
    struct halide_type_t type;
    const struct halide_scalar_value_t *def, *min, *max;
};

/**
 * halide_filter_argument_t is essentially a plain-C-struct equivalent to
 * Halide::Argument; most user code will never need to create one.
 */
struct halide_filter_argument_t {
    const char *name;       // name of the argument; will never be null or empty.
    int32_t kind;           // actually halide_argument_kind_t
    int32_t dimensions;     // always zero for scalar arguments
    struct halide_type_t type;
    // These pointers should always be null for buffer arguments,
    // and *may* be null for scalar arguments. (A null value means
    // there is no def/min/max/estimate specified for this argument.)
    const struct halide_scalar_value_t *scalar_def, *scalar_min, *scalar_max, *scalar_estimate;
    // This pointer should always be null for scalar arguments,
    // and *may* be null for buffer arguments. If not null, it should always
    // point to an array of dimensions*2 pointers, which will be the (min, extent)
    // estimates for each dimension of the buffer. (Note that any of the pointers
    // may be null as well.)
    int64_t const* const* buffer_estimates;
};

struct halide_filter_metadata_t {
#ifdef __cplusplus
    static const int32_t VERSION = 1;
#endif

    /** version of this metadata; currently always 1. */
    int32_t version;

    /** The number of entries in the arguments field. This is always >= 1. */
    int32_t num_arguments;

    /** An array of the filters input and output arguments; this will never be
     * null. The order of arguments is not guaranteed (input and output arguments
     * may come in any order); however, it is guaranteed that all arguments
     * will have a unique name within a given filter. */
    const struct halide_filter_argument_t* arguments;

    /** The Target for which the filter was compiled. This is always
     * a canonical Target string (ie a product of Target::to_string). */
    const char* target;

    /** The function name of the filter. */
    const char* name;
};

/** halide_register_argv_and_metadata() is a **user-defined** function that
 * must be provided in order to use the registration.cc files produced
 * by Generators when the 'registration' output is requested. Each registration.cc
 * file provides a static initializer that calls this function with the given
 * filter's argv-call variant, its metadata, and (optionally) and additional
 * textual data that the build system chooses to tack on for its own purposes.
 * Note that this will be called at static-initializer time (i.e., before
 * main() is called), and in an unpredictable order. Note that extra_key_value_pairs
 * may be nullptr; if it's not null, it's expected to be a null-terminated list
 * of strings, with an even number of entries. */
void halide_register_argv_and_metadata(
    int (*filter_argv_call)(void **),
    const struct halide_filter_metadata_t *filter_metadata,
    const char * const *extra_key_value_pairs
);

/** The functions below here are relevant for pipelines compiled with
 * the -profile target flag, which runs a sampling profiler thread
 * alongside the pipeline. */

/** Per-Func state tracked by the sampling profiler. */
struct halide_profiler_func_stats {
    /** Total time taken evaluating this Func (in nanoseconds). */
    uint64_t time;

    /** The current memory allocation of this Func. */
    uint64_t memory_current;

    /** The peak memory allocation of this Func. */
    uint64_t memory_peak;

    /** The total memory allocation of this Func. */
    uint64_t memory_total;

    /** The peak stack allocation of this Func's threads. */
    uint64_t stack_peak;

    /** The average number of thread pool worker threads active while computing this Func. */
    uint64_t active_threads_numerator, active_threads_denominator;

    /** The name of this Func. A global constant string. */
    const char *name;

    /** The total number of memory allocation of this Func. */
    int num_allocs;
};

/** Per-pipeline state tracked by the sampling profiler. These exist
 * in a linked list. */
struct halide_profiler_pipeline_stats {
    /** Total time spent inside this pipeline (in nanoseconds) */
    uint64_t time;

    /** The current memory allocation of funcs in this pipeline. */
    uint64_t memory_current;

    /** The peak memory allocation of funcs in this pipeline. */
    uint64_t memory_peak;

    /** The total memory allocation of funcs in this pipeline. */
    uint64_t memory_total;

    /** The average number of thread pool worker threads doing useful
     * work while computing this pipeline. */
    uint64_t active_threads_numerator, active_threads_denominator;

    /** The name of this pipeline. A global constant string. */
    const char *name;

    /** An array containing states for each Func in this pipeline. */
    struct halide_profiler_func_stats *funcs;

    /** The next pipeline_stats pointer. It's a void * because types
     * in the Halide runtime may not currently be recursive. */
    void *next;

    /** The number of funcs in this pipeline. */
    int num_funcs;

    /** An internal base id used to identify the funcs in this pipeline. */
    int first_func_id;

    /** The number of times this pipeline has been run. */
    int runs;

    /** The total number of samples taken inside of this pipeline. */
    int samples;

    /** The total number of memory allocation of funcs in this pipeline. */
    int num_allocs;
};

/** The global state of the profiler. */

struct halide_profiler_state {
    /** Guards access to the fields below. If not locked, the sampling
     * profiler thread is free to modify things below (including
     * reordering the linked list of pipeline stats). */
    struct halide_mutex lock;

    /** The amount of time the profiler thread sleeps between samples
     * in milliseconds. Defaults to 1 */
    int sleep_time;

    /** An internal id used for bookkeeping. */
    int first_free_id;

    /** The id of the current running Func. Set by the pipeline, read
     * periodically by the profiler thread. */
    int current_func;

    /** The number of threads currently doing work. */
    int active_threads;

    /** A linked list of stats gathered for each pipeline. */
    struct halide_profiler_pipeline_stats *pipelines;

    /** Retrieve remote profiler state. Used so that the sampling
     * profiler can follow along with execution that occurs elsewhere,
     * e.g. on a DSP. If null, it reads from the int above instead. */
    void (*get_remote_profiler_state)(int *func, int *active_workers);

    /** Sampling thread reference to be joined at shutdown. */
    struct halide_thread *sampling_thread;
};

/** Profiler func ids with special meanings. */
enum {
    /// current_func takes on this value when not inside Halide code
    halide_profiler_outside_of_halide = -1,
    /// Set current_func to this value to tell the profiling thread to
    /// halt. It will start up again next time you run a pipeline with
    /// profiling enabled.
    halide_profiler_please_stop = -2
};

/** Get a pointer to the global profiler state for programmatic
 * inspection. Lock it before using to pause the profiler. */
extern struct halide_profiler_state *halide_profiler_get_state();

/** Get a pointer to the pipeline state associated with pipeline_name.
 * This function grabs the global profiler state's lock on entry. */
extern struct halide_profiler_pipeline_stats *halide_profiler_get_pipeline_state(const char *pipeline_name);

/** Reset profiler state cheaply. May leave threads running or some
 * memory allocated but all accumluated statistics are reset.
 * WARNING: Do NOT call this method while any halide pipeline is
 * running; halide_profiler_memory_allocate/free and
 * halide_profiler_stack_peak_update update the profiler pipeline's
 * state without grabbing the global profiler state's lock. */
extern void halide_profiler_reset();

/** Reset all profiler state.
 * WARNING: Do NOT call this method while any halide pipeline is
 * running; halide_profiler_memory_allocate/free and
 * halide_profiler_stack_peak_update update the profiler pipeline's
 * state without grabbing the global profiler state's lock. */
void halide_profiler_shutdown();

/** Print out timing statistics for everything run since the last
 * reset. Also happens at process exit. */
extern void halide_profiler_report(void *user_context);

/// \name "Float16" functions
/// These functions operate of bits (``uint16_t``) representing a half
/// precision floating point number (IEEE-754 2008 binary16).
//{@

/** Read bits representing a half precision floating point number and return
 *  the float that represents the same value */
extern float halide_float16_bits_to_float(uint16_t);

/** Read bits representing a half precision floating point number and return
 *  the double that represents the same value */
extern double halide_float16_bits_to_double(uint16_t);

// TODO: Conversion functions to half

//@}

// Allocating and freeing device memory is often very slow. The
// methods below give Halide's runtime permission to hold onto device
// memory to service future requests instead of returning it to the
// underlying device API. The API does not manage an allocation pool,
// all it does is provide access to a shared counter that acts as a
// limit on the unused memory not yet returned to the underlying
// device API. It makes callbacks to participants when memory needs to
// be released because the limit is about to be exceeded (either
// because the limit has been reduced, or because the memory owned by
// some participant becomes unused).

/** Tell Halide whether or not it is permitted to hold onto device
 * allocations to service future requests instead of returning them
 * eagerly to the underlying device API. Many device allocators are
 * quite slow, so it can be beneficial to set this to true. The
 * default value for now is false.
 *
 * Note that if enabled, the eviction policy is very simplistic. The
 * 32 most-recently used allocations are preserved, regardless of
 * their size. Additionally, if a call to cuMalloc results in an
 * out-of-memory error, the entire cache is flushed and the allocation
 * is retried. See https://github.com/halide/Halide/issues/4093
 *
 * If set to false, releases all unused device allocations back to the
 * underlying device APIs. For finer-grained control, see specific
 * methods in each device api runtime. */
extern int halide_reuse_device_allocations(void *user_context, bool);

/** Determines whether on device_free the memory is returned
 * immediately to the device API, or placed on a free list for future
 * use. Override and switch based on the user_context for
 * finer-grained control. By default just returns the value most
 * recently set by the method above. */
extern bool halide_can_reuse_device_allocations(void *user_context);

struct halide_device_allocation_pool {
    int (*release_unused)(void *user_context);
    struct halide_device_allocation_pool *next;
};

/** Register a callback to be informed when
 * halide_reuse_device_allocations(false) is called, and all unused
 * device allocations must be released. The object passed should have
 * global lifetime, and its next field will be clobbered. */
extern void halide_register_device_allocation_pool(struct halide_device_allocation_pool *);

#ifdef __cplusplus
} // End extern "C"
#endif

#ifdef __cplusplus

namespace {
template<typename T> struct check_is_pointer;
template<typename T> struct check_is_pointer<T *> {};
}

/** Construct the halide equivalent of a C type */
template<typename T>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of() {
    // Create a compile-time error if T is not a pointer (without
    // using any includes - this code goes into the runtime).
    check_is_pointer<T> check;
    (void)check;
    return halide_type_t(halide_type_handle, 64);
}

template<>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of<float>() {
    return halide_type_t(halide_type_float, 32);
}

template<>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of<double>() {
    return halide_type_t(halide_type_float, 64);
}

template<>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of<bool>() {
    return halide_type_t(halide_type_uint, 1);
}

template<>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of<uint8_t>() {
    return halide_type_t(halide_type_uint, 8);
}

template<>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of<uint16_t>() {
    return halide_type_t(halide_type_uint, 16);
}

template<>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of<uint32_t>() {
    return halide_type_t(halide_type_uint, 32);
}

template<>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of<uint64_t>() {
    return halide_type_t(halide_type_uint, 64);
}

template<>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of<int8_t>() {
    return halide_type_t(halide_type_int, 8);
}

template<>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of<int16_t>() {
    return halide_type_t(halide_type_int, 16);
}

template<>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of<int32_t>() {
    return halide_type_t(halide_type_int, 32);
}

template<>
HALIDE_ALWAYS_INLINE halide_type_t halide_type_of<int64_t>() {
    return halide_type_t(halide_type_int, 64);
}

#endif

#endif // HALIDE_HALIDERUNTIME_H

#ifdef COMPILING_HALIDE_RUNTIME
#include "HalideRuntime.h"
#define HALIDE_BUFFER_HELPER_ATTRS __attribute__((always_inline, weak))
#else
#define HALIDE_BUFFER_HELPER_ATTRS inline
#endif

// Structs are annoying to deal with from within Halide Stmts. These
// utility functions are for dealing with buffer_t in that
// context. They are not intended for use outside of Halide code, and
// not exposed in HalideRuntime.h. The symbols are private to the
// module and should be inlined and then stripped. This blob of code
// also gets copy-pasted into C outputs.

extern "C" {

HALIDE_BUFFER_HELPER_ATTRS
int _halide_buffer_get_dimensions(const halide_buffer_t *buf) {
    return buf->dimensions;
}

HALIDE_BUFFER_HELPER_ATTRS
uint8_t *_halide_buffer_get_host(const halide_buffer_t *buf) {
    return buf->host;
}

HALIDE_BUFFER_HELPER_ATTRS
uint64_t _halide_buffer_get_device(const halide_buffer_t *buf) {
    return buf->device;
}

HALIDE_BUFFER_HELPER_ATTRS
const struct halide_device_interface_t *_halide_buffer_get_device_interface(const halide_buffer_t *buf) {
    return buf->device_interface;
}

HALIDE_BUFFER_HELPER_ATTRS
int _halide_buffer_get_min(const halide_buffer_t *buf, int d) {
    return buf->dim[d].min;
}

HALIDE_BUFFER_HELPER_ATTRS
int _halide_buffer_get_max(const halide_buffer_t *buf, int d) {
    return buf->dim[d].min + buf->dim[d].extent - 1;
}

HALIDE_BUFFER_HELPER_ATTRS
int _halide_buffer_get_extent(const halide_buffer_t *buf, int d) {
    return buf->dim[d].extent;
}

HALIDE_BUFFER_HELPER_ATTRS
int _halide_buffer_get_stride(const halide_buffer_t *buf, int d) {
    return buf->dim[d].stride;
}

HALIDE_BUFFER_HELPER_ATTRS
int _halide_buffer_set_host_dirty(halide_buffer_t *buf, bool val) {
    buf->set_host_dirty(val);
    return 0;
}

HALIDE_BUFFER_HELPER_ATTRS
int _halide_buffer_set_device_dirty(halide_buffer_t *buf, bool val) {
    buf->set_device_dirty(val);
    return 0;
}

HALIDE_BUFFER_HELPER_ATTRS
bool _halide_buffer_get_host_dirty(const halide_buffer_t *buf) {
    return buf->host_dirty();
}

HALIDE_BUFFER_HELPER_ATTRS
bool _halide_buffer_get_device_dirty(const halide_buffer_t *buf) {
    return buf->device_dirty();
}

HALIDE_BUFFER_HELPER_ATTRS
halide_dimension_t *_halide_buffer_get_shape(halide_buffer_t *buf) {
    return buf->dim;
}

HALIDE_BUFFER_HELPER_ATTRS
bool _halide_buffer_is_bounds_query(const halide_buffer_t *buf) {
    return buf->host == NULL && buf->device == 0;
}

HALIDE_BUFFER_HELPER_ATTRS
uint32_t _halide_buffer_get_type(const halide_buffer_t *buf) {
    return buf->type.as_u32();
}

HALIDE_BUFFER_HELPER_ATTRS
halide_buffer_t *_halide_buffer_init(halide_buffer_t *dst,
                                     halide_dimension_t *dst_shape,
                                     void *host,
                                     uint64_t device,
                                     const halide_device_interface_t *device_interface,
                                     int type_code, int type_bits,
                                     int dimensions,
                                     halide_dimension_t *shape,
                                     uint64_t flags) {
    dst->host = (uint8_t *)host;
    dst->device = device;
    dst->device_interface = device_interface;
    dst->type.code = (halide_type_code_t)type_code;
    dst->type.bits = (uint8_t)type_bits;
    dst->type.lanes = 1;
    dst->dimensions = dimensions;
    dst->dim = dst_shape;
    if (shape != dst->dim) {
        for (int i = 0; i < dimensions; i++) {
            dst->dim[i] = shape[i];
        }
    }
    dst->flags = flags;
    return dst;
}

HALIDE_BUFFER_HELPER_ATTRS
halide_buffer_t *_halide_buffer_init_from_buffer(halide_buffer_t *dst,
                                                 halide_dimension_t *dst_shape,
                                                 const halide_buffer_t *src) {
    dst->host = src->host;
    dst->device = src->device;
    dst->device_interface = src->device_interface;
    dst->type = src->type;
    dst->dimensions = src->dimensions;
    dst->dim = dst_shape;
    dst->flags = src->flags;
    for (int i = 0; i < dst->dimensions; i++) {
        dst->dim[i] = src->dim[i];
    }
    return dst;
}

HALIDE_BUFFER_HELPER_ATTRS
halide_buffer_t *_halide_buffer_crop(void *user_context,
                                     halide_buffer_t *dst,
                                     halide_dimension_t *dst_shape,
                                     const halide_buffer_t *src,
                                     const int *min, const int *extent) {
    *dst = *src;
    dst->dim = dst_shape;
    int64_t offset = 0;
    for (int i = 0; i < dst->dimensions; i++) {
        dst->dim[i] = src->dim[i];
        dst->dim[i].min = min[i];
        dst->dim[i].extent = extent[i];
        offset += (min[i] - src->dim[i].min) * src->dim[i].stride;
    }
    if (dst->host) {
        dst->host += offset * src->type.bytes();
    }
    dst->device_interface = 0;
    dst->device = 0;
    if (src->device_interface) {
        src->device_interface->device_crop(user_context, src, dst);
    }
    return dst;
}


// Called on return from an extern stage where the output buffer was a
// crop of some other larger buffer. This happens for extern stages
// with distinct store_at/compute_at levels. Each call to the stage
// only fills in part of the buffer.
HALIDE_BUFFER_HELPER_ATTRS
int _halide_buffer_retire_crop_after_extern_stage(void *user_context,
                                                   void *obj) {
    halide_buffer_t **buffers = (halide_buffer_t **)obj;
    halide_buffer_t *crop = buffers[0];
    halide_buffer_t *parent = buffers[1];

    if (crop->device) {
        if (!parent->device) {
            // We have been given a device allocation by the extern
            // stage. It only represents the cropped region, so we
            // can't just give it to the parent.
            if (crop->device_dirty()) {
                crop->device_interface->copy_to_host(user_context, crop);
            }
            crop->device_interface->device_free(user_context, crop);
        } else {
            // We are a crop of an existing device allocation.
            if (crop->device_dirty()) {
                parent->set_device_dirty();
            }
            crop->device_interface->device_release_crop(user_context, crop);
        }
    }
    if (crop->host_dirty()) {
        parent->set_host_dirty();
    }
    return 0;
}

HALIDE_BUFFER_HELPER_ATTRS
int _halide_buffer_retire_crops_after_extern_stage(void *user_context,
                                                    void *obj) {
    halide_buffer_t **buffers = (halide_buffer_t **)obj;
    while (*buffers) {
        _halide_buffer_retire_crop_after_extern_stage(user_context, buffers);
        buffers += 2;
    }
    return 0;
}

HALIDE_BUFFER_HELPER_ATTRS
halide_buffer_t *_halide_buffer_set_bounds(halide_buffer_t *buf,
                                           int dim, int min, int extent) {
    buf->dim[dim].min = min;
    buf->dim[dim].extent = extent;
    return buf;
}

}

#undef HALIDE_BUFFER_HELPER_ATTRS


// ll suffix in OpenCL is reserved for 128-bit integers.
#if defined __OPENCL_VERSION__
#define ADD_INT64_T_SUFFIX(x) x##l
#define ADD_UINT64_T_SUFFIX(x) x##ul
// HLSL doesn't have any suffixes.
#elif defined HLSL_VERSION
#define ADD_INT64_T_SUFFIX(x) x
#define ADD_UINT64_T_SUFFIX(x) x
#else
#define ADD_INT64_T_SUFFIX(x) x##ll
#define ADD_UINT64_T_SUFFIX(x) x##ull
#endif

#ifndef HALIDE_MUST_USE_RESULT
#ifdef __has_attribute
#if __has_attribute(nodiscard)
#define HALIDE_MUST_USE_RESULT [[nodiscard]]
#elif __has_attribute(warn_unused_result)
#define HALIDE_MUST_USE_RESULT __attribute__((warn_unused_result))
#else
#define HALIDE_MUST_USE_RESULT
#endif
#else
#define HALIDE_MUST_USE_RESULT
#endif
#endif

#ifndef HALIDE_FUNCTION_ATTRS
#define HALIDE_FUNCTION_ATTRS
#endif

// EmitOneAPIFunc MARKER 
#include <CL/sycl.hpp>
#include <sycl/ext/intel/fpga_extensions.hpp>
#include "dpc_common.hpp"
#include "pipe_array.hpp"
using namespace sycl;
void halide_device_and_host_free_as_destructor(void *user_context, void *obj) {
}
struct device_handle {
    // Important: order these to avoid any padding between fields;
    // some Win32 compiler optimizer configurations can inconsistently
    // insert padding otherwise.
    uint64_t offset;
    void* mem;
};
typedef union {
bool __attribute__ ((aligned(4))) s[4];
struct {bool s0,  s1,  s2,  s3;};
} bool4;

// CodeGen_OneAPI DefineVectorStructTypes 
using _aLoader_channel = sycl::ext::intel::pipe<class _aLoader_channelPipe, float4, 128>;
typedef struct { float4 s[4]; } _aFeeder_channel_array_t;
using _aFeeder_channel = sycl::ext::intel::pipe<class _aFeeder_channelPipe, _aFeeder_channel_array_t, 128>;
using _bLoader_channel = sycl::ext::intel::pipe<class _bLoader_channelPipe, float4, 128>;
typedef struct { float4 s[4]; } _bFeeder_channel_array_t;
using _bFeeder_channel = sycl::ext::intel::pipe<class _bFeeder_channelPipe, _bFeeder_channel_array_t, 128>;
typedef struct { float s[4][4]; } _Out_channel_array_t;
using _Out_channel = sycl::ext::intel::pipe<class _Out_channelPipe, _Out_channel_array_t, 1024>;
typedef struct { float s[4]; } _drainer_channel_array_t;
using _drainer_channel = sycl::ext::intel::pipe<class _drainer_channelPipe, _drainer_channel_array_t, 128>;
using _collector_channel = sycl::ext::intel::pipe<class _collector_channelPipe, float4, 128>;

// CodeGen_OneAPI DeclareChannels 
double gemm(const sycl::device_selector &deviceSelector, struct halide_buffer_t *_A_buffer, struct halide_buffer_t *_B_buffer, struct halide_buffer_t *_deserializer_buffer) {
  std::vector<sycl::event> oneapi_kernel_events;
  std::cout << "// creating device queues\n";
  sycl::queue q_host( sycl::host_selector{}, dpc_common::exception_handler, sycl::property::queue::enable_profiling());
  sycl::queue q_device(deviceSelector, dpc_common::exception_handler, sycl::property::queue::enable_profiling() );
  std::cout << "// Host: " << q_host.get_device().get_info< sycl::info::device::name>() << "\n";
  std::cout << "// Device: " << q_device.get_device().get_info< sycl::info::device::name>() << "\n";
  sycl::device dev = q_device.get_device();
  void * const _ucon = nullptr;
  void *_0 = _halide_buffer_get_host(_A_buffer);
  void * _A = _0;
  uint32_t _1 = _halide_buffer_get_type(_A_buffer);
  int32_t _2 = _halide_buffer_get_dimensions(_A_buffer);
  int32_t _3 = _halide_buffer_get_min(_A_buffer, 0);
  int32_t _4 = _halide_buffer_get_extent(_A_buffer, 0);
  int32_t _5 = _halide_buffer_get_stride(_A_buffer, 0);
  int32_t _6 = _halide_buffer_get_min(_A_buffer, 1);
  int32_t _7 = _halide_buffer_get_extent(_A_buffer, 1);
  int32_t _8 = _halide_buffer_get_stride(_A_buffer, 1);
  void *_9 = _halide_buffer_get_host(_B_buffer);
  void * _B = _9;
  uint32_t _10 = _halide_buffer_get_type(_B_buffer);
  int32_t _11 = _halide_buffer_get_dimensions(_B_buffer);
  int32_t _12 = _halide_buffer_get_min(_B_buffer, 0);
  int32_t _13 = _halide_buffer_get_extent(_B_buffer, 0);
  int32_t _14 = _halide_buffer_get_stride(_B_buffer, 0);
  int32_t _15 = _halide_buffer_get_min(_B_buffer, 1);
  int32_t _16 = _halide_buffer_get_extent(_B_buffer, 1);
  int32_t _17 = _halide_buffer_get_stride(_B_buffer, 1);
  void *_18 = _halide_buffer_get_host(_deserializer_buffer);
  void * _deserializer = _18;
  uint32_t _19 = _halide_buffer_get_type(_deserializer_buffer);
  int32_t _20 = _halide_buffer_get_dimensions(_deserializer_buffer);
  int32_t _21 = _halide_buffer_get_min(_deserializer_buffer, 0);
  int32_t _22 = _halide_buffer_get_extent(_deserializer_buffer, 0);
  int32_t _23 = _halide_buffer_get_stride(_deserializer_buffer, 0);
  int32_t _24 = _halide_buffer_get_min(_deserializer_buffer, 1);
  int32_t _25 = _halide_buffer_get_extent(_deserializer_buffer, 1);
  int32_t _26 = _halide_buffer_get_stride(_deserializer_buffer, 1);
  int32_t _27 = _halide_buffer_get_min(_deserializer_buffer, 2);
  int32_t _28 = _halide_buffer_get_extent(_deserializer_buffer, 2);
  int32_t _29 = _halide_buffer_get_stride(_deserializer_buffer, 2);
  int32_t _30 = _halide_buffer_get_min(_deserializer_buffer, 3);
  int32_t _31 = _halide_buffer_get_extent(_deserializer_buffer, 3);
  int32_t _32 = _halide_buffer_get_stride(_deserializer_buffer, 3);
  int32_t _33 = _halide_buffer_get_min(_deserializer_buffer, 4);
  int32_t _34 = _halide_buffer_get_extent(_deserializer_buffer, 4);
  int32_t _35 = _halide_buffer_get_stride(_deserializer_buffer, 4);
  int32_t _36 = _halide_buffer_get_min(_deserializer_buffer, 5);
  int32_t _37 = _halide_buffer_get_extent(_deserializer_buffer, 5);
  int32_t _38 = _halide_buffer_get_stride(_deserializer_buffer, 5);
  bool _39 = _halide_buffer_is_bounds_query(_A_buffer);
  if (_39)
  {
   struct halide_dimension_t *_40 = _halide_buffer_get_shape(_A_buffer);
   uint64_t _41 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
   void *_42 = (void *)(_41);
   struct halide_device_interface_t *_43 = (struct halide_device_interface_t *)(_41);
   struct halide_dimension_t s0[2] = {
    {_3, _4, 1, 0},
    {_6, _7, _4, 0},
   };
   struct halide_dimension_t *_44 = s0;
   struct halide_buffer_t *_45 = _halide_buffer_init(_A_buffer, _40, _42, _41, _43, 2, 32, 2, _44, _41);
   (void)_45;
  } // if _39
  bool _46 = _halide_buffer_is_bounds_query(_B_buffer);
  if (_46)
  {
   struct halide_dimension_t *_47 = _halide_buffer_get_shape(_B_buffer);
   uint64_t _48 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
   void *_49 = (void *)(_48);
   struct halide_device_interface_t *_50 = (struct halide_device_interface_t *)(_48);
   struct halide_dimension_t s1[2] = {
    {_12, _13, 1, 0},
    {_15, _16, _13, 0},
   };
   struct halide_dimension_t *_51 = s1;
   struct halide_buffer_t *_52 = _halide_buffer_init(_B_buffer, _47, _49, _48, _50, 2, 32, 2, _51, _48);
   (void)_52;
  } // if _46
  bool _53 = _halide_buffer_is_bounds_query(_deserializer_buffer);
  if (_53)
  {
   struct halide_dimension_t *_54 = _halide_buffer_get_shape(_deserializer_buffer);
   uint64_t _55 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
   void *_56 = (void *)(_55);
   struct halide_device_interface_t *_57 = (struct halide_device_interface_t *)(_55);
   int32_t _58 = _13 >> 4;
   int32_t _59 = _7 >> 4;
   int32_t _60 = _58 * 256;
   struct halide_dimension_t s2[6] = {
    {0, 4, 1, 0},
    {0, 4, 4, 0},
    {0, 4, 16, 0},
    {0, 4, 64, 0},
    {0, _58, 256, 0},
    {0, _59, _60, 0},
   };
   struct halide_dimension_t *_61 = s2;
   struct halide_buffer_t *_62 = _halide_buffer_init(_deserializer_buffer, _54, _56, _55, _57, 2, 32, 6, _61, _55);
   (void)_62;
  } // if _53
  bool _63 = _halide_buffer_is_bounds_query(_deserializer_buffer);
  bool _64 = _halide_buffer_is_bounds_query(_A_buffer);
  bool _65 = _halide_buffer_is_bounds_query(_B_buffer);
  bool _66 = _64 || _65;
  bool _67 = _63 || _66;
  bool _68 = !(_67);
  if (_68)
  {
   int64_t _69 = (int64_t)(_7);
   int64_t _70 = (int64_t)(_4);
   int64_t _71 = _69 * _70;
   int64_t _72 = (int64_t)(_16);
   int64_t _73 = (int64_t)(_13);
   int64_t _74 = _72 * _73;
   int64_t _75 = (int64_t)(_25);
   int64_t _76 = (int64_t)(_22);
   int64_t _77 = _75 * _76;
   int64_t _78 = (int64_t)(_28);
   int64_t _79 = _77 * _78;
   int64_t _80 = (int64_t)(_31);
   int64_t _81 = _79 * _80;
   int64_t _82 = (int64_t)(_34);
   int64_t _83 = _81 * _82;
   int64_t _84 = (int64_t)(_37);
   int64_t _85 = _83 * _84;
   halide_buffer_t b2;
   struct halide_buffer_t *_86 = &b2;
   int32_t _87 = _4 >> 4;
   int32_t _88 = _87 * 256;
   int32_t _89 = _7 >> 4;
   struct halide_dimension_t s3[9] = {
    {0, 4, 1, 0},
    {0, 1, 4, 0},
    {0, 4, 4, 0},
    {0, 1, 16, 0},
    {0, 4, 16, 0},
    {0, 4, 64, 0},
    {0, _87, 256, 0},
    {0, 1, _88, 0},
    {0, _89, _88, 0},
   };
   struct halide_dimension_t *_90 = s3;
   uint64_t _91 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
   void *_92 = (void *)(_91);
   struct halide_device_interface_t *_93 = (struct halide_device_interface_t *)(_91);
   struct halide_dimension_t s4[9] = {
    {0, 4, 1, 0},
    {0, 1, 4, 0},
    {0, 4, 4, 0},
    {0, 1, 16, 0},
    {0, 4, 16, 0},
    {0, 4, 64, 0},
    {0, _87, 256, 0},
    {0, 1, _88, 0},
    {0, _89, _88, 0},
   };
   struct halide_dimension_t *_94 = s4;
   struct halide_buffer_t *_95 = _halide_buffer_init(_86, _90, _92, _91, _93, 2, 32, 9, _94, _91);
   struct halide_buffer_t * _A_serializer_mem_channel_buffer = _95;
   struct halide_device_interface_t const *_96 = NULL /* halide_oneapi_device_interface() replaced */;
   int32_t _97 = 0; // halide_device_and_host_malloc(_A_serializer_mem_channel_buffer, _96) replaced with line(s) below 
   if( !_A_serializer_mem_channel_buffer->device ){ // device malloc
     std::cout << "//	 device malloc _A_serializer_mem_channel_buffer\n";
     assert(_A_serializer_mem_channel_buffer->size_in_bytes() != 0);
     uint64_t lowest_index = 0;
     uint64_t highest_index = 0;
     for (int i = 0; i < _A_serializer_mem_channel_buffer->dimensions; i++) {
         if (_A_serializer_mem_channel_buffer->dim[i].stride < 0) {
             lowest_index += (uint64_t)(_A_serializer_mem_channel_buffer->dim[i].stride) * (_A_serializer_mem_channel_buffer->dim[i].extent - 1);
         }
         if (_A_serializer_mem_channel_buffer->dim[i].stride > 0) {
             highest_index += (uint64_t)(_A_serializer_mem_channel_buffer->dim[i].stride) * (_A_serializer_mem_channel_buffer->dim[i].extent - 1);
         }
     }
     device_handle *dev_handle = (device_handle *)std::malloc(sizeof(device_handle));
     dev_handle->mem = (void*)sycl::malloc_device(_A_serializer_mem_channel_buffer->size_in_bytes(), q_device);
     dev_handle->offset = 0;
     _A_serializer_mem_channel_buffer->device = (uint64_t)dev_handle;
   };
   { // host malloc
     std::cout << "//\t host malloc _A_serializer_mem_channel_buffer\n";
     assert(_A_serializer_mem_channel_buffer->size_in_bytes() != 0);
     _A_serializer_mem_channel_buffer->host = (uint8_t*)std::malloc(_A_serializer_mem_channel_buffer->size_in_bytes() );
     assert(_A_serializer_mem_channel_buffer->host != NULL);
   };
   struct s5 { void * const ucon; void * const arg; s5(void *ucon, void *a) : ucon(ucon), arg((void *)a) {} ~s5() { halide_device_and_host_free_as_destructor(ucon, arg); } } d0(_ucon, _A_serializer_mem_channel_buffer);
   void *_98 = 0;
   (void)_98;
   {
    void *_99 = _halide_buffer_get_host(_A_serializer_mem_channel_buffer);
    float *_A_serializer_mem_channel = (float *)(_99);
    if (!_A_serializer_mem_channel)
    {
     std::cout << "Condition '_A_serializer_mem_channel' failed with error id_msg: None\n";
     assert(false);
    }
    // produce A_serializer
    {
     int32_t _addr_temp;
     _addr_temp = 0;
     int32_t _100 = 0; // halide_copy_to_host(_A_buffer) replaced with line(s) below 
     { // memcpy 
       bool from_host = (_A_buffer->device == 0) || (_A_buffer->host_dirty() && _A_buffer->host != NULL);
       bool to_host = 1;
       if (!from_host && to_host) {
         std::cout << "//	 memcpy device->host _A_buffer\n";
         q_device.submit([&](handler& h){ h.memcpy((void *)_A_buffer->host, (void *)(((device_handle*)_A_buffer->device)->mem), _A_buffer->size_in_bytes() ); }).wait();
       } else if (from_host && !to_host) {
         std::cout << "//	 memcpy host->device _A_buffer\n";
         q_device.submit([&](handler& h){ h.memcpy((void *)(((device_handle*)_A_buffer->device)->mem), (void *)_A_buffer->host, _A_buffer->size_in_bytes() ); }).wait();
       } else if (!from_host && !to_host) {
         std::cout << "//	 memcpy device->device not implemented yet\n";
         assert(false);
       } else {
         std::cout << "//	 memcpy _A_buffer Do nothing.\n";
       }
     };
     // kernel_A_serializer
     std::cout << "// kernel kernel_A_serializer\n";
     float *_A = (float*)(_A_buffer->host);
     _A_serializer_mem_channel = (float*)(_A_serializer_mem_channel_buffer->host);
     {
      int _101 = _7 >> 4;
      for (int _A_serializer_s0_i = 0; _A_serializer_s0_i < 0 + _101; _A_serializer_s0_i++)
      {
       int _102 = _4 >> 4;
       for (int _A_serializer_s0_k = 0; _A_serializer_s0_k < 0 + _102; _A_serializer_s0_k++)
       {
        for (int _A_serializer_s0_kk_ii_iii_kkk = 0; _A_serializer_s0_kk_ii_iii_kkk < 0 + 256; _A_serializer_s0_kk_ii_iii_kkk++)
        {
         int _103 = _A_serializer_s0_i * 16;
         int _104 = _A_serializer_s0_kk_ii_iii_kkk & 15;
         int _105 = _104 >> 2;
         int _106 = _A_serializer_s0_kk_ii_iii_kkk & 63;
         int _107 = _106 >> 4;
         int _108 = _107 * 4;
         int _109 = _105 + _108;
         int _110 = _103 + _109;
         int _111 = _110 * _8;
         int _112 = _A_serializer_s0_k * 16;
         int _113 = _A_serializer_s0_kk_ii_iii_kkk >> 6;
         int _114 = _113 * 4;
         int _115 = _A_serializer_s0_kk_ii_iii_kkk & 3;
         int _116 = _114 + _115;
         int _117 = _112 + _116;
         int _118 = _111 + _117;
         int _119 = _6 * _8;
         int _120 = _119 + _3;
         int _121 = _118 - _120;
         float _122 = (( float *)_A)[_121];
         int _123 = _addr_temp;
         _A_serializer_mem_channel[_123] = _122;
         int _124 = _addr_temp;
         int _125 = _124 + 1;
         _addr_temp = _125;
        } // for _A_serializer_s0_kk_ii_iii_kkk
       } // for _A_serializer_s0_k
      } // for _A_serializer_s0_i
     } // // kernel_kernel_A_serializer
     bool _126 = (bool)(ADD_UINT64_T_SUFFIX(1));
     int32_t _127 = _halide_buffer_set_host_dirty(_A_serializer_mem_channel_buffer, _126);
     (void)_127;
    } // alloc _addr_temp
    // consume A_serializer
    // produce aLoader
    struct halide_device_interface_t const *_128 = NULL /* halide_oneapi_device_interface() replaced */;
    int32_t _129 = 0; // halide_copy_to_device(_A_serializer_mem_channel_buffer, _128) replaced with line(s) below 
    { // memcpy 
      bool from_host = (_A_serializer_mem_channel_buffer->device == 0) || (_A_serializer_mem_channel_buffer->host_dirty() && _A_serializer_mem_channel_buffer->host != NULL);
      bool to_host = 0;
      if (!from_host && to_host) {
        std::cout << "//	 memcpy device->host _A_serializer_mem_channel_buffer\n";
        q_device.submit([&](handler& h){ h.memcpy((void *)_A_serializer_mem_channel_buffer->host, (void *)(((device_handle*)_A_serializer_mem_channel_buffer->device)->mem), _A_serializer_mem_channel_buffer->size_in_bytes() ); }).wait();
      } else if (from_host && !to_host) {
        std::cout << "//	 memcpy host->device _A_serializer_mem_channel_buffer\n";
        q_device.submit([&](handler& h){ h.memcpy((void *)(((device_handle*)_A_serializer_mem_channel_buffer->device)->mem), (void *)_A_serializer_mem_channel_buffer->host, _A_serializer_mem_channel_buffer->size_in_bytes() ); }).wait();
      } else if (!from_host && !to_host) {
        std::cout << "//	 memcpy device->device not implemented yet\n";
        assert(false);
      } else {
        std::cout << "//	 memcpy _A_serializer_mem_channel_buffer Do nothing.\n";
      }
    };
    // kernel_aLoader
    std::cout << "// kernel kernel_aLoader\n";
    _A_serializer_mem_channel = (float*)(((device_handle*) _A_serializer_mem_channel_buffer->device)->mem);
    oneapi_kernel_events.push_back( q_device.submit([&](sycl::handler &h){
      h.single_task<class kernel_aLoader_class>([=](){
        int _addr_temp;
        _addr_temp = 0;
        int _130 = _7 >> 4;
        for (int _aLoader_s0_i = 0; _aLoader_s0_i < 0 + _130; _aLoader_s0_i++)
        {
         int _131 = _13 >> 4;
         for (int _aLoader_s0_j = 0; _aLoader_s0_j < 0 + _131; _aLoader_s0_j++)
         {
          int _132 = _4 >> 4;
          for (int _aLoader_s0_k = 0; _aLoader_s0_k < 0 + _132; _aLoader_s0_k++)
          {
           for (int _aLoader_s0_kk_ii_iii = 0; _aLoader_s0_kk_ii_iii < 0 + 64; _aLoader_s0_kk_ii_iii++)
           {
            int _133 = _addr_temp;
            int _134 = _13 >> 4;
            int _135 = _4 >> 4;
            int _136 = _134 * _135;
            int _137 = _136 * 64;
            int _138 = _133 / _137;
            int _139 = _138 * _135;
            int _140 = _139 * 64;
            int _141 = _135 * 64;
            int _142 = _133 % _141;
            int _143 = _140 + _142;
            int _144 = _143 * 4;
            float4 _145;
            _145 = { 
              _A_serializer_mem_channel[_144+0],
              _A_serializer_mem_channel[_144+1],
              _A_serializer_mem_channel[_144+2],
              _A_serializer_mem_channel[_144+3]
            };
            // write_channel_intel(_aLoader_channel, _145);
            _aLoader_channel::write( _145 );
            (void)_145;
            int _146 = _133 + 1;
            _addr_temp = _146;
           } // for _aLoader_s0_kk_ii_iii
          } // for _aLoader_s0_k
         } // for _aLoader_s0_j
        } // for _aLoader_s0_i
      }); //  h.single_task kernel_aLoader_class
    }) ); // q_device.submit
    // consume aLoader
    // produce aFeeder
    // kernel_aFeeder
    std::cout << "// kernel kernel_aFeeder\n";
    oneapi_kernel_events.push_back( q_device.submit([&](sycl::handler &h){
      h.single_task<class kernel_aFeeder_class>([=](){
        _aFeeder_channel_array_t _aFeeder_channel_array;
        float4 _aFeeder_value_shreg;
        uint32_t _aFeeder_time_stamp_shreg;
        float4 _aFeeder_in_v_temp;
        uint _aFeeder_cycle_temp;
        // float4 __attribute__((memory, numbanks(4), singlepump, numwriteports(1), numreadports(1))) _aFeeder_DB_0_ibuffer[2][4][4][4];
        [[intel::fpga_memory(), intel::numbanks(4), intel::singlepump, intel::simple_dual_port]] float4 _aFeeder_DB_0_ibuffer[2][4][4][4];
        #pragma unroll
        for (int _aFeeder_s0_jjj_init = 0; _aFeeder_s0_jjj_init < 0 + 4; _aFeeder_s0_jjj_init++)
        {
         bool _147 = _aFeeder_s0_jjj_init == 0;
         if (_147)
         {
          uint _148 = (uint)(ADD_UINT64_T_SUFFIX(0));
          _aFeeder_cycle_temp = _148;
         } // if _147
        } // for _aFeeder_s0_jjj_init
        int _149 = _4 >> 4;
        int _150 = _7 >> 4;
        int _151 = _13 >> 4;
        int _152 = _150 * _151;
        int _153 = _149 * _152;
        int _154 = _153 * 64;
        int _155 = _154 + 64;
        for (int _aFeeder_s0_outermost_loop = 0; _aFeeder_s0_outermost_loop < 0 + _155; _aFeeder_s0_outermost_loop++)
        {
         uint _156 = _aFeeder_cycle_temp;
         uint _157 = (uint)(ADD_UINT64_T_SUFFIX(6));
         uint _158 = _156 >> _157;
         int _159 = (int)(_158);
         int _160 = _4 >> 4;
         int _161 = _7 >> 4;
         int _162 = _13 >> 4;
         int _163 = _161 * _162;
         int _164 = _160 * _163;
         bool _165 = _159 < _164;
         if (_165)
         {
          // float4 __166 = read_channel_intel(_aLoader_channel);
          float4 __166 = _aLoader_channel::read();
          _aFeeder_in_v_temp = __166;
         } // if _165
         #pragma unroll
         for (int _aFeeder_s0_buf = 0; _aFeeder_s0_buf < 0 + 4; _aFeeder_s0_buf++)
         {
          bool _167 = _aFeeder_s0_buf == 0;
          if (_167)
          {
           float4 _168 = _aFeeder_in_v_temp;
           _aFeeder_value_shreg = _168;
           (void)_168;
           uint _169 = _aFeeder_cycle_temp;
           _aFeeder_time_stamp_shreg = _169;
           (void)_169;
          } // if _167
          else
          {
           float4 _171 = _aFeeder_value_shreg;
           _aFeeder_value_shreg = _171;
           (void)_171;
           uint _173 = _aFeeder_time_stamp_shreg;
           _aFeeder_time_stamp_shreg = _173;
           (void)_173;
          } // if _167 else
          float4 _175 = _aFeeder_value_shreg;
          float4 _176 = (float4){
            sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_175[0])),
            sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_175[1])),
            sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_175[2])),
            sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_175[3]))
          };
          _aFeeder_value_shreg = _176;
          (void)_176;
          uint _178 = _aFeeder_time_stamp_shreg;
          uint _179 = sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_178));
          _aFeeder_time_stamp_shreg = _179;
          (void)_179;
          uint _181 = _aFeeder_time_stamp_shreg;
          uint _182 = (uint)(ADD_UINT64_T_SUFFIX(63));
          uint _183 = _181 & _182;
          uint _184 = (uint)(ADD_UINT64_T_SUFFIX(3));
          uint _185 = _183 & _184;
          int _186 = (int)(_185);
          bool _187 = _aFeeder_s0_buf == _186;
          if (_187)
          {
           float4 _189 = _aFeeder_value_shreg;
           uint _191 = _aFeeder_time_stamp_shreg;
           uint _192 = (uint)(ADD_UINT64_T_SUFFIX(6));
           uint _193 = _191 >> _192;
           uint _194 = (uint)(ADD_UINT64_T_SUFFIX(1));
           uint _195 = _193 & _194;
           bool _196 = (bool)(_195);
           uint _198 = (uint)(ADD_UINT64_T_SUFFIX(63));
           uint _199 = _191 & _198;
           int _200 = (int)(_199);
           int _201 = _200 >> 4;
           int _203 = _200 >> 2;
           int _204 = _203 & 3;
           _aFeeder_DB_0_ibuffer[_196][_201][_204][_aFeeder_s0_buf] = _189;
          } // if _187
          uint _206 = _aFeeder_time_stamp_shreg;
          uint _207 = (uint)(ADD_UINT64_T_SUFFIX(6));
          uint _208 = _206 >> _207;
          int _209 = (int)(_208);
          int _210 = _4 >> 4;
          int _211 = _7 >> 4;
          int _212 = _13 >> 4;
          int _213 = _211 * _212;
          int _214 = _210 * _213;
          bool _215 = _209 <= _214;
          uint _216 = (uint)(ADD_UINT64_T_SUFFIX(0));
          bool _218 = _216 < _208;
          bool _219 = _215 && _218;
          if (_219)
          {
           uint _221 = _aFeeder_time_stamp_shreg;
           uint _222 = (uint)(ADD_UINT64_T_SUFFIX(6));
           uint _223 = _221 >> _222;
           uint _224 = (uint)(ADD_UINT64_T_SUFFIX(1));
           uint _225 = _223 & _224;
           bool _226 = (bool)(_225);
           bool _227 = !(_226);
           uint _229 = (uint)(ADD_UINT64_T_SUFFIX(63));
           uint _230 = _221 & _229;
           int _231 = (int)(_230);
           int _232 = _231 >> 4;
           int _234 = _231 >> 2;
           int _235 = _234 & 3;
           float4 _236 = _aFeeder_DB_0_ibuffer[_227][_232][_235][_aFeeder_s0_buf];
           _aFeeder_channel_array.s[_aFeeder_s0_buf] = _236;
           (void)_aFeeder_s0_buf;
          } // if _219
         } // for _aFeeder_s0_buf
         uint _238 = _aFeeder_time_stamp_shreg;
         uint _239 = (uint)(ADD_UINT64_T_SUFFIX(6));
         uint _240 = _238 >> _239;
         int _241 = (int)(_240);
         int _242 = _4 >> 4;
         int _243 = _7 >> 4;
         int _244 = _13 >> 4;
         int _245 = _243 * _244;
         int _246 = _242 * _245;
         bool _247 = _241 <= _246;
         uint _248 = (uint)(ADD_UINT64_T_SUFFIX(0));
         bool _250 = _248 < _240;
         bool _251 = _247 && _250;
         if (_251)
         {
          // write_channel_intel(_aFeeder_channel, _aFeeder_channel_array);
          _aFeeder_channel::write( _aFeeder_channel_array );
          (void)_aFeeder_channel_array;
         } // if _251
         uint _252 = _aFeeder_cycle_temp;
         uint _253 = (uint)(ADD_UINT64_T_SUFFIX(1));
         uint _254 = _252 + _253;
         _aFeeder_cycle_temp = _254;
        } // for _aFeeder_s0_outermost_loop
      }); //  h.single_task kernel_aFeeder_class
    }) ); // q_device.submit
    // consume aFeeder
    int32_t _255 = _13 >> 4;
    int32_t _256 = _4 >> 4;
    int32_t _257 = _255 * _256;
    halide_buffer_t b3;
    struct halide_buffer_t *_258 = &b3;
    int32_t _259 = _256 * 256;
    int32_t _260 = _257 * 256;
    struct halide_dimension_t s6[9] = {
     {0, 4, 1, 0},
     {0, 4, 4, 0},
     {0, 1, 16, 0},
     {0, 4, 16, 0},
     {0, 1, 64, 0},
     {0, 4, 64, 0},
     {0, _256, 256, 0},
     {0, _255, _259, 0},
     {0, 1, _260, 0},
    };
    struct halide_dimension_t *_261 = s6;
    uint64_t _262 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
    void *_263 = (void *)(_262);
    struct halide_device_interface_t *_264 = (struct halide_device_interface_t *)(_262);
    struct halide_dimension_t s7[9] = {
     {0, 4, 1, 0},
     {0, 4, 4, 0},
     {0, 1, 16, 0},
     {0, 4, 16, 0},
     {0, 1, 64, 0},
     {0, 4, 64, 0},
     {0, _256, 256, 0},
     {0, _255, _259, 0},
     {0, 1, _260, 0},
    };
    struct halide_dimension_t *_265 = s7;
    struct halide_buffer_t *_266 = _halide_buffer_init(_258, _261, _263, _262, _264, 2, 32, 9, _265, _262);
    struct halide_buffer_t * _B_serializer_mem_channel_buffer = _266;
    struct halide_device_interface_t const *_267 = NULL /* halide_oneapi_device_interface() replaced */;
    int32_t _268 = 0; // halide_device_and_host_malloc(_B_serializer_mem_channel_buffer, _267) replaced with line(s) below 
    if( !_B_serializer_mem_channel_buffer->device ){ // device malloc
      std::cout << "//	 device malloc _B_serializer_mem_channel_buffer\n";
      assert(_B_serializer_mem_channel_buffer->size_in_bytes() != 0);
      uint64_t lowest_index = 0;
      uint64_t highest_index = 0;
      for (int i = 0; i < _B_serializer_mem_channel_buffer->dimensions; i++) {
          if (_B_serializer_mem_channel_buffer->dim[i].stride < 0) {
              lowest_index += (uint64_t)(_B_serializer_mem_channel_buffer->dim[i].stride) * (_B_serializer_mem_channel_buffer->dim[i].extent - 1);
          }
          if (_B_serializer_mem_channel_buffer->dim[i].stride > 0) {
              highest_index += (uint64_t)(_B_serializer_mem_channel_buffer->dim[i].stride) * (_B_serializer_mem_channel_buffer->dim[i].extent - 1);
          }
      }
      device_handle *dev_handle = (device_handle *)std::malloc(sizeof(device_handle));
      dev_handle->mem = (void*)sycl::malloc_device(_B_serializer_mem_channel_buffer->size_in_bytes(), q_device);
      dev_handle->offset = 0;
      _B_serializer_mem_channel_buffer->device = (uint64_t)dev_handle;
    };
    { // host malloc
      std::cout << "//\t host malloc _B_serializer_mem_channel_buffer\n";
      assert(_B_serializer_mem_channel_buffer->size_in_bytes() != 0);
      _B_serializer_mem_channel_buffer->host = (uint8_t*)std::malloc(_B_serializer_mem_channel_buffer->size_in_bytes() );
      assert(_B_serializer_mem_channel_buffer->host != NULL);
    };
    struct s8 { void * const ucon; void * const arg; s8(void *ucon, void *a) : ucon(ucon), arg((void *)a) {} ~s8() { halide_device_and_host_free_as_destructor(ucon, arg); } } d1(_ucon, _B_serializer_mem_channel_buffer);
    void *_269 = 0;
    (void)_269;
    {
     void *_270 = _halide_buffer_get_host(_B_serializer_mem_channel_buffer);
     float *_B_serializer_mem_channel = (float *)(_270);
     if (!_B_serializer_mem_channel)
     {
      std::cout << "Condition '_B_serializer_mem_channel' failed with error id_msg: None\n";
      assert(false);
     }
     // produce B_serializer
     {
      int32_t _addr_temp;
      _addr_temp = 0;
      int32_t _271 = 0; // halide_copy_to_host(_B_buffer) replaced with line(s) below 
      { // memcpy 
        bool from_host = (_B_buffer->device == 0) || (_B_buffer->host_dirty() && _B_buffer->host != NULL);
        bool to_host = 1;
        if (!from_host && to_host) {
          std::cout << "//	 memcpy device->host _B_buffer\n";
          q_device.submit([&](handler& h){ h.memcpy((void *)_B_buffer->host, (void *)(((device_handle*)_B_buffer->device)->mem), _B_buffer->size_in_bytes() ); }).wait();
        } else if (from_host && !to_host) {
          std::cout << "//	 memcpy host->device _B_buffer\n";
          q_device.submit([&](handler& h){ h.memcpy((void *)(((device_handle*)_B_buffer->device)->mem), (void *)_B_buffer->host, _B_buffer->size_in_bytes() ); }).wait();
        } else if (!from_host && !to_host) {
          std::cout << "//	 memcpy device->device not implemented yet\n";
          assert(false);
        } else {
          std::cout << "//	 memcpy _B_buffer Do nothing.\n";
        }
      };
      // kernel_B_serializer
      std::cout << "// kernel kernel_B_serializer\n";
      float *_B = (float*)(_B_buffer->host);
      _B_serializer_mem_channel = (float*)(_B_serializer_mem_channel_buffer->host);
      {
       int _272 = _13 >> 4;
       for (int _B_serializer_s0_j = 0; _B_serializer_s0_j < 0 + _272; _B_serializer_s0_j++)
       {
        int _273 = _4 >> 4;
        for (int _B_serializer_s0_k = 0; _B_serializer_s0_k < 0 + _273; _B_serializer_s0_k++)
        {
         for (int _B_serializer_s0_kk_jj_jjj = 0; _B_serializer_s0_kk_jj_jjj < 0 + 64; _B_serializer_s0_kk_jj_jjj++)
         {
          int _274 = _B_serializer_s0_kk_jj_jjj >> 4;
          int _275 = _B_serializer_s0_k * 4;
          int _276 = _274 + _275;
          int _277 = _276 * _17;
          int _278 = _277 * 4;
          int _279 = _B_serializer_s0_j * 16;
          int _280 = _B_serializer_s0_kk_jj_jjj & 15;
          int _281 = _280 >> 2;
          int _282 = _281 * 4;
          int _283 = _B_serializer_s0_kk_jj_jjj & 3;
          int _284 = _282 + _283;
          int _285 = _279 + _284;
          int _286 = _278 + _285;
          int _287 = _15 * _17;
          int _288 = _287 + _12;
          int _289 = _286 - _288;
          int4 _290 = _289 + _17 * (int4){0, 1, 2, 3};
          float4 _V0;
          _V0[0] = (( float*)_B)[_290[0]];
          _V0[1] = (( float*)_B)[_290[1]];
          _V0[2] = (( float*)_B)[_290[2]];
          _V0[3] = (( float*)_B)[_290[3]];
          int _291 = _addr_temp;
          int _292 = _291 * 4;
          _B_serializer_mem_channel[_292+0] = _V0[0];
          _B_serializer_mem_channel[_292+1] = _V0[1];
          _B_serializer_mem_channel[_292+2] = _V0[2];
          _B_serializer_mem_channel[_292+3] = _V0[3];
          int _293 = _addr_temp;
          int _294 = _293 + 1;
          _addr_temp = _294;
         } // for _B_serializer_s0_kk_jj_jjj
        } // for _B_serializer_s0_k
       } // for _B_serializer_s0_j
      } // // kernel_kernel_B_serializer
      bool _295 = (bool)(ADD_UINT64_T_SUFFIX(1));
      int32_t _296 = _halide_buffer_set_host_dirty(_B_serializer_mem_channel_buffer, _295);
      (void)_296;
     } // alloc _addr_temp
     // consume B_serializer
     // produce bLoader
     struct halide_device_interface_t const *_297 = NULL /* halide_oneapi_device_interface() replaced */;
     int32_t _298 = 0; // halide_copy_to_device(_B_serializer_mem_channel_buffer, _297) replaced with line(s) below 
     { // memcpy 
       bool from_host = (_B_serializer_mem_channel_buffer->device == 0) || (_B_serializer_mem_channel_buffer->host_dirty() && _B_serializer_mem_channel_buffer->host != NULL);
       bool to_host = 0;
       if (!from_host && to_host) {
         std::cout << "//	 memcpy device->host _B_serializer_mem_channel_buffer\n";
         q_device.submit([&](handler& h){ h.memcpy((void *)_B_serializer_mem_channel_buffer->host, (void *)(((device_handle*)_B_serializer_mem_channel_buffer->device)->mem), _B_serializer_mem_channel_buffer->size_in_bytes() ); }).wait();
       } else if (from_host && !to_host) {
         std::cout << "//	 memcpy host->device _B_serializer_mem_channel_buffer\n";
         q_device.submit([&](handler& h){ h.memcpy((void *)(((device_handle*)_B_serializer_mem_channel_buffer->device)->mem), (void *)_B_serializer_mem_channel_buffer->host, _B_serializer_mem_channel_buffer->size_in_bytes() ); }).wait();
       } else if (!from_host && !to_host) {
         std::cout << "//	 memcpy device->device not implemented yet\n";
         assert(false);
       } else {
         std::cout << "//	 memcpy _B_serializer_mem_channel_buffer Do nothing.\n";
       }
     };
     // kernel_bLoader
     std::cout << "// kernel kernel_bLoader\n";
     _B_serializer_mem_channel = (float*)(((device_handle*) _B_serializer_mem_channel_buffer->device)->mem);
     oneapi_kernel_events.push_back( q_device.submit([&](sycl::handler &h){
       h.single_task<class kernel_bLoader_class>([=](){
         int _addr_temp;
         _addr_temp = 0;
         int _299 = _7 >> 4;
         for (int _bLoader_s0_i = 0; _bLoader_s0_i < 0 + _299; _bLoader_s0_i++)
         {
          int _300 = _13 >> 4;
          for (int _bLoader_s0_j = 0; _bLoader_s0_j < 0 + _300; _bLoader_s0_j++)
          {
           int _301 = _4 >> 4;
           for (int _bLoader_s0_k = 0; _bLoader_s0_k < 0 + _301; _bLoader_s0_k++)
           {
            for (int _bLoader_s0_kk_jj_jjj = 0; _bLoader_s0_kk_jj_jjj < 0 + 64; _bLoader_s0_kk_jj_jjj++)
            {
             int _302 = _addr_temp;
             int _303 = _13 >> 4;
             int _304 = _4 >> 4;
             int _305 = _303 * _304;
             int _306 = _305 * 64;
             int _307 = _302 % _306;
             int _308 = _307 * 4;
             float4 _309;
             _309 = { 
               _B_serializer_mem_channel[_308+0],
               _B_serializer_mem_channel[_308+1],
               _B_serializer_mem_channel[_308+2],
               _B_serializer_mem_channel[_308+3]
             };
             // write_channel_intel(_bLoader_channel, _309);
             _bLoader_channel::write( _309 );
             (void)_309;
             int _310 = _302 + 1;
             _addr_temp = _310;
            } // for _bLoader_s0_kk_jj_jjj
           } // for _bLoader_s0_k
          } // for _bLoader_s0_j
         } // for _bLoader_s0_i
       }); //  h.single_task kernel_bLoader_class
     }) ); // q_device.submit
     // consume bLoader
     // produce bFeeder
     // kernel_bFeeder
     std::cout << "// kernel kernel_bFeeder\n";
     oneapi_kernel_events.push_back( q_device.submit([&](sycl::handler &h){
       h.single_task<class kernel_bFeeder_class>([=](){
         _bFeeder_channel_array_t _bFeeder_channel_array;
         float4 _bFeeder_value_shreg;
         uint32_t _bFeeder_time_stamp_shreg;
         float4 _bFeeder_in_v_temp;
         uint _bFeeder_cycle_temp;
         // float4 __attribute__((memory, numbanks(4), singlepump, numwriteports(1), numreadports(1))) _bFeeder_DB_0_ibuffer[2][4][4][4];
         [[intel::fpga_memory(), intel::numbanks(4), intel::singlepump, intel::simple_dual_port]] float4 _bFeeder_DB_0_ibuffer[2][4][4][4];
         #pragma unroll
         for (int _bFeeder_s0_iii_init = 0; _bFeeder_s0_iii_init < 0 + 4; _bFeeder_s0_iii_init++)
         {
          bool _311 = _bFeeder_s0_iii_init == 0;
          if (_311)
          {
           uint _312 = (uint)(ADD_UINT64_T_SUFFIX(0));
           _bFeeder_cycle_temp = _312;
          } // if _311
         } // for _bFeeder_s0_iii_init
         int _313 = _4 >> 4;
         int _314 = _7 >> 4;
         int _315 = _13 >> 4;
         int _316 = _314 * _315;
         int _317 = _313 * _316;
         int _318 = _317 * 64;
         int _319 = _318 + 64;
         for (int _bFeeder_s0_outermost_loop = 0; _bFeeder_s0_outermost_loop < 0 + _319; _bFeeder_s0_outermost_loop++)
         {
          uint _320 = _bFeeder_cycle_temp;
          uint _321 = (uint)(ADD_UINT64_T_SUFFIX(6));
          uint _322 = _320 >> _321;
          int _323 = (int)(_322);
          int _324 = _4 >> 4;
          int _325 = _7 >> 4;
          int _326 = _13 >> 4;
          int _327 = _325 * _326;
          int _328 = _324 * _327;
          bool _329 = _323 < _328;
          if (_329)
          {
           // float4 __330 = read_channel_intel(_bLoader_channel);
           float4 __330 = _bLoader_channel::read();
           _bFeeder_in_v_temp = __330;
          } // if _329
          #pragma unroll
          for (int _bFeeder_s0_buf = 0; _bFeeder_s0_buf < 0 + 4; _bFeeder_s0_buf++)
          {
           bool _331 = _bFeeder_s0_buf == 0;
           if (_331)
           {
            float4 _332 = _bFeeder_in_v_temp;
            _bFeeder_value_shreg = _332;
            (void)_332;
            uint _333 = _bFeeder_cycle_temp;
            _bFeeder_time_stamp_shreg = _333;
            (void)_333;
           } // if _331
           else
           {
            float4 _335 = _bFeeder_value_shreg;
            _bFeeder_value_shreg = _335;
            (void)_335;
            uint _337 = _bFeeder_time_stamp_shreg;
            _bFeeder_time_stamp_shreg = _337;
            (void)_337;
           } // if _331 else
           float4 _339 = _bFeeder_value_shreg;
           float4 _340 = (float4){
             sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_339[0])),
             sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_339[1])),
             sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_339[2])),
             sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_339[3]))
           };
           _bFeeder_value_shreg = _340;
           (void)_340;
           uint _342 = _bFeeder_time_stamp_shreg;
           uint _343 = sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_342));
           _bFeeder_time_stamp_shreg = _343;
           (void)_343;
           uint _345 = _bFeeder_time_stamp_shreg;
           uint _346 = (uint)(ADD_UINT64_T_SUFFIX(63));
           uint _347 = _345 & _346;
           uint _348 = (uint)(ADD_UINT64_T_SUFFIX(3));
           uint _349 = _347 & _348;
           int _350 = (int)(_349);
           bool _351 = _bFeeder_s0_buf == _350;
           if (_351)
           {
            float4 _353 = _bFeeder_value_shreg;
            uint _355 = _bFeeder_time_stamp_shreg;
            uint _356 = (uint)(ADD_UINT64_T_SUFFIX(6));
            uint _357 = _355 >> _356;
            uint _358 = (uint)(ADD_UINT64_T_SUFFIX(1));
            uint _359 = _357 & _358;
            bool _360 = (bool)(_359);
            uint _362 = (uint)(ADD_UINT64_T_SUFFIX(63));
            uint _363 = _355 & _362;
            int _364 = (int)(_363);
            int _365 = _364 >> 4;
            int _367 = _364 >> 2;
            int _368 = _367 & 3;
            _bFeeder_DB_0_ibuffer[_360][_365][_368][_bFeeder_s0_buf] = _353;
           } // if _351
           uint _370 = _bFeeder_time_stamp_shreg;
           uint _371 = (uint)(ADD_UINT64_T_SUFFIX(6));
           uint _372 = _370 >> _371;
           int _373 = (int)(_372);
           int _374 = _4 >> 4;
           int _375 = _7 >> 4;
           int _376 = _13 >> 4;
           int _377 = _375 * _376;
           int _378 = _374 * _377;
           bool _379 = _373 <= _378;
           uint _380 = (uint)(ADD_UINT64_T_SUFFIX(0));
           bool _382 = _380 < _372;
           bool _383 = _379 && _382;
           if (_383)
           {
            uint _385 = _bFeeder_time_stamp_shreg;
            uint _386 = (uint)(ADD_UINT64_T_SUFFIX(6));
            uint _387 = _385 >> _386;
            uint _388 = (uint)(ADD_UINT64_T_SUFFIX(1));
            uint _389 = _387 & _388;
            bool _390 = (bool)(_389);
            bool _391 = !(_390);
            uint _393 = (uint)(ADD_UINT64_T_SUFFIX(63));
            uint _394 = _385 & _393;
            int _395 = (int)(_394);
            int _396 = _395 >> 4;
            int _398 = _395 & 3;
            float4 _399 = _bFeeder_DB_0_ibuffer[_391][_396][_398][_bFeeder_s0_buf];
            _bFeeder_channel_array.s[_bFeeder_s0_buf] = _399;
            (void)_bFeeder_s0_buf;
           } // if _383
          } // for _bFeeder_s0_buf
          uint _401 = _bFeeder_time_stamp_shreg;
          uint _402 = (uint)(ADD_UINT64_T_SUFFIX(6));
          uint _403 = _401 >> _402;
          int _404 = (int)(_403);
          int _405 = _4 >> 4;
          int _406 = _7 >> 4;
          int _407 = _13 >> 4;
          int _408 = _406 * _407;
          int _409 = _405 * _408;
          bool _410 = _404 <= _409;
          uint _411 = (uint)(ADD_UINT64_T_SUFFIX(0));
          bool _413 = _411 < _403;
          bool _414 = _410 && _413;
          if (_414)
          {
           // write_channel_intel(_bFeeder_channel, _bFeeder_channel_array);
           _bFeeder_channel::write( _bFeeder_channel_array );
           (void)_bFeeder_channel_array;
          } // if _414
          uint _415 = _bFeeder_cycle_temp;
          uint _416 = (uint)(ADD_UINT64_T_SUFFIX(1));
          uint _417 = _415 + _416;
          _bFeeder_cycle_temp = _417;
         } // for _bFeeder_s0_outermost_loop
       }); //  h.single_task kernel_bFeeder_class
     }) ); // q_device.submit
     // consume bFeeder
     // produce Out
     // kernel_Out
     std::cout << "// kernel kernel_Out\n";
     oneapi_kernel_events.push_back( q_device.submit([&](sycl::handler &h){
       h.single_task<class kernel_Out_class>([=](){
         _bFeeder_channel_array_t _bFeeder_channel_array;
         _aFeeder_channel_array_t _aFeeder_channel_array;
         _Out_channel_array_t _Out_channel_array;
         // produce Z
         float _Z_shreg[16][4][4];
         // produce Y
         float4 _Y_shreg[4];
         float _Z_temp[4][4];
         // produce X
         float4 _X_shreg[4];
         float _Z_shreg_temp;
         int _418 = _7 >> 4;
         for (int _X_s0_i = 0; _X_s0_i < 0 + _418; _X_s0_i++)
         {
          int _419 = _13 >> 4;
          for (int _X_s0_j = 0; _X_s0_j < 0 + _419; _X_s0_j++)
          {
           int _420 = _4 >> 4;
           for (int _X_s0_k = 0; _X_s0_k < 0 + _420; _X_s0_k++)
           {
            for (int _X_s0_kk_ii_jj = 0; _X_s0_kk_ii_jj < 0 + 64; _X_s0_kk_ii_jj++)
            {
             #pragma unroll
             for (int _dummy__1_s0_iii = 0; _dummy__1_s0_iii < 0 + 4; _dummy__1_s0_iii++)
             {
              #pragma unroll
              for (int _dummy_s0_jjj = 0; _dummy_s0_jjj < 0 + 4; _dummy_s0_jjj++)
              {
               float _422 = _Z_shreg[15][_dummy_s0_jjj][_dummy__1_s0_iii];
               _Z_temp[_dummy_s0_jjj][_dummy__1_s0_iii] = _422;
               #pragma unroll
               for (int _dummy__2_s0_l1 = 0; _dummy__2_s0_l1 < 0 + 15; _dummy__2_s0_l1++)
               {
                int _423 = 15 - _dummy__2_s0_l1;
                int _424 = 14 - _dummy__2_s0_l1;
                float _426 = _Z_shreg[_424][_dummy_s0_jjj][_dummy__1_s0_iii];
                _Z_shreg[_423][_dummy_s0_jjj][_dummy__1_s0_iii] = _426;
                (void)_426;
               } // for _dummy__2_s0_l1
               float _427 = _Z_temp[_dummy_s0_jjj][_dummy__1_s0_iii];
               _Z_shreg[0][_dummy_s0_jjj][_dummy__1_s0_iii] = _427;
               (void)_427;
              } // for _dummy_s0_jjj
             } // for _dummy__1_s0_iii
             bool _Out_channel_temp;
             _Out_channel_temp = 0;
             // _bFeeder_channel_array_t __428 = read_channel_intel(_bFeeder_channel);
             _bFeeder_channel_array_t __428 = _bFeeder_channel::read();
             _bFeeder_channel_array = __428;
             (void)__428;
             // _aFeeder_channel_array_t __429 = read_channel_intel(_aFeeder_channel);
             _aFeeder_channel_array_t __429 = _aFeeder_channel::read();
             _aFeeder_channel_array = __429;
             (void)__429;
             #pragma unroll
             for (int _X_s0_iii = 0; _X_s0_iii < 0 + 4; _X_s0_iii++)
             {
              #pragma unroll
              for (int _X_s0_jjj = 0; _X_s0_jjj < 0 + 4; _X_s0_jjj++)
              {
               float4 _430;
               bool _431 = _X_s0_jjj == 0;
               if (_431)
               {
                float4 __432 = _aFeeder_channel_array.s[_X_s0_iii];
                _430 = __432;
               } // if _431
               else
               {
                float4 _434 = _X_shreg[_X_s0_iii];
                _430 = _434;
               } // if _431 else
               float4 _435 = _430;
               _X_shreg[_X_s0_iii] = _435;
               (void)_435;
               float4 _437 = _X_shreg[_X_s0_iii];
               float4 _438 = (float4){
                 sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_437[0])),
                 sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_437[1])),
                 sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_437[2])),
                 sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_437[3]))
               };
               _X_shreg[_X_s0_iii] = _438;
               (void)_438;
               float4 _439;
               bool _440 = _X_s0_iii == 0;
               if (_440)
               {
                float4 __441 = _bFeeder_channel_array.s[_X_s0_jjj];
                _439 = __441;
               } // if _440
               else
               {
                float4 _443 = _Y_shreg[_X_s0_jjj];
                _439 = _443;
               } // if _440 else
               float4 _444 = _439;
               _Y_shreg[_X_s0_jjj] = _444;
               (void)_444;
               float4 _446 = _Y_shreg[_X_s0_jjj];
               float4 _447 = (float4){
                 sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_446[0])),
                 sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_446[1])),
                 sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_446[2])),
                 sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_446[3]))
               };
               _Y_shreg[_X_s0_jjj] = _447;
               (void)_447;
               float _448;
               bool _449 = _X_s0_k == 0;
               int _450 = _X_s0_kk_ii_jj >> 4;
               bool _451 = _450 == 0;
               bool _452 = _449 && _451;
               if (_452)
               {
                float _453 = float_from_bits(0 /* 0 */);
                _448 = _453;
               } // if _452
               else
               {
                float _455 = _Z_shreg[0][_X_s0_jjj][_X_s0_iii];
                float _456 = sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_455));
                _448 = _456;
               } // if _452 else
               float _457 = _448;
               _Z_shreg_temp = _457;
               #pragma unroll
               for (int _X_s0_kkk = 0; _X_s0_kkk < 0 + 4; _X_s0_kkk++)
               {
                float _458 = _Z_shreg_temp;
                float _460 = _X_shreg[_X_s0_iii][_X_s0_kkk];
                float _462 = _Y_shreg[_X_s0_jjj][_X_s0_kkk];
                float _463 = _460 * _462;
                float _464 = _458 + _463;
                _Z_shreg_temp = _464;
                bool _465 = _X_s0_kkk == 3;
                if (_465)
                {
                 float _466 = _Z_shreg_temp;
                 float _467 = sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_466));
                 _Z_shreg_temp = _467;
                } // if _465
               } // for _X_s0_kkk
               float _468 = _Z_shreg_temp;
               _Z_shreg[0][_X_s0_jjj][_X_s0_iii] = _468;
               (void)_468;
               #pragma unroll
               for (int _X_s0_kkk = 0; _X_s0_kkk < 0 + 4; _X_s0_kkk++)
               {
                bool _469 = _X_s0_kkk == 3;
                int _470 = _X_s0_kk_ii_jj >> 4;
                bool _471 = _470 == 3;
                bool _472 = _469 && _471;
                int _473 = _4 >> 4;
                int _474 = _473 + -1;
                bool _475 = _X_s0_k == _474;
                bool _476 = _472 && _475;
                if (_476)
                {
                 float _478 = _Z_shreg[0][_X_s0_jjj][_X_s0_iii];
                 _Out_channel_array.s[_X_s0_iii][_X_s0_jjj] = _478;
                 (void)_X_s0_jjj;
                 _Out_channel_temp = 1;
                } // if _476
               } // for _X_s0_kkk
              } // for _X_s0_jjj
             } // for _X_s0_iii
             bool _479 = _Out_channel_temp;
             if (_479)
             {
              // write_channel_intel(_Out_channel, _Out_channel_array);
              _Out_channel::write( _Out_channel_array );
              (void)_Out_channel_array;
             } // if _479
            } // for _X_s0_kk_ii_jj
           } // for _X_s0_k
          } // for _X_s0_j
         } // for _X_s0_i
       }); //  h.single_task kernel_Out_class
     }) ); // q_device.submit
     // consume Out
     // consume Z
     // consume Y
     // consume X
     // produce drainer
     // kernel_drainer_gather_Out
     std::cout << "// kernel kernel_drainer_gather_Out\n";
     oneapi_kernel_events.push_back( q_device.submit([&](sycl::handler &h){
       h.single_task<class kernel_drainer_gather_Out_class>([=](){
         _Out_channel_array_t _Out_channel_array;
         _drainer_channel_array_t _drainer_channel_array;
         float _drainer_gather_Out_shreg[4];
         int _480 = _7 >> 4;
         for (int _drainer_s0_i = 0; _drainer_s0_i < 0 + _480; _drainer_s0_i++)
         {
          int _481 = _13 >> 4;
          for (int _drainer_s0_j = 0; _drainer_s0_j < 0 + _481; _drainer_s0_j++)
          {
           for (int _drainer_s0_ii_jj = 0; _drainer_s0_ii_jj < 0 + 16; _drainer_s0_ii_jj++)
           {
            // _Out_channel_array_t __482 = read_channel_intel(_Out_channel);
            _Out_channel_array_t __482 = _Out_channel::read();
            _Out_channel_array = __482;
            (void)__482;
            for (int _drainer_s0_iii_gather = 0; _drainer_s0_iii_gather < 0 + 4; _drainer_s0_iii_gather++)
            {
             #pragma unroll
             for (int _drainer_s0_iii = 0; _drainer_s0_iii < 0 + 4; _drainer_s0_iii++)
             {
              #pragma unroll
              for (int _drainer_s0_jjj = 0; _drainer_s0_jjj < 0 + 4; _drainer_s0_jjj++)
              {
               bool _483 = _drainer_s0_iii_gather == _drainer_s0_iii;
               if (_483)
               {
                float __484 = _Out_channel_array.s[_drainer_s0_iii][_drainer_s0_jjj];
                _drainer_gather_Out_shreg[_drainer_s0_jjj] = __484;
                (void)__484;
               } // if _483
               else
               {
                bool _485 = 0 < _drainer_s0_iii;
                if (_485)
                {
                 float _487 = _drainer_gather_Out_shreg[_drainer_s0_jjj];
                 _drainer_gather_Out_shreg[_drainer_s0_jjj] = _487;
                 (void)_487;
                } // if _485
                float _489 = _drainer_gather_Out_shreg[_drainer_s0_jjj];
                float _490 = sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(_489));
                _drainer_gather_Out_shreg[_drainer_s0_jjj] = _490;
                (void)_490;
               } // if _483 else
               bool _491 = _drainer_s0_iii == 3;
               if (_491)
               {
                float _493 = _drainer_gather_Out_shreg[_drainer_s0_jjj];
                _drainer_channel_array.s[_drainer_s0_jjj] = _493;
                (void)_drainer_s0_jjj;
               } // if _491
              } // for _drainer_s0_jjj
             } // for _drainer_s0_iii
             // write_channel_intel(_drainer_channel, _drainer_channel_array);
             _drainer_channel::write( _drainer_channel_array );
             (void)_drainer_channel_array;
            } // for _drainer_s0_iii_gather
           } // for _drainer_s0_ii_jj
          } // for _drainer_s0_j
         } // for _drainer_s0_i
       }); //  h.single_task kernel_drainer_gather_Out_class
     }) ); // q_device.submit
     // consume drainer
     // produce collector
     // kernel_collector
     std::cout << "// kernel kernel_collector\n";
     oneapi_kernel_events.push_back( q_device.submit([&](sycl::handler &h){
       h.single_task<class kernel_collector_class>([=](){
         _drainer_channel_array_t _drainer_channel_array;
         int _494 = _7 >> 4;
         for (int _collector_s0_i = 0; _collector_s0_i < 0 + _494; _collector_s0_i++)
         {
          int _495 = _13 >> 4;
          for (int _collector_s0_j = 0; _collector_s0_j < 0 + _495; _collector_s0_j++)
          {
           for (int _collector_s0_ii_jj_iii = 0; _collector_s0_ii_jj_iii < 0 + 64; _collector_s0_ii_jj_iii++)
           {
            // _drainer_channel_array_t __496 = read_channel_intel(_drainer_channel);
            _drainer_channel_array_t __496 = _drainer_channel::read();
            _drainer_channel_array = __496;
            (void)__496;
            int _497 = 0 >> 0;
            int _498 = _497 + 0;
            int _499 = (int)(_498);
            float __500 = _drainer_channel_array.s[_499];
            int _501 = _497 + 1;
            int _502 = (int)(_501);
            float __503 = _drainer_channel_array.s[_502];
            int _504 = _497 + 2;
            int _505 = (int)(_504);
            float __506 = _drainer_channel_array.s[_505];
            int _507 = _497 + 3;
            int _508 = (int)(_507);
            float __509 = _drainer_channel_array.s[_508];
            float4 _510 = (float4){__500, __503, __506, __509}; // Converted using c++ standard (){} instead of ()()
            // write_channel_intel(_collector_channel, _510);
            _collector_channel::write( _510 );
            (void)_510;
           } // for _collector_s0_ii_jj_iii
          } // for _collector_s0_j
         } // for _collector_s0_i
       }); //  h.single_task kernel_collector_class
     }) ); // q_device.submit
     // consume collector
     halide_buffer_t b4;
     struct halide_buffer_t *_511 = &b4;
     int32_t _512 = _13 >> 4;
     int32_t _513 = _7 >> 4;
     int32_t _514 = _512 * 256;
     struct halide_dimension_t s9[6] = {
      {0, 4, 1, 0},
      {0, 4, 4, 0},
      {0, 4, 16, 0},
      {0, 4, 64, 0},
      {0, _512, 256, 0},
      {0, _513, _514, 0},
     };
     struct halide_dimension_t *_515 = s9;
     uint64_t _516 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
     void *_517 = (void *)(_516);
     struct halide_device_interface_t *_518 = (struct halide_device_interface_t *)(_516);
     struct halide_dimension_t s10[6] = {
      {0, 4, 1, 0},
      {0, 4, 4, 0},
      {0, 4, 16, 0},
      {0, 4, 64, 0},
      {0, _512, 256, 0},
      {0, _513, _514, 0},
     };
     struct halide_dimension_t *_519 = s10;
     struct halide_buffer_t *_520 = _halide_buffer_init(_511, _515, _517, _516, _518, 2, 32, 6, _519, _516);
     struct halide_buffer_t * _unloader_mem_channel_buffer = _520;
     struct halide_device_interface_t const *_521 = NULL /* halide_oneapi_device_interface() replaced */;
     int32_t _522 = 0; // halide_device_and_host_malloc(_unloader_mem_channel_buffer, _521) replaced with line(s) below 
     if( !_unloader_mem_channel_buffer->device ){ // device malloc
       std::cout << "//	 device malloc _unloader_mem_channel_buffer\n";
       assert(_unloader_mem_channel_buffer->size_in_bytes() != 0);
       uint64_t lowest_index = 0;
       uint64_t highest_index = 0;
       for (int i = 0; i < _unloader_mem_channel_buffer->dimensions; i++) {
           if (_unloader_mem_channel_buffer->dim[i].stride < 0) {
               lowest_index += (uint64_t)(_unloader_mem_channel_buffer->dim[i].stride) * (_unloader_mem_channel_buffer->dim[i].extent - 1);
           }
           if (_unloader_mem_channel_buffer->dim[i].stride > 0) {
               highest_index += (uint64_t)(_unloader_mem_channel_buffer->dim[i].stride) * (_unloader_mem_channel_buffer->dim[i].extent - 1);
           }
       }
       device_handle *dev_handle = (device_handle *)std::malloc(sizeof(device_handle));
       dev_handle->mem = (void*)sycl::malloc_device(_unloader_mem_channel_buffer->size_in_bytes(), q_device);
       dev_handle->offset = 0;
       _unloader_mem_channel_buffer->device = (uint64_t)dev_handle;
     };
     { // host malloc
       std::cout << "//\t host malloc _unloader_mem_channel_buffer\n";
       assert(_unloader_mem_channel_buffer->size_in_bytes() != 0);
       _unloader_mem_channel_buffer->host = (uint8_t*)std::malloc(_unloader_mem_channel_buffer->size_in_bytes() );
       assert(_unloader_mem_channel_buffer->host != NULL);
     };
     struct s11 { void * const ucon; void * const arg; s11(void *ucon, void *a) : ucon(ucon), arg((void *)a) {} ~s11() { halide_device_and_host_free_as_destructor(ucon, arg); } } d2(_ucon, _unloader_mem_channel_buffer);
     void *_523 = 0;
     (void)_523;
     {
      void *_524 = _halide_buffer_get_host(_unloader_mem_channel_buffer);
      float *_unloader_mem_channel = (float *)(_524);
      if (!_unloader_mem_channel)
      {
       std::cout << "Condition '_unloader_mem_channel' failed with error id_msg: None\n";
       assert(false);
      }
      // produce unloader
      struct halide_device_interface_t const *_525 = NULL /* halide_oneapi_device_interface() replaced */;
      int32_t _526 = 0; // halide_device_malloc(_unloader_mem_channel_buffer, _525) replaced with line(s) below 
      if( !_unloader_mem_channel_buffer->device ){ // device malloc
        std::cout << "//	 device malloc _unloader_mem_channel_buffer\n";
        assert(_unloader_mem_channel_buffer->size_in_bytes() != 0);
        uint64_t lowest_index = 0;
        uint64_t highest_index = 0;
        for (int i = 0; i < _unloader_mem_channel_buffer->dimensions; i++) {
            if (_unloader_mem_channel_buffer->dim[i].stride < 0) {
                lowest_index += (uint64_t)(_unloader_mem_channel_buffer->dim[i].stride) * (_unloader_mem_channel_buffer->dim[i].extent - 1);
            }
            if (_unloader_mem_channel_buffer->dim[i].stride > 0) {
                highest_index += (uint64_t)(_unloader_mem_channel_buffer->dim[i].stride) * (_unloader_mem_channel_buffer->dim[i].extent - 1);
            }
        }
        device_handle *dev_handle = (device_handle *)std::malloc(sizeof(device_handle));
        dev_handle->mem = (void*)sycl::malloc_device(_unloader_mem_channel_buffer->size_in_bytes(), q_device);
        dev_handle->offset = 0;
        _unloader_mem_channel_buffer->device = (uint64_t)dev_handle;
      };
      // kernel_unloader
      std::cout << "// kernel kernel_unloader\n";
      _unloader_mem_channel = (float*)(((device_handle*) _unloader_mem_channel_buffer->device)->mem);
      oneapi_kernel_events.push_back( q_device.submit([&](sycl::handler &h){
        h.single_task<class kernel_unloader_class>([=](){
          int _addr_temp;
          _addr_temp = 0;
          int _527 = _7 >> 4;
          for (int _unloader_s0_i = 0; _unloader_s0_i < 0 + _527; _unloader_s0_i++)
          {
           int _528 = _13 >> 4;
           for (int _unloader_s0_j = 0; _unloader_s0_j < 0 + _528; _unloader_s0_j++)
           {
            for (int _unloader_s0_ii_jj_iii = 0; _unloader_s0_ii_jj_iii < 0 + 64; _unloader_s0_ii_jj_iii++)
            {
             // float4 __529 = read_channel_intel(_collector_channel);
             float4 __529 = _collector_channel::read();
             int _530 = _addr_temp;
             int _531 = _530 * 4;
             _unloader_mem_channel[_531+0] = __529[0];
             _unloader_mem_channel[_531+1] = __529[1];
             _unloader_mem_channel[_531+2] = __529[2];
             _unloader_mem_channel[_531+3] = __529[3];
             int _532 = _addr_temp;
             int _533 = _532 + 1;
             _addr_temp = _533;
            } // for _unloader_s0_ii_jj_iii
           } // for _unloader_s0_j
          } // for _unloader_s0_i
        }); //  h.single_task kernel_unloader_class
      }) ); // q_device.submit
      int32_t _534 = 0; // halide_opencl_wait_for_kernels_finish() replaced with line(s) below 
      for(unsigned int i = 0; i < oneapi_kernel_events.size(); i++){ oneapi_kernel_events.at(i).wait(); };
      bool _535 = (bool)(ADD_UINT64_T_SUFFIX(1));
      int32_t _536 = _halide_buffer_set_device_dirty(_unloader_mem_channel_buffer, _535);
      (void)_536;
      // consume unloader
      // produce deserializer
      {
       int32_t _addr_temp;
       _addr_temp = 0;
       int32_t _537 = 0; // halide_copy_to_host(_unloader_mem_channel_buffer) replaced with line(s) below 
       { // memcpy 
         bool from_host = (_unloader_mem_channel_buffer->device == 0) || (_unloader_mem_channel_buffer->host_dirty() && _unloader_mem_channel_buffer->host != NULL);
         bool to_host = 1;
         if (!from_host && to_host) {
           std::cout << "//	 memcpy device->host _unloader_mem_channel_buffer\n";
           q_device.submit([&](handler& h){ h.memcpy((void *)_unloader_mem_channel_buffer->host, (void *)(((device_handle*)_unloader_mem_channel_buffer->device)->mem), _unloader_mem_channel_buffer->size_in_bytes() ); }).wait();
         } else if (from_host && !to_host) {
           std::cout << "//	 memcpy host->device _unloader_mem_channel_buffer\n";
           q_device.submit([&](handler& h){ h.memcpy((void *)(((device_handle*)_unloader_mem_channel_buffer->device)->mem), (void *)_unloader_mem_channel_buffer->host, _unloader_mem_channel_buffer->size_in_bytes() ); }).wait();
         } else if (!from_host && !to_host) {
           std::cout << "//	 memcpy device->device not implemented yet\n";
           assert(false);
         } else {
           std::cout << "//	 memcpy _unloader_mem_channel_buffer Do nothing.\n";
         }
       };
       int32_t _538 = 0; // halide_copy_to_host(_deserializer_buffer) replaced with line(s) below 
       { // memcpy 
         bool from_host = (_deserializer_buffer->device == 0) || (_deserializer_buffer->host_dirty() && _deserializer_buffer->host != NULL);
         bool to_host = 1;
         if (!from_host && to_host) {
           std::cout << "//	 memcpy device->host _deserializer_buffer\n";
           q_device.submit([&](handler& h){ h.memcpy((void *)_deserializer_buffer->host, (void *)(((device_handle*)_deserializer_buffer->device)->mem), _deserializer_buffer->size_in_bytes() ); }).wait();
         } else if (from_host && !to_host) {
           std::cout << "//	 memcpy host->device _deserializer_buffer\n";
           q_device.submit([&](handler& h){ h.memcpy((void *)(((device_handle*)_deserializer_buffer->device)->mem), (void *)_deserializer_buffer->host, _deserializer_buffer->size_in_bytes() ); }).wait();
         } else if (!from_host && !to_host) {
           std::cout << "//	 memcpy device->device not implemented yet\n";
           assert(false);
         } else {
           std::cout << "//	 memcpy _deserializer_buffer Do nothing.\n";
         }
       };
       // kernel_deserializer
       std::cout << "// kernel kernel_deserializer\n";
       float *_deserializer = (float*)(_deserializer_buffer->host);
       _unloader_mem_channel = (float*)(_unloader_mem_channel_buffer->host);
       {
        int _539 = _7 >> 4;
        for (int _deserializer_s0_i = 0; _deserializer_s0_i < 0 + _539; _deserializer_s0_i++)
        {
         int _540 = _13 >> 4;
         for (int _deserializer_s0_j = 0; _deserializer_s0_j < 0 + _540; _deserializer_s0_j++)
         {
          for (int _deserializer_s0_ii_jj_iii = 0; _deserializer_s0_ii_jj_iii < 0 + 64; _deserializer_s0_ii_jj_iii++)
          {
           int _541 = _addr_temp;
           float _542 = _unloader_mem_channel[_541];
           int _543 = _deserializer_s0_i * _38;
           int _544 = _deserializer_s0_j * _35;
           int _545 = _deserializer_s0_ii_jj_iii >> 4;
           int _546 = _545 * _32;
           int _547 = _deserializer_s0_ii_jj_iii & 15;
           int _548 = _547 >> 2;
           int _549 = _548 * _29;
           int _550 = _deserializer_s0_ii_jj_iii & 3;
           int _551 = _550 * _26;
           int _552 = _549 + _551;
           int _553 = _546 + _552;
           int _554 = _544 + _553;
           int _555 = _543 + _554;
           int _556 = _36 * _38;
           int _557 = _33 * _35;
           int _558 = _30 * _32;
           int _559 = _27 * _29;
           int _560 = _24 * _26;
           int _561 = _560 + _21;
           int _562 = _559 + _561;
           int _563 = _558 + _562;
           int _564 = _557 + _563;
           int _565 = _556 + _564;
           int _566 = _555 - _565;
           (( float *)_deserializer)[_566] = _542;
           int _567 = _addr_temp;
           int _568 = _567 + 1;
           float _569 = _unloader_mem_channel[_568];
           int _570 = _deserializer_s0_i * _38;
           int _571 = _deserializer_s0_j * _35;
           int _572 = _deserializer_s0_ii_jj_iii >> 4;
           int _573 = _572 * _32;
           int _574 = _deserializer_s0_ii_jj_iii & 15;
           int _575 = _574 >> 2;
           int _576 = _575 * _29;
           int _577 = _deserializer_s0_ii_jj_iii & 3;
           int _578 = _577 * _26;
           int _579 = _576 + _578;
           int _580 = _573 + _579;
           int _581 = _571 + _580;
           int _582 = _570 + _581;
           int _583 = _36 * _38;
           int _584 = _33 * _35;
           int _585 = _30 * _32;
           int _586 = _27 * _29;
           int _587 = _24 * _26;
           int _588 = _587 + _21;
           int _589 = _586 + _588;
           int _590 = _585 + _589;
           int _591 = _584 + _590;
           int _592 = _583 + _591;
           int _593 = _582 - _592;
           int _594 = _593 + 1;
           (( float *)_deserializer)[_594] = _569;
           int _595 = _addr_temp;
           int _596 = _595 + 2;
           float _597 = _unloader_mem_channel[_596];
           int _598 = _deserializer_s0_i * _38;
           int _599 = _deserializer_s0_j * _35;
           int _600 = _deserializer_s0_ii_jj_iii >> 4;
           int _601 = _600 * _32;
           int _602 = _deserializer_s0_ii_jj_iii & 15;
           int _603 = _602 >> 2;
           int _604 = _603 * _29;
           int _605 = _deserializer_s0_ii_jj_iii & 3;
           int _606 = _605 * _26;
           int _607 = _604 + _606;
           int _608 = _601 + _607;
           int _609 = _599 + _608;
           int _610 = _598 + _609;
           int _611 = _36 * _38;
           int _612 = _33 * _35;
           int _613 = _30 * _32;
           int _614 = _27 * _29;
           int _615 = _24 * _26;
           int _616 = _615 + _21;
           int _617 = _614 + _616;
           int _618 = _613 + _617;
           int _619 = _612 + _618;
           int _620 = _611 + _619;
           int _621 = _610 - _620;
           int _622 = _621 + 2;
           (( float *)_deserializer)[_622] = _597;
           int _623 = _addr_temp;
           int _624 = _623 + 3;
           float _625 = _unloader_mem_channel[_624];
           int _626 = _deserializer_s0_i * _38;
           int _627 = _deserializer_s0_j * _35;
           int _628 = _deserializer_s0_ii_jj_iii >> 4;
           int _629 = _628 * _32;
           int _630 = _deserializer_s0_ii_jj_iii & 15;
           int _631 = _630 >> 2;
           int _632 = _631 * _29;
           int _633 = _deserializer_s0_ii_jj_iii & 3;
           int _634 = _633 * _26;
           int _635 = _632 + _634;
           int _636 = _629 + _635;
           int _637 = _627 + _636;
           int _638 = _626 + _637;
           int _639 = _36 * _38;
           int _640 = _33 * _35;
           int _641 = _30 * _32;
           int _642 = _27 * _29;
           int _643 = _24 * _26;
           int _644 = _643 + _21;
           int _645 = _642 + _644;
           int _646 = _641 + _645;
           int _647 = _640 + _646;
           int _648 = _639 + _647;
           int _649 = _638 - _648;
           int _650 = _649 + 3;
           (( float *)_deserializer)[_650] = _625;
           int _651 = _addr_temp;
           int _652 = _651 + 4;
           _addr_temp = _652;
          } // for _deserializer_s0_ii_jj_iii
         } // for _deserializer_s0_j
        } // for _deserializer_s0_i
       } // // kernel_kernel_deserializer
       bool _653 = (bool)(ADD_UINT64_T_SUFFIX(1));
       int32_t _654 = _halide_buffer_set_host_dirty(_deserializer_buffer, _653);
       (void)_654;
       int32_t _655 = 0; // halide_device_and_host_free(_unloader_mem_channel_buffer) replaced with line(s) below 
       if( _unloader_mem_channel_buffer->device ){ // device free
         sycl::free( ((device_handle*)_unloader_mem_channel_buffer->device)->mem , q_device);
         assert(((device_handle *)_unloader_mem_channel_buffer->device)->offset == 0);
         std::free((device_handle *)_unloader_mem_channel_buffer->device);
         _unloader_mem_channel_buffer->set_device_dirty(false);
       }
       if(_unloader_mem_channel_buffer->host){ // host free
         std::free( (void*)_unloader_mem_channel_buffer->host );
         _unloader_mem_channel_buffer->host = NULL;
         _unloader_mem_channel_buffer->set_host_dirty(false);
       };
      } // alloc _addr_temp
      _unloader_mem_channel = NULL;
     } // alloc _unloader_mem_channel
     _B_serializer_mem_channel = NULL;
    } // alloc _B_serializer_mem_channel
    _A_serializer_mem_channel = NULL;
   } // alloc _A_serializer_mem_channel
  } // if _68
  for(unsigned int i = 0; i < oneapi_kernel_events.size(); i++){ oneapi_kernel_events.at(i).wait(); };
  std::cout << "// return the kernel execution time in nanoseconds\n";
  if(oneapi_kernel_events.size() > 0){
  	double k_earliest_start_time = oneapi_kernel_events.at(0).get_profiling_info<sycl::info::event_profiling::command_start>();
  	double k_latest_end_time = oneapi_kernel_events.at(0).get_profiling_info<sycl::info::event_profiling::command_end>();
  	for (unsigned i = 1; i < oneapi_kernel_events.size(); i++) {
  	  double tmp_start = oneapi_kernel_events.at(i).get_profiling_info<sycl::info::event_profiling::command_start>();
  	  double tmp_end = oneapi_kernel_events.at(i).get_profiling_info<sycl::info::event_profiling::command_end>();
  	  if (tmp_start < k_earliest_start_time) {
           k_earliest_start_time = tmp_start;
  	  }
  	  if (tmp_end > k_latest_end_time) {
           k_latest_end_time = tmp_end;
  	  }
  	}
  	// Get time in ns
  	double events_time = (k_latest_end_time - k_earliest_start_time);
  	return events_time;
  }
  return (double)0;
}

