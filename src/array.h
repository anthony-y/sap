#ifndef CESSENTIALS_ARRAY_h
#define CESSENTIALS_ARRAY_h

#include "common.h"
#include <stdlib.h>

#define Array(T) struct {T *data; u64 elem_size; u64 length; u64 capacity;}
#define array_grow(_arr) ((_arr).capacity *= 2, (_arr).data=realloc(_arr.data, _arr.capacity*_arr.elem_size))
#define array_add(_arr, _elem) ((_arr).length+1>(_arr).capacity ? array_grow((_arr)) : 0, (_arr).data[(_arr).length++] = _elem)
#define array_init(_arr, T) ((_arr).elem_size=sizeof(T), (_arr).length=0, (_arr).capacity=32, (_arr).data=calloc((_arr).capacity,(_arr).elem_size))
#define array_free(_arr) ((_arr).length=0, (_arr).capacity=0, free((_arr).data))

#endif
