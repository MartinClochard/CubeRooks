
#ifndef BITSET_H
#define BITSET_H

#include <cstddef>
#include <cinttypes>

/* bitset type. */
typedef uint8_t bitset;
/* dimensions for bitset type. This should be enough for
   the largest bitset types (uint64_t) as well. */
typedef int8_t dims;

//ffs over the integral bitset type.
#ifdef FAST_FFS

//The pre-processor do not want to hear about sizeof, let's do that with
//templates.
template < bool b , typename T , typename E > struct IF;
template < typename T , typename E > struct IF<true,T,E> {
  typedef typename T::res res;
};
template < typename T , typename E > struct IF<false,T,E> {
  typedef typename E::res res;
};

class make_ffs {
private:
  struct ffs_int {
    typedef ffs_int res;
    typedef unsigned int input_type;
    static inline int ffs(input_type x) { return __builtin_ffs(x); }
  };

  struct ffsl_int {
    typedef ffsl_int res;
    typedef unsigned long input_type;
    static inline int ffs(input_type x) { return __builtin_ffsl(x); }
  };

  struct ffsll_int {
    typedef ffsll_int res;
    typedef unsigned long long input_type;
    static inline int ffs(input_type x) { return __builtin_ffsll(x); }
  };
  
  struct ffs_failure {};
  typedef typename
    IF<sizeof(bitset) <= sizeof(ffs_int::input_type),
      ffs_int,
      IF<sizeof(bitset) <= sizeof(ffsl_int::input_type),
         ffsl_int,
         IF<sizeof(bitset) <= sizeof(ffsll_int::input_type),
            ffsll_int,
            ffs_failure> > >::res ffs_type;
public:  
  static inline int ffs(ffs_type::input_type x) {
    return ffs_type::ffs(x);
  }
};

#define FFS_BITSET(lhint,x) make_ffs::ffs(x)
#else
#define FFS_BITSET(lhint,x) ( ([](int _lw,bitset b) {\
    if(b != 0) {\
      int lw = _lw;\
      bitset _1 = static_cast<bitset>(1);\
      /* Horrible. */\
      while(!(b & (_1 << lw++))) {}\
      return lw;\
    } else {\
      return 0;\
    }\
  })((lhint),(x)) )
#endif

#endif

