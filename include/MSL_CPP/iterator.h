#ifndef ITERATOR_H
#define ITERATOR_H

#include "types.h"

#ifdef __cplusplus
namespace std {

#ifdef TARGET_PC
/* On PC, don't redefine input_iterator_tag (system <iterator> may provide it),
 * but keep our simple distance() since the decomp iterators don't have
 * iterator_traits::difference_type that the system std::distance requires. */
#else
struct input_iterator_tag {};
#endif

template <class InputIterator>
inline
s32
__distance(InputIterator first, InputIterator last) {
	s32 result = 0;
	for (; first != last; ++first)
		++result;
	return result;
}

template <class InputIterator>
inline
s32
distance(InputIterator first, InputIterator last) {
	return __distance(first, last);
}

} // namespace std
#endif

#endif
