/****************************************************************************
 * Copyright (C) 2007-2008 Jean-Marc Valin                                  *
 * Copyright (C) 2008      Thorvald Natvig                                  *
 *                                                                          *
 * File: resample.c                                                         *
 * Arbitrary resampling code                                                *
 ****************************************************************************
 * Redistribution and use in source and binary forms, with or without       *
 * modification, are permitted provided that the following conditions are   *
 * met:                                                                     *
 *                                                                          *
 * 1. Redistributions of source code must retain the above copyright notice,*
 * this list of conditions and the following disclaimer.                    *
 *                                                                          *
 * 2. Redistributions in binary form must reproduce the above copyright     *
 * notice, this list of conditions and the following disclaimer in the      *
 * documentation and/or other materials provided with the distribution.     *
 *                                                                          *
 * 3. The name of the author may not be used to endorse or promote products *
 * derived from this software without specific prior written permission.    *
 ****************************************************************************
 * The design goals of this code are:                                       *
 *   - Very fast algorithm                                                  *
 *   - SIMD-friendly algorithm                                              *
 *   - Low memory requirement                                               *
 *   - Good *perceptual* quality (and not best SNR)                         *
 * Warning: This resampler is relatively new. Although I think I got rid of *
 * all the major bugs and I don't expect the API to change anymore, there   *
 * may be something I've missed. So use with caution.                       *
 *                                                                          *
 * This algorithm is based on this original resampling algorithm:           *
 * Smith, Julius O. Digital Audio Resampling Home Page                      *
 * Center for Computer Research in Music and Acoustics (CCRMA),             *
 * Stanford University, 2007.                                               *
 * Web published at https://ccrma.stanford.edu/~jos/resample/.              *
 *                                                                          *
 * There is one main difference, though. This resampler uses cubic          *
 * interpolation instead of linear interpolation in the above paper. This   *
 * makes the table much smaller and makes it possible to compute that table *
 * on a per-stream basis. In turn, being able to tweak the table for each   *
 * stream makes it possible to both reduce complexity on simple ratios      *
 * (e.g. 2/3), and get rid of the rounding operations in the inner loop.    *
 * The latter both reduces CPU time and makes the algorithm more            *
 * SIMD-friendly.                                                           *
 ****************************************************************************/

#define MULT16_32_Q15(a,b)     ((a)*(b))
#define MULT16_16(a,b)     ((spx_word32_t)(a)*(spx_word32_t)(b))
#define SHR32(a,shift) (a)
#define PSHR32(a,shift) (a)
#define SATURATE32PSHR(x,shift,a) (x)
typedef float spx_word16_t;
typedef float spx_word32_t;

#include <stdlib.h>
static void *speex_alloc(size_t size) {return calloc(size,1);}
static void *speex_realloc(void *ptr, size_t size) {return realloc(ptr, size);}
static void speex_free(void *ptr) {free(ptr);}

#include "resample.h"

#include <math.h>
#include <limits.h>
#include <windows.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* #define IMAX(a,b) ((a) > (b) ? (a) : (b))
 * #define IMIN(a,b) ((a) < (b) ? (a) : (b)) */

#ifndef UINT32_MAX
#define UINT32_MAX 4294967295U
#endif

typedef int (*resampler_basic_func)(SpeexResamplerState *, spx_uint32_t , const spx_word16_t *, spx_uint32_t *, spx_word16_t *, spx_uint32_t *);

struct SpeexResamplerState_ {
   spx_uint32_t in_rate;
   spx_uint32_t out_rate;
   spx_uint32_t num_rate;
   spx_uint32_t den_rate;

   int    quality;
   spx_uint32_t nb_channels;
   spx_uint32_t filt_len;
   spx_uint32_t mem_alloc_size;
   spx_uint32_t buffer_size;
   int          int_advance;
   int          frac_advance;
   float  cutoff;
   spx_uint32_t oversample;
   int          initialised;
   int          started;

   /* These are per-channel */
   spx_int32_t  *last_sample;
   spx_uint32_t *samp_frac_num;
   spx_uint32_t *magic_samples;

   spx_word16_t *mem;
   spx_word16_t *sinc_table;
   spx_uint32_t sinc_table_length;
   resampler_basic_func resampler_ptr;

   int    in_stride;
   int    out_stride;
} ;

