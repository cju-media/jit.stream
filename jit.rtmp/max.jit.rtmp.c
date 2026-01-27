#include "jit.common.h"
#include "max.jit.mop.h"

typedef struct _max_jit_rtmp 
{
    t_object        ob;
    void            *obex;
} t_max_jit_rtmp;

t_jit_err jit_rtmp_init(void); 

void *max_jit_rtmp_new(t_symbol *s, long argc, t_atom *argv);
void max_jit_rtmp_free(t_max_jit_rtmp *x);
void max_jit_rtmp_jit_gl_texture(t_max_jit_rtmp *x, t_symbol *s, long argc, t_atom *argv);
void *max_jit_rtmp_class;

void ext_main(void *r)
{
    void *p, *q;
    
    jit_rtmp_init();
    
    setup((t_messlist **)&max_jit_rtmp_class, (method)max_jit_rtmp_new, (method)max_jit_rtmp_free, (short)sizeof(t_max_jit_rtmp), 0, A_GIMME, 0);

    p = max_jit_classex_setup(calcoffset(t_max_jit_rtmp, obex));
    q = jit_class_findbyname(gensym("jit_rtmp"));
    max_jit_classex_mop_wrap(p, q, 0); // 0 means do not use standard matrix_calc wrapper, but usually we do? 
    // Actually, for simple external we want the standard behavior which forwards jit_matrix to the jit object.
    // The standard way is using max_jit_classex_mop_wrap.
    
    max_jit_classex_standard_wrap(p, q, 0);
    addmess((method)max_jit_mop_jit_matrix, "jit_matrix", A_GIMME, 0);
    addmess((method)max_jit_rtmp_jit_gl_texture, "jit_gl_texture", A_GIMME, 0);
}

void max_jit_rtmp_jit_gl_texture(t_max_jit_rtmp *x, t_symbol *s, long argc, t_atom *argv)
{
    object_error((t_object *)x, "jit.rtmp: direct texture input not supported. Please use [jit.gl.asmatrix] to convert texture to matrix.");
}

void *max_jit_rtmp_new(t_symbol *s, long argc, t_atom *argv)
{
    t_max_jit_rtmp *x;
    void *o;

    if (x = (t_max_jit_rtmp *)max_jit_obex_new(max_jit_rtmp_class, gensym("jit_rtmp"))) {
        if (o = jit_object_new(gensym("jit_rtmp"))) {
            max_jit_mop_setup_simple(x, o, argc, argv);
            max_jit_attr_args(x, argc, argv);
        } else {
            jit_object_error((t_object *)x, "jit.rtmp: could not allocate object");
            freeobject((t_object *)x);
            x = NULL;
        }
    }
    return (x);
}

void max_jit_rtmp_free(t_max_jit_rtmp *x)
{
    max_jit_mop_free(x);
    jit_object_free(max_jit_obex_jitob_get(x));
    max_jit_obex_free(x);
}
