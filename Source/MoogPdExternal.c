/* GPL-3.0-or-later: CA-72 Model D external registered in EFFECTS libpd.
 * Pd device is JUCE's existing audio callback; this object opens no devices. */
#include "m_pd.h"
#include <stdint.h>
#include <stddef.h>

typedef struct Engine Engine;
extern Engine* ca72_pd_new(double rate, uint32_t poly);
extern void ca72_pd_free(Engine*);
extern void ca72_pd_note(Engine*, int32_t, int32_t);
extern void ca72_pd_parameter(Engine*, uint32_t, double);
extern void ca72_pd_process(Engine*, float*, float*, int32_t);

typedef struct _moog_model_d {
    t_object x_obj;
    t_float dummy_in;
    t_outlet* left;
    t_outlet* right;
    Engine* engine;
    int poly;
    double rate;
} t_moog_model_d;

static t_class* moog_model_d_class;

static void moog_note(t_moog_model_d* x,t_floatarg pitch,t_floatarg velocity) {
    if (x->engine)
        ca72_pd_note(x->engine,(int32_t)pitch,(int32_t)velocity);
}
static void moog_parameter(t_moog_model_d* x,t_floatarg id,t_floatarg value) {
    if (x->engine && id>=0)
        ca72_pd_parameter(x->engine,(uint32_t)id,(double)value);
}
static t_int* moog_perform(t_int* w) {
    t_moog_model_d* x = (t_moog_model_d*)w[1];
    t_sample* l = (t_sample*)w[2];
    t_sample* r = (t_sample*)w[3];
    int n = (int)w[4];
    if (x->engine) {
        ca72_pd_process(x->engine,(float*)l,(float*)r,(int32_t)n);
    } else {
        for (int i=0;i<n;i++) l[i]=r[i]=0;
    }
    return w+5;
}
static void moog_dsp(t_moog_model_d* x,t_signal** sp) {
    if (sizeof(t_sample) != sizeof(float)) {
        pd_error(x,"moog_model_d~ requires 32-bit Pd samples");
        return;
    }
    if (!x->engine || x->rate != sp[0]->s_sr) {
        if (x->engine) ca72_pd_free(x->engine);
        x->rate = sp[0]->s_sr;
        x->engine = ca72_pd_new(x->rate,(uint32_t)x->poly);
    }
    // sp[0] is the signal inlet; sp[1] and sp[2] are our outlets.
    dsp_add(moog_perform,4,x,sp[1]->s_vec,sp[2]->s_vec,sp[0]->s_n);
}
static void moog_free(t_moog_model_d* x) {
    if (x->engine) ca72_pd_free(x->engine);
    x->engine = NULL;
}
static void* moog_new(t_floatarg poly) {
    t_moog_model_d* x = (t_moog_model_d*)pd_new(moog_model_d_class);
    x->engine = NULL;
    x->rate = 0;
    x->poly = (int)poly;
    if (x->poly<1) x->poly=1;
    if (x->poly>8) x->poly=8;
    x->left = outlet_new(&x->x_obj,&s_signal);
    x->right = outlet_new(&x->x_obj,&s_signal);
    return x;
}
void moog_model_d_tilde_setup(void) {
    if (moog_model_d_class) return;
    moog_model_d_class=class_new(gensym("moog_model_d~"),
        (t_newmethod)moog_new,(t_method)moog_free,
        sizeof(t_moog_model_d),CLASS_DEFAULT,A_DEFFLOAT,0);
    CLASS_MAINSIGNALIN(moog_model_d_class,t_moog_model_d,dummy_in);
    class_addmethod(moog_model_d_class,(t_method)moog_dsp,gensym("dsp"),A_CANT,0);
    class_addmethod(moog_model_d_class,(t_method)moog_note,gensym("note"),A_FLOAT,A_FLOAT,0);
    class_addmethod(moog_model_d_class,(t_method)moog_parameter,gensym("param"),A_FLOAT,A_FLOAT,0);
}