static const float kaiser8_table[36] = {
   0.99635258, 1.00000000, 0.99635258, 0.98548012, 0.96759014, 0.94302200,
   0.91223751, 0.87580811, 0.83439927, 0.78875245, 0.73966538, 0.68797126,
   0.63451750, 0.58014482, 0.52566725, 0.47185369, 0.41941150, 0.36897272,
   0.32108304, 0.27619388, 0.23465776, 0.19672670, 0.16255380, 0.13219758,
   0.10562887, 0.08273982, 0.06335451, 0.04724088, 0.03412321, 0.02369490,
   0.01563093, 0.00959968, 0.00527363, 0.00233883, 0.00050000, 0.00000000
};

static const float kaiser6_table[36] = {
   0.99733006, 1.00000000, 0.99733006, 0.98935595, 0.97618418, 0.95799003,
   0.93501423, 0.90755855, 0.87598009, 0.84068475, 0.80211977, 0.76076565,
   0.71712752, 0.67172623, 0.62508937, 0.57774224, 0.53019925, 0.48295561,
   0.43647969, 0.39120616, 0.34752997, 0.30580127, 0.26632152, 0.22934058,
   0.19505503, 0.16360756, 0.13508755, 0.10953262, 0.08693120, 0.06722600,
   0.05031820, 0.03607231, 0.02432151, 0.01487334, 0.00752000, 0.00000000
};

struct FuncDef {
   const float *table;
   int oversample;
};

static const struct FuncDef kaiser8_funcdef = {kaiser8_table, 32};
#define KAISER8 (&kaiser8_funcdef)
static const struct FuncDef kaiser6_funcdef = {kaiser6_table, 32};
#define KAISER6 (&kaiser6_funcdef)

struct QualityMapping {
   int base_length;
   int oversample;
   float downsample_bandwidth;
   float upsample_bandwidth;
   const struct FuncDef *window_func;
};

/* This table maps conversion quality to internal parameters. There are two
 * reasons that explain why the up-sampling bandwidth is larger than the
 * down-sampling bandwidth:
 * 1) When up-sampling, we can assume that the spectrum is already attenuated
 *    close to the Nyquist rate (from an A/D or a previous resampling filter)
 * 2) Any aliasing that occurs very close to the Nyquist rate will be masked
 *    by the sinusoids/noise just below the Nyquist rate (guaranteed only for
 *    up-sampling).
 */
static const struct QualityMapping quality_map[5] = {
   {  8,  4, 0.830f, 0.860f, KAISER6 }, /* Q0 */
   { 16,  4, 0.850f, 0.880f, KAISER6 }, /* Q1 */
   { 32,  4, 0.882f, 0.910f, KAISER6 }, /* Q2 */  /* 82.3% cutoff ( ~60 dB stop) 6  */
   { 48,  8, 0.895f, 0.917f, KAISER8 }, /* Q3 */  /* 84.9% cutoff ( ~80 dB stop) 8  */
   { 64,  8, 0.921f, 0.940f, KAISER8 }, /* Q4 */  /* 88.7% cutoff ( ~80 dB stop) 8  */
};
/* 8, 24, 40, 56, 80, 104, 128, 160, 200, 256, 320 */
static inline float compute_func(float x, const struct FuncDef *func)
{
   float y, frac;
   float interp[4];
   int ind;
   y = x*func->oversample;
//   ind = (y>0)? (int)y: (int)(y-1.f);
   ind= (int)(y + 32768.F) - 32768;
   frac = (y-ind);
   /* CSE with handle the repeated powers */
   interp[3] =  -0.1666666667F*frac + 0.1666666667F*frac*(frac*frac);
   interp[2] = frac + 0.5F*(frac*frac) - 0.5F*(frac*frac)*frac;
   interp[0] = -0.3333333333F*frac + 0.5F*(frac*frac) - 0.1666666667F*frac*(frac*frac);
   /* Just to make sure we don't have rounding problems */
   interp[1] = 1.f-interp[3]-interp[2]-interp[0];

   return   interp[0]*func->table[ind] + interp[1]*func->table[ind+1] 
          + interp[2]*func->table[ind+2] + interp[3]*func->table[ind+3];
}

