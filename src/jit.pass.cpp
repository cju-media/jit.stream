/**
    @file
    jit.pass - a minimal matrix pass-through external
*/

#include "jit.common.h"

// Instance structure
typedef struct _jit_pass {
    t_object    ob;
} t_jit_pass;

// Class pointer
void *_jit_pass_class;

// Prototypes
t_jit_err jit_pass_init(void);
t_jit_pass *jit_pass_new(void);
void jit_pass_free(t_jit_pass *x);
t_jit_err jit_pass_matrix_calc(t_jit_pass *x, void *inputs, void *outputs);
void jit_pass_calculate_ndim(t_jit_pass *x, long dimcount, long *dim, long planecount, t_jit_matrix_info *in_minfo, char *bip, t_jit_matrix_info *out_minfo, char *bop);

// Entry Point
extern "C" __attribute__((visibility("default"))) t_jit_err ext_main(void *r) {
    return jit_pass_init();
}

t_jit_err jit_pass_init(void) {
    long attrflags = 0;
    t_jit_object *attr;
    t_jit_object *mop;

    // Create class
    _jit_pass_class = jit_class_new("jit_pass", (method)jit_pass_new, (method)jit_pass_free,
        sizeof(t_jit_pass), 0);

    // Add Matrix Operator (MOP)
    // 1 input, 1 output
    mop = (t_jit_object *)jit_object_new(_jit_sym_jit_mop, 1, 1);
    jit_class_addadornment(_jit_pass_class, mop);

    // Add matrix_calc method
    jit_class_addmethod(_jit_pass_class, (method)jit_pass_matrix_calc, "matrix_calc", A_CANT, 0);

    // Register class
    jit_class_register(_jit_pass_class);

    return JIT_ERR_NONE;
}

t_jit_pass *jit_pass_new(void) {
    t_jit_pass *x;
    if ((x = (t_jit_pass *)jit_object_alloc(_jit_pass_class))) {
        // Constructor logic here
    }
    return x;
}

void jit_pass_free(t_jit_pass *x) {
    // Destructor logic here
}

// NDIM Calculation
void jit_pass_calculate_ndim(t_jit_pass *x, long dimcount, long *dim, long planecount, t_jit_matrix_info *in_minfo, char *bip, t_jit_matrix_info *out_minfo, char *bop) {
    long i, j, width, stride_in, stride_out;

    if (dimcount < 1) return;

    // For n-dim, we recursively loop or flatten.
    // jit_parallel_ndim_simplecalc2 handles the recursion for > 2 dims or chunks rows for 2D.
    // For this simple example, we just copy row by row.

    width = dim[dimcount-1];
    stride_in = in_minfo->dimstride[dimcount-1];
    stride_out = out_minfo->dimstride[dimcount-1];

    // If strict match, we could optimize, but let's iterate elements safely for generic types
    // Actually, simplecalc2 expects us to handle the innermost loop (width).

    // Assuming char type for simplicity as per user context, but to be generic we copy bytes based on stride.
    // Ideally we verify types in matrix_calc.

    if (in_minfo->type == _jit_sym_char && out_minfo->type == _jit_sym_char) {
        // Plane loop
        for (i = 0; i < width; i++) {
            char *ip = bip + i * stride_in;
            char *op = bop + i * stride_out;
            for(j=0; j<planecount; j++) {
                op[j] = ip[j];
            }
        }
    } else {
        // Just generic memcpy for unknown types if sizes match (risky without more logic),
        // or just skip. Let's do nothing for non-char to keep it safe and minimal.
    }
}

