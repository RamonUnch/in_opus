#ifndef _OPUSFILE_HTTP_H
#define _OPUSFILE_HTTP_H (1)

#ifndef _LARGEFILE_SOURCE
# define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
# define _LARGEFILE64_SOURCE
#endif
#ifndef _FILE_OFFSET_BITS
#  define _FILE_OFFSET_BITS 64
#endif

#include <stdlib.h>
#include <opusfile.h>

# if OP_GNUC_PREREQ(3,0)
/*Another alternative is
    (__builtin_constant_p(_x)?!!(_x):__builtin_expect(!!(_x),1))
   but that evaluates _x multiple times, which may be bad.*/
#  define OP_LIKELY(_x) (__builtin_expect(!!(_x),1))
#  define OP_UNLIKELY(_x) (__builtin_expect(!!(_x),0))
# else
#  define OP_LIKELY(_x)   (!!(_x))
#  define OP_UNLIKELY(_x) (!!(_x))
# endif

#define OP_FATAL(_str) abort()
#define OP_ASSERT(_cond)
#define OP_ALWAYS_TRUE(_cond) ((void)(_cond))

#define OP_INT64_MAX (2*(((ogg_int64_t)1<<62)-1)|1)
#define OP_INT64_MIN (-OP_INT64_MAX-1)
#define OP_INT32_MAX (2*(((ogg_int32_t)1<<30)-1)|1)
#define OP_INT32_MIN (-OP_INT32_MAX-1)

#define OP_MIN(_a,_b)        ((_a)<(_b)?(_a):(_b))
#define OP_MAX(_a,_b)        ((_a)>(_b)?(_a):(_b))
#define OP_CLAMP(_lo,_x,_hi) (OP_MAX(_lo,OP_MIN(_x,_hi)))

/* Advance a file offset by the given amount, clamping against OP_INT64_MAX.
 * This is used to advance a known offset by things like OP_CHUNK_SIZE or
 * OP_PAGE_SIZE_MAX, while making sure to avoid signed overflow.
 * It assumes that both _offset and _amount are non-negative.
 */
#define OP_ADV_OFFSET(_offset,_amount) \
 (OP_MIN(_offset,OP_INT64_MAX-(_amount))+(_amount))

/*The maximum channel count for any mapping we'll actually decode.*/
# define OP_NCHANNELS_MAX (8)

/*Initial state.*/
# define  OP_NOTOPEN   (0)
/*We've found the first Opus stream in the first link.*/
# define  OP_PARTOPEN  (1)
# define  OP_OPENED    (2)
/*We've found the first Opus stream in the current link.*/
# define  OP_STREAMSET (3)
/*We've initialized the decoder for the chosen Opus stream in the current
   link.*/
# define  OP_INITSET   (4)

/* Information cached for a single link in a chained Ogg Opus file.
 * We choose the first Opus stream encountered in each link to play back (and
 * require at least one).
 */

int op_strncasecmp(const char *_a,const char *_b,int _n);

#endif