/* The slow way of computing a sinc for the table. Should improve that some day */
static spx_word16_t sinc(float cutoff, float x, int N, const struct FuncDef *window_func)
{
   /*fprintf (stderr, "%f ", x);*/
   float xx, xabs;
   xabs=fabsf(x);
   if (xabs < 1e-6)
      return cutoff;
   else if (xabs > .5*N)
      return 0;

   /*FIXME: Can it really be any slower than this? */
   xx = x * cutoff;
   return cutoff*sinf(M_PI*xx)/(M_PI*xx) * compute_func(2.F*xabs/N, window_func);
}

/* SSE ASEMBLY FUNCTION */
#ifdef __SSE1
float inner_product_single(const float *a, const float *b, unsigned int len);
char HAVE_SSE;
#endif

static int resampler_basic_direct_single(SpeexResamplerState *st, spx_uint32_t channel_index, const spx_word16_t *in, spx_uint32_t *in_len, spx_word16_t *out, spx_uint32_t *out_len)
{
   int N, out_sample, last_sample, j;

   spx_uint32_t samp_frac_num;
   spx_word16_t *sinc_table;
   int out_stride, int_advance, frac_advance;
   spx_uint32_t den_rate;
   spx_word32_t sum;
   const spx_word16_t *sinct;
   const spx_word16_t *iptr;

   N = st->filt_len;
   out_sample = 0;
   last_sample = st->last_sample[channel_index];
   samp_frac_num = st->samp_frac_num[channel_index];
   sinc_table = st->sinc_table;
   out_stride = st->out_stride;
   int_advance = st->int_advance;
   frac_advance = st->frac_advance;
   den_rate = st->den_rate;


   while (!(last_sample >= (spx_int32_t)*in_len || out_sample >= (spx_int32_t)*out_len))
   {
      sinct = &sinc_table[samp_frac_num*N];
      iptr  = &in[last_sample];
      
      #ifdef __SSE1
      if(HAVE_SSE){
         sum = inner_product_single(sinct, iptr, N);
      } else
      #endif
      {
         sum = 0;
         for(j=0; j<N; j++) sum += MULT16_16(sinct[j], iptr[j]);
      }
      sum = SATURATE32PSHR(sum, 15, 32767);
      out[out_stride * out_sample++] = sum;
      last_sample += int_advance;
      samp_frac_num += frac_advance;
      
      if (samp_frac_num >= den_rate)
      {
         samp_frac_num -= den_rate;
         last_sample++;
      }
   }

   st->last_sample[channel_index] = last_sample;
   st->samp_frac_num[channel_index] = samp_frac_num;

   return out_sample;
}

static int multiply_frac(spx_uint32_t *result, spx_uint32_t value, spx_uint32_t num, spx_uint32_t den)
{
   spx_uint32_t major = value / den;
   spx_uint32_t remain = value % den;
   /* TODO: Could use 64 bits operation to check for overflow. But only guaranteed in C99+ */
   if (remain > UINT32_MAX / num || major > UINT32_MAX / num
       || major * num > UINT32_MAX - remain * num / den)
      return RESAMPLER_ERR_OVERFLOW;
   *result = remain * num / den + major * num;
   return RESAMPLER_ERR_SUCCESS;
}