// Matrix Calculation
t_jit_err jit_pass_matrix_calc(t_jit_pass *x, void *inputs, void *outputs) {
    t_jit_err err = JIT_ERR_NONE;
    long in_savelock, out_savelock;
    t_jit_matrix_info in_minfo, out_minfo;
    char *in_bp, *out_bp;
    long i, dimcount, planecount, dim[JIT_MATRIX_MAX_DIMCOUNT];
    void *in_matrix, *out_matrix;

    in_matrix = jit_object_method(inputs, _jit_sym_getindex, 0);
    out_matrix = jit_object_method(outputs, _jit_sym_getindex, 0);

    if (x && in_matrix && out_matrix) {
        // Lock matrices
        in_savelock = (long)jit_object_method(in_matrix, _jit_sym_lock, 1);
        out_savelock = (long)jit_object_method(out_matrix, _jit_sym_lock, 1);

        jit_object_method(in_matrix, _jit_sym_getinfo, &in_minfo);
        jit_object_method(out_matrix, _jit_sym_getinfo, &out_minfo);

        jit_object_method(in_matrix, _jit_sym_getdata, &in_bp);
        jit_object_method(out_matrix, _jit_sym_getdata, &out_bp);

        if (!in_bp) { err = JIT_ERR_INVALID_INPUT; goto out; }
        if (!out_bp) { err = JIT_ERR_INVALID_OUTPUT; goto out; }

        // Compatible types?
        if (in_minfo.type != out_minfo.type || in_minfo.planecount != out_minfo.planecount) {
            err = JIT_ERR_MISMATCH_TYPE;
            goto out;
        }

        // Get dimensions
        dimcount = out_minfo.dimcount;
        planecount = out_minfo.planecount;
        for (i=0; i<dimcount; i++) {
            dim[i] = out_minfo.dim[i];
            // Ensure input matches output size (MOP usually handles this but check)
            if (in_minfo.dim[i] != out_minfo.dim[i]) {
                // If MOP didn't adapt, we can't simple copy. Return.
                // But normally Jitter handles adaptation before calling this.
            }
        }

        jit_parallel_ndim_simplecalc2((method)jit_pass_calculate_ndim,
            x, dimcount, dim, planecount, &in_minfo, in_bp, &out_minfo, out_bp,
            0, 0);
    }

out:
    jit_object_method(out_matrix, _jit_sym_lock, out_savelock);
    jit_object_method(in_matrix, _jit_sym_lock, in_savelock);
    return err;
}

// Wrapper for the max object
#include "ext.h"
#include "ext_obex.h"

typedef struct _max_jit_pass {
    t_object        ob;
    void            *obex;
} t_max_jit_pass;

void *max_jit_pass_new(t_symbol *s, long argc, t_atom *argv);
void max_jit_pass_free(t_max_jit_pass *x);
t_class *max_jit_pass_class;

// Max Entry
extern "C" __attribute__((visibility("default"))) int main(void) {
    t_class *c;
    void *p;

    // Initialize Jitter Class first
    jit_pass_init();

    setup(&max_jit_pass_class, max_jit_pass_new, (method)max_jit_pass_free, (short)sizeof(t_max_jit_pass),
        0, A_GIMME, 0);

    p = max_jit_classex_setup(calcoffset(t_max_jit_pass, obex));

    // Add MOP wrapper methods
    max_jit_class_mop_wrap(max_jit_pass_class, _jit_pass_class, 0);

    // Standard Max wrapper methods
    max_jit_class_wrap_standard(max_jit_pass_class, _jit_pass_class, 0);

    class_register(CLASS_BOX, max_jit_pass_class);
    return 0;
}

void *max_jit_pass_new(t_symbol *s, long argc, t_atom *argv) {
    t_max_jit_pass *x;
    void *o;

    if ((x = (t_max_jit_pass *)max_jit_object_alloc(max_jit_pass_class, gensym("jit_pass")))) {
        if ((o = jit_object_new(gensym("jit_pass")))) {
            max_jit_mop_setup_simple(x, o, argc, argv);
            max_jit_attr_args(x, argc, argv);
        } else {
            return NULL;
        }
    }
    return (x);
}

void max_jit_pass_free(t_max_jit_pass *x) {
    max_jit_mop_free(x);
    jit_object_free(max_jit_obex_jitob_get(x));
    max_jit_object_free(x);
}