static int update_filter(SpeexResamplerState *st)
{
   spx_uint32_t old_length = st->filt_len;
   spx_uint32_t old_alloc_size = st->mem_alloc_size;
   spx_uint32_t min_sinc_table_length;
   spx_uint32_t min_alloc_size;
   
   spx_int32_t i, j;

   st->int_advance = st->num_rate/st->den_rate;
   st->frac_advance = st->num_rate%st->den_rate;
   st->oversample = quality_map[st->quality].oversample;
   st->filt_len = quality_map[st->quality].base_length;

   if (st->num_rate > st->den_rate)
   {
      /* down-sampling */
      st->cutoff = quality_map[st->quality].downsample_bandwidth * st->den_rate / st->num_rate;
      if (multiply_frac(&st->filt_len,st->filt_len,st->num_rate,st->den_rate) != RESAMPLER_ERR_SUCCESS)
         goto fail;
      /* Round up to make sure we have a multiple of 8 for SSE */
      st->filt_len = ((st->filt_len-1)&(~0x7))+8;
      if (2*st->den_rate < st->num_rate)
         st->oversample >>= 1;
      if (4*st->den_rate < st->num_rate)
         st->oversample >>= 1;
      if (8*st->den_rate < st->num_rate)
         st->oversample >>= 1;
      if (16*st->den_rate < st->num_rate)
         st->oversample >>= 1;
      if (st->oversample < 1)
         st->oversample = 1;
   } else {
      /* up-sampling */
      st->cutoff = quality_map[st->quality].upsample_bandwidth;
   }
   
   if (INT_MAX/sizeof(spx_word16_t)/st->den_rate < st->filt_len)
      goto fail;

   min_sinc_table_length = st->filt_len*st->den_rate;
   
   if (st->sinc_table_length < min_sinc_table_length)
   {
      spx_word16_t *sinc_table = speex_realloc(st->sinc_table,min_sinc_table_length*sizeof(spx_word16_t));
      if (!sinc_table)
         goto fail;

      st->sinc_table = sinc_table;
      st->sinc_table_length = min_sinc_table_length;
   }
   for (i=0;i<(spx_int32_t)st->den_rate;i++)
      for (j=0;j<(spx_int32_t)st->filt_len;j++)
          st->sinc_table[i*st->filt_len+j] = sinc(st->cutoff,((j-(spx_int32_t)st->filt_len/2+1)
                                                - ((float)i)/st->den_rate), st->filt_len
                                                , quality_map[st->quality].window_func);
 
    st->resampler_ptr = resampler_basic_direct_single;

   /*fprintf (stderr, "resampler uses direct sinc table and normalised cutoff %f\n", cutoff);*/
   
   /* Here's the place where we update the filter memory to take into account
      the change in filter length. It's probably the messiest part of the code
      due to handling of lots of corner cases. */

   /* Adding buffer_size to filt_len won't overflow here because filt_len
      could be multiplied by sizeof(spx_word16_t) above. */
   min_alloc_size = st->filt_len-1 + st->buffer_size;
   if (min_alloc_size > st->mem_alloc_size)
   {
      spx_word16_t *mem;
      if (INT_MAX/sizeof(spx_word16_t)/st->nb_channels < min_alloc_size)
          goto fail;
      else if (!(mem = (spx_word16_t*)speex_realloc(st->mem, st->nb_channels*min_alloc_size * sizeof(*mem))))
          goto fail;

      st->mem = mem;
      st->mem_alloc_size = min_alloc_size;
   }
   if (!st->started)
   {
      spx_uint32_t i;
      for (i=0;i<st->nb_channels*st->mem_alloc_size;i++)
         st->mem[i] = 0;
      /*speex_warning("reinit filter");*/
   } else if (st->filt_len > old_length)
   {
      spx_uint32_t i;
      /* Increase the filter length */
      /*speex_warning("increase filter size");*/
      for (i=st->nb_channels;i--;)
      {
         spx_uint32_t j;
         spx_uint32_t olen = old_length;
         /*if (st->magic_samples[i])*/
         {
            /* Try and remove the magic samples as if nothing had happened */

            /* FIXME: This is wrong but for now we need it to avoid going over the array bounds */
            olen = old_length + 2*st->magic_samples[i];
            for (j=old_length-1+st->magic_samples[i];j--;)
               st->mem[i*st->mem_alloc_size+j+st->magic_samples[i]] = st->mem[i*old_alloc_size+j];
            for (j=0;j<st->magic_samples[i];j++)
               st->mem[i*st->mem_alloc_size+j] = 0;
            st->magic_samples[i] = 0;
         }
         if (st->filt_len > olen)
         {
            /* If the new filter length is still bigger than the "augmented" length */
            /* Copy data going backward */
            for (j=0;j<olen-1;j++)
               st->mem[i*st->mem_alloc_size+(st->filt_len-2-j)] = st->mem[i*st->mem_alloc_size+(olen-2-j)];
            /* Then put zeros for lack of anything better */
            for (;j<st->filt_len-1;j++)
               st->mem[i*st->mem_alloc_size+(st->filt_len-2-j)] = 0;
            /* Adjust last_sample */
            st->last_sample[i] += (st->filt_len - olen)/2;
         } else {
            /* Put back some of the magic! */
            st->magic_samples[i] = (olen - st->filt_len)/2;
            for (j=0;j<st->filt_len-1+st->magic_samples[i];j++)
               st->mem[i*st->mem_alloc_size+j] = st->mem[i*st->mem_alloc_size+j+st->magic_samples[i]];
         }
      }
   } else if (st->filt_len < old_length)
   {
      spx_uint32_t i;
      /* Reduce filter length, this a bit tricky. We need to store some of the memory as "magic"
         samples so they can be used directly as input the next time(s) */
      for (i=0;i<st->nb_channels;i++)
      {
         spx_uint32_t j;
         spx_uint32_t old_magic = st->magic_samples[i];
         st->magic_samples[i] = (old_length - st->filt_len)/2;
         /* We must copy some of the memory that's no longer used */
         /* Copy data going backward */
         for (j=0;j<st->filt_len-1+st->magic_samples[i]+old_magic;j++)
            st->mem[i*st->mem_alloc_size+j] = st->mem[i*st->mem_alloc_size+j+st->magic_samples[i]];
         st->magic_samples[i] += old_magic;
      }
   }
   return RESAMPLER_ERR_SUCCESS;

fail:
   st->resampler_ptr = NULL;
   /* st->mem may still contain consumed input samples for the filter.
      Restore filt_len so that filt_len - 1 still points to the position after
      the last of these samples. */
   st->filt_len = old_length;
//   MessageBoxA(NULL, "RESAMPLER_ERR_ALLOC_FAILED", "in_OPUS_resampler", 0);
   
   return RESAMPLER_ERR_ALLOC_FAILED;

} // END update_filter(SpeexResamplerState *st)


SpeexResamplerState *speex_resampler_init(spx_uint32_t nb_channels, spx_uint32_t in_rate, spx_uint32_t out_rate, int quality, int *err)
{
   return speex_resampler_init_frac(nb_channels, in_rate, out_rate, in_rate, out_rate, quality, err);
}

SpeexResamplerState *speex_resampler_init_frac(spx_uint32_t nb_channels
                                             , spx_uint32_t ratio_num
                                             , spx_uint32_t ratio_den
                                             , spx_uint32_t in_rate
                                             , spx_uint32_t out_rate
                                             , int quality
                                             , int *err)
{
   SpeexResamplerState *st;
   int filter_err;
   #ifdef __SSE1
   __builtin_cpu_init();
   HAVE_SSE=__builtin_cpu_supports("sse");
   #endif
//   if(HAVE_SSE)MessageBoxA(NULL, "SSE", "We have SSE", 0);

   if (nb_channels == 0 
       || ratio_num == 0 
       || ratio_den == 0 
       || quality > SPEEX_RESAMPLER_QUALITY_MAX  
       || quality < SPEEX_RESAMPLER_QUALITY_MIN)
   {
      if (err)
         *err = RESAMPLER_ERR_INVALID_ARG;
      return NULL;
   }
   st = speex_alloc(sizeof(SpeexResamplerState));
   if (!st)
   {
      if (err)
         *err = RESAMPLER_ERR_ALLOC_FAILED;
      return NULL;
   }
   st->initialised = 0;
   st->started = 0;
   st->in_rate = 0;
   st->out_rate = 0;
   st->num_rate = 0;
   st->den_rate = 0;
   st->quality = -1;
   st->sinc_table_length = 0;
   st->mem_alloc_size = 0;
   st->filt_len = 0;
   st->mem = 0;
   st->resampler_ptr = 0;

   st->cutoff = 1.f;
   st->nb_channels = nb_channels;
   st->in_stride = 1;
   st->out_stride = 1;

   st->buffer_size = 160;

   /* Per channel data */
   if (!(st->last_sample = speex_alloc(nb_channels*sizeof(spx_int32_t))))
      goto fail;
   if (!(st->magic_samples = speex_alloc(nb_channels*sizeof(spx_uint32_t))))
      goto fail;
   if (!(st->samp_frac_num = speex_alloc(nb_channels*sizeof(spx_uint32_t))))
      goto fail;

   speex_resampler_set_quality(st, quality);
   speex_resampler_set_rate_frac(st, ratio_num, ratio_den, in_rate, out_rate);

   filter_err = update_filter(st);
   if (filter_err == RESAMPLER_ERR_SUCCESS)
   {
      st->initialised = 1;
   } else {
      speex_resampler_destroy(st);
      st = NULL;
   }
   if (err)
      *err = filter_err;

   return st;

fail:
   if (err)
      *err = RESAMPLER_ERR_ALLOC_FAILED;
   speex_resampler_destroy(st);
   return NULL;
}

void speex_resampler_destroy(SpeexResamplerState *st)
{
   speex_free(st->mem);
   speex_free(st->sinc_table);
   speex_free(st->last_sample);
   speex_free(st->magic_samples);
   speex_free(st->samp_frac_num);
   speex_free(st);
}

static int speex_resampler_process_native(
    SpeexResamplerState *__restrict st, spx_uint32_t channel_index, 
    spx_uint32_t *in_len,
    spx_word16_t *out, spx_uint32_t *out_len)
{
   int j=0;
   const int N = st->filt_len;
   int out_sample = 0;
   spx_word16_t *mem = st->mem + channel_index * st->mem_alloc_size;
   spx_uint32_t ilen;

   st->started = 1;

   /* Call the right resampler through the function ptr */
   out_sample = st->resampler_ptr(st, channel_index, mem, in_len, out, out_len);

   if (st->last_sample[channel_index] < (spx_int32_t)*in_len)
      *in_len = st->last_sample[channel_index];
   *out_len = out_sample;
   st->last_sample[channel_index] -= *in_len;

   ilen = *in_len;

   for(j=0;j<N-1;++j)
     mem[j] = mem[j+ilen];

   return RESAMPLER_ERR_SUCCESS;
}

static int speex_resampler_magic(SpeexResamplerState *st, spx_uint32_t channel_index, spx_word16_t **out, spx_uint32_t out_len) {
   spx_uint32_t tmp_in_len = st->magic_samples[channel_index];
   spx_word16_t *mem = st->mem + channel_index * st->mem_alloc_size;
   const int N = st->filt_len;

   speex_resampler_process_native(st, channel_index, &tmp_in_len, *out, &out_len);

   st->magic_samples[channel_index] -= tmp_in_len;

   /* If we couldn't process all "magic" input samples, save the rest for next time */
   if (st->magic_samples[channel_index])
   {
      spx_uint32_t i;
      for (i=0;i<st->magic_samples[channel_index];i++)
         mem[N-1+i]=mem[N-1+i+tmp_in_len];
   }
   *out += out_len*st->out_stride;
   return out_len;
}

static inline int speex_resampler_process_float(
    SpeexResamplerState *__restrict st, spx_uint32_t channel_index, 
    const float *__restrict in, spx_uint32_t *in_len, 
    float *out, spx_uint32_t *out_len)
{
   spx_uint32_t j;
   spx_uint32_t ilen = *in_len;
   spx_uint32_t olen = *out_len;
   spx_word16_t *x = st->mem + channel_index * st->mem_alloc_size;
   const int filt_offs = st->filt_len - 1;
   const spx_uint32_t xlen = st->mem_alloc_size - filt_offs;
   const int istride = st->in_stride;
   
   if(st->resampler_ptr == NULL) return RESAMPLER_ERR_ALLOC_FAILED;

   if (st->magic_samples[channel_index])
      olen -= speex_resampler_magic(st, channel_index, &out, olen);
   if (! st->magic_samples[channel_index]) {
      while (ilen && olen) {
        spx_uint32_t ichunk = (ilen > xlen) ? xlen : ilen;
        spx_uint32_t ochunk = olen;

        if (in) {
           for(j=0;j<ichunk;++j)
              x[j+filt_offs]=in[j*istride];
        } else {
          for(j=0;j<ichunk;++j)
            x[j+filt_offs]=0;
        }
        speex_resampler_process_native(st, channel_index, &ichunk, out, &ochunk);
        ilen -= ichunk;
        olen -= ochunk;
        out += ochunk * st->out_stride;
        if (in)
           in += ichunk * istride;
      }
   }
   *in_len -= ilen;
   *out_len -= olen;
   
   return RESAMPLER_ERR_SUCCESS;
}

int speex_resampler_process_interleaved_float(
    SpeexResamplerState *__restrict st,
    const float *__restrict in, spx_uint32_t *in_len,
    float *__restrict out, spx_uint32_t *out_len)
{
   spx_uint32_t i;
   int istride_save, ostride_save;
   spx_uint32_t bak_out_len = *out_len;
   spx_uint32_t bak_in_len = *in_len;
   
   istride_save = st->in_stride;
   ostride_save = st->out_stride;
   st->in_stride = st->out_stride = st->nb_channels;
   
   if (in == NULL) return RESAMPLER_ERR_INVALID_ARG;
   if(st->resampler_ptr == NULL) return RESAMPLER_ERR_ALLOC_FAILED;
   
   for (i=0 ; i < st->nb_channels; i++)
   {
      *out_len = bak_out_len;
      *in_len = bak_in_len;
      speex_resampler_process_float(st, i, in+i, in_len, out+i, out_len);
   }
   st->in_stride = istride_save;
   st->out_stride = ostride_save;

   return RESAMPLER_ERR_SUCCESS;
}

int speex_resampler_set_rate(SpeexResamplerState *st, spx_uint32_t in_rate, spx_uint32_t out_rate)
{
   return speex_resampler_set_rate_frac(st, in_rate, out_rate, in_rate, out_rate);
}

void speex_resampler_get_rate(SpeexResamplerState *st, spx_uint32_t *in_rate, spx_uint32_t *out_rate)
{
   *in_rate = st->in_rate;
   *out_rate = st->out_rate;
}

static inline spx_uint32_t compute_gcd(spx_uint32_t a, spx_uint32_t b)
{
   while (b != 0)
   {
      spx_uint32_t temp = a;

      a = b;
      b = temp % b;
   }
   return a;
}
int speex_resampler_set_rate_frac(SpeexResamplerState *st, spx_uint32_t ratio_num, spx_uint32_t ratio_den, spx_uint32_t in_rate, spx_uint32_t out_rate)
{
   spx_uint32_t fact;
   spx_uint32_t old_den;
   spx_uint32_t i;

   if (ratio_num == 0 || ratio_den == 0)
      return RESAMPLER_ERR_INVALID_ARG;

   if (st->in_rate == in_rate && st->out_rate == out_rate && st->num_rate == ratio_num && st->den_rate == ratio_den)
      return RESAMPLER_ERR_SUCCESS;

   old_den = st->den_rate;
   st->in_rate = in_rate;
   st->out_rate = out_rate;
   st->num_rate = ratio_num;
   st->den_rate = ratio_den;

   fact = compute_gcd(st->num_rate, st->den_rate);

   st->num_rate /= fact;
   st->den_rate /= fact;

   if (old_den > 0)
   {
      for (i=0;i<st->nb_channels;i++)
      {
         if (multiply_frac(&st->samp_frac_num[i],st->samp_frac_num[i],st->den_rate,old_den) != RESAMPLER_ERR_SUCCESS)
            return RESAMPLER_ERR_OVERFLOW;
         /* Safety net */
         if (st->samp_frac_num[i] >= st->den_rate)
            st->samp_frac_num[i] = st->den_rate-1;
      }
   }

   if (st->initialised)
      return update_filter(st);
   return RESAMPLER_ERR_SUCCESS;
}

void speex_resampler_get_ratio(SpeexResamplerState *st, spx_uint32_t *ratio_num, spx_uint32_t *ratio_den)
{
   *ratio_num = st->num_rate;
   *ratio_den = st->den_rate;
}

int speex_resampler_set_quality(SpeexResamplerState *st, int quality)
{
   if (quality > SPEEX_RESAMPLER_QUALITY_MAX || quality < SPEEX_RESAMPLER_QUALITY_MIN)
      return RESAMPLER_ERR_INVALID_ARG;
   if (st->quality == quality)
      return RESAMPLER_ERR_SUCCESS;
   st->quality = quality;
   if (st->initialised)
      return update_filter(st);
   return RESAMPLER_ERR_SUCCESS;
}
void speex_resampler_get_quality(SpeexResamplerState *st, int *quality)
{
   *quality = st->quality;
}

void speex_resampler_set_input_stride(SpeexResamplerState *st, spx_uint32_t stride)
{
   st->in_stride = stride;
}

void speex_resampler_get_input_stride(SpeexResamplerState *st, spx_uint32_t *stride)
{
   *stride = st->in_stride;
}

void speex_resampler_set_output_stride(SpeexResamplerState *st, spx_uint32_t stride)
{
   st->out_stride = stride;
}

void speex_resampler_get_output_stride(SpeexResamplerState *st, spx_uint32_t *stride)
{
   *stride = st->out_stride;
}

int speex_resampler_get_input_latency(SpeexResamplerState *st)
{
  return st->filt_len / 2;
}

int speex_resampler_get_output_latency(SpeexResamplerState *st)
{
  return ((st->filt_len / 2) * st->den_rate + (st->num_rate >> 1)) / st->num_rate;
}

int speex_resampler_skip_zeros(SpeexResamplerState *st)
{
   spx_uint32_t i;
   for (i=0;i<st->nb_channels;i++)
      st->last_sample[i] = st->filt_len/2;
   return RESAMPLER_ERR_SUCCESS;
}

int speex_resampler_reset_mem(SpeexResamplerState *st)
{
   spx_uint32_t i;
   for (i=0;i<st->nb_channels;i++)
   {
      st->last_sample[i] = 0;
      st->magic_samples[i] = 0;
      st->samp_frac_num[i] = 0;
   }
   for (i=0;i<st->nb_channels*(st->filt_len-1);i++)
      st->mem[i] = 0;
   return RESAMPLER_ERR_SUCCESS;
}

const char *speex_resampler_strerror(int err)
{
   switch (err)
   {
      case RESAMPLER_ERR_SUCCESS:
         return "Success.";
      case RESAMPLER_ERR_ALLOC_FAILED:
         return "Memory allocation failed.";
      case RESAMPLER_ERR_BAD_STATE:
         return "Bad resampler state.";
      case RESAMPLER_ERR_INVALID_ARG:
         return "Invalid argument.";
      case RESAMPLER_ERR_PTR_OVERLAP:
         return "Input and output buffers overlap.";
      default:
         return "Unknown error. Bad error code or strange version mismatch.";
   }
}


void float2int_dither(void *__restrict _dst, const float *__restrict _src ,int _nsamples, char bps, char use_dithering)
{
    int i, si, silent;
    static unsigned short seed, tmpseed;
    static unsigned mute=16384;
    float Gain, r, s;
    union{
        int integer;
        char byte[3];
    } int24b;
    /* Set Correct Gain */
    if     (bps==32) Gain=OP_GAIN32;
    else if(bps==24) Gain=OP_GAIN24;
    else if(bps== 8) Gain=OP_GAIN8;
    else             Gain=OP_GAIN16;

    for(i=0; i<_nsamples; i++){
         s=_src[i];
         s*=Gain;
         si=lrintf(s);

         if(use_dithering){
             if(si == 0) silent=1;
             else        silent=0;

             if(mute<8192){ /*  ~200ms */
                 seed=RANDG(seed);
                 tmpseed=RANDG(seed);
                 r = ((int)tmpseed-(int)seed)*OP_PRNG_GAINS;
                 s+=r;
             }

             if(bps==16) {
                 short *dst16=(short *)_dst;
                 dst16[i]=(short)WORD2INT16(s);

             } else if(bps==8) {
                 unsigned char *dst8=(unsigned char *)_dst;
                 dst8[i]=0x80^WORD2INT8(s);

             } else if(bps==24){
                 char *buf_=(char *)_dst;
                 int24b.integer=WORD2INT24(s);
                 buf_[3*i+0] = int24b.byte[0];
                 buf_[3*i+1] = int24b.byte[1];
                 buf_[3*i+2] = int24b.byte[2];

             } else if (bps==32) {
                 int *buf32=(int *)_dst;
                 buf32[i]=WORD2INT32(s); //32b mode
             }
             if(!silent)mute=0;
             else mute++;
         } else { // No dithering...
             if(bps==16) {
                 short *dst16=(short *)_dst;
                 dst16[i]=(short)OP_CLAMP(-32768,si,+32767);

             } else if(bps==8) {
                 unsigned char *dst8=(unsigned char *)_dst;
                 dst8[i]=0x80^OP_CLAMP(-128,si,+127);

             } else if(bps==24){
                 char *buf_=(char *)_dst;
                 int24b.integer=OP_CLAMP(-8388608,si,+8388607);
                 buf_[3*i+0] = int24b.byte[0];
                 buf_[3*i+1] = int24b.byte[1];
                 buf_[3*i+2] = int24b.byte[2];

             } else if (bps==32) {
                 int *buf32=(int *)_dst;
                 buf32[i]=WORD2INT32(s); //32b mode
             }
         } // end if dithering/else
    }
    mute=MIN(mute, 16384);
}

inline double floorz(double n)
{
    long long i=(long long)n;
    return i>=0? i : i-1;
}
