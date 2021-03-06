/*
* Copyright (c) 2017, The Linux Foundation. All rights reserved.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _APPS_MEM_SKEL_H
#define _APPS_MEM_SKEL_H
#include "apps_mem.h"
#include "remote.h"
#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdlib.h>
#include <stdint.h>

typedef struct _heap _heap;
struct _heap
{
	_heap *pPrev;
	const char *loc;
	uint64_t buf;
};

typedef struct allocator
{
	_heap *pheap;
	uint8_t *stack;
	uint8_t *stackEnd;
	int nSize;
} allocator;

static __inline int _heap_alloc(_heap **ppa, const char *loc, int size, void **ppbuf)
{
	_heap *pn = 0;
	pn = malloc(size + sizeof(_heap) - sizeof(uint64_t));
	if (pn != 0)
	{
		pn->pPrev = *ppa;
		pn->loc = loc;
		*ppa = pn;
		*ppbuf = (void *) & (pn->buf);
		return 0;
	}
	else
	{
		return -1;
	}
}
#define _ALIGN_SIZE(x, y) (((x) + (y-1)) & ~(y-1))


static __inline int allocator_alloc(allocator *me,
                                    const char *loc,
                                    int size,
                                    unsigned int al,
                                    void **ppbuf)
{
	if (size < 0)
	{
		return -1;
	}
	else if (size == 0)
	{
		*ppbuf = 0;
		return 0;
	}
	if ((_ALIGN_SIZE((uintptr_t)me->stackEnd, al) + size) < (uintptr_t)me->stack + me->nSize)
	{
		*ppbuf = (uint8_t *)_ALIGN_SIZE((uintptr_t)me->stackEnd, al);
		me->stackEnd = (uint8_t *)_ALIGN_SIZE((uintptr_t)me->stackEnd, al) + size;
		return 0;
	}
	else
	{
		return _heap_alloc(&me->pheap, loc, size, ppbuf);
	}
}


static __inline void allocator_deinit(allocator *me)
{
	_heap *pa = me->pheap;
	while (pa != 0)
	{
		_heap *pn = pa;
		const char *loc = pn->loc;
		(void)loc;
		pa = pn->pPrev;
		free(pn);
	}
}

static __inline void allocator_init(allocator *me, uint8_t *stack, int stackSize)
{
	me->stack =  stack;
	me->stackEnd =  stack + stackSize;
	me->nSize = stackSize;
	me->pheap = 0;
}


#endif // ALLOCATOR_H

#ifndef SLIM_H
#define SLIM_H

#include <stdint.h>

//a C data structure for the idl types that can be used to implement
//static and dynamic language bindings fairly efficiently.
//
//the goal is to have a minimal ROM and RAM footprint and without
//doing too many allocations.  A good way to package these things seemed
//like the module boundary, so all the idls within  one module can share
//all the type references.


#define PARAMETER_IN       0x0
#define PARAMETER_OUT      0x1
#define PARAMETER_INOUT    0x2
#define PARAMETER_ROUT     0x3
#define PARAMETER_INROUT   0x4

//the types that we get from idl
#define TYPE_OBJECT             0x0
#define TYPE_INTERFACE          0x1
#define TYPE_PRIMITIVE          0x2
#define TYPE_ENUM               0x3
#define TYPE_STRING             0x4
#define TYPE_WSTRING            0x5
#define TYPE_STRUCTURE          0x6
#define TYPE_UNION              0x7
#define TYPE_ARRAY              0x8
#define TYPE_SEQUENCE           0x9

//these require the pack/unpack to recurse
//so it's a hint to those languages that can optimize in cases where
//recursion isn't necessary.
#define TYPE_COMPLEX_STRUCTURE  (0x10 | TYPE_STRUCTURE)
#define TYPE_COMPLEX_UNION      (0x10 | TYPE_UNION)
#define TYPE_COMPLEX_ARRAY      (0x10 | TYPE_ARRAY)
#define TYPE_COMPLEX_SEQUENCE   (0x10 | TYPE_SEQUENCE)


typedef struct Type Type;

#define INHERIT_TYPE\
   int32_t nativeSize;                /*in the simple case its the same as wire size and alignment*/\
   union {\
      struct {\
         const uintptr_t         p1;\
         const uintptr_t         p2;\
      } _cast;\
      struct {\
         uint32_t  iid;\
         uint32_t  bNotNil;\
      } object;\
      struct {\
         const Type  *arrayType;\
         int32_t      nItems;\
      } array;\
      struct {\
         const Type *seqType;\
         int32_t      nMaxLen;\
      } seqSimple; \
      struct {\
         uint32_t bFloating;\
         uint32_t bSigned;\
      } prim; \
      const SequenceType* seqComplex;\
      const UnionType  *unionType;\
      const StructType *structType;\
      int32_t         stringMaxLen;\
      uint8_t        bInterfaceNotNil;\
   } param;\
   uint8_t    type;\
   uint8_t    nativeAlignment\

typedef struct UnionType UnionType;
typedef struct StructType StructType;
typedef struct SequenceType SequenceType;
struct Type
{
	INHERIT_TYPE;
};

struct SequenceType
{
	const Type          *seqType;
	uint32_t               nMaxLen;
	uint32_t               inSize;
	uint32_t               routSizePrimIn;
	uint32_t               routSizePrimROut;
};

//byte offset from the start of the case values for
//this unions case value array.  it MUST be aligned
//at the alignment requrements for the descriptor
//
//if negative it means that the unions cases are
//simple enumerators, so the value read from the descriptor
//can be used directly to find the correct case
typedef union CaseValuePtr CaseValuePtr;
union CaseValuePtr
{
	const uint8_t   *value8s;
	const uint16_t  *value16s;
	const uint32_t  *value32s;
	const uint64_t  *value64s;
};

//these are only used in complex cases
//so I pulled them out of the type definition as references to make
//the type smaller
struct UnionType
{
	const Type           *descriptor;
	uint32_t               nCases;
	const CaseValuePtr   caseValues;
	const Type *const   *cases;
	int32_t               inSize;
	int32_t               routSizePrimIn;
	int32_t               routSizePrimROut;
	uint8_t                inAlignment;
	uint8_t                routAlignmentPrimIn;
	uint8_t                routAlignmentPrimROut;
	uint8_t                inCaseAlignment;
	uint8_t                routCaseAlignmentPrimIn;
	uint8_t                routCaseAlignmentPrimROut;
	uint8_t                nativeCaseAlignment;
	uint8_t              bDefaultCase;
};

struct StructType
{
	uint32_t               nMembers;
	const Type *const   *members;
	int32_t               inSize;
	int32_t               routSizePrimIn;
	int32_t               routSizePrimROut;
	uint8_t                inAlignment;
	uint8_t                routAlignmentPrimIn;
	uint8_t                routAlignmentPrimROut;
};

typedef struct Parameter Parameter;
struct Parameter
{
	INHERIT_TYPE;
	uint8_t    mode;
	uint8_t  bNotNil;
};

#define SLIM_SCALARS_IS_DYNAMIC(u) (((u) & 0x00ffffff) == 0x00ffffff)

typedef struct Method Method;
struct Method
{
	uint32_t                    uScalars;            //no method index
	int32_t                     primInSize;
	int32_t                     primROutSize;
	int                         maxArgs;
	int                         numParams;
	const Parameter *const     *params;
	uint8_t                       primInAlignment;
	uint8_t                       primROutAlignment;
};

typedef struct Interface Interface;

struct Interface
{
	int                            nMethods;
	const Method   *const          *methodArray;
	int                            nIIds;
	const uint32_t                   *iids;
	const uint16_t                  *methodStringArray;
	const uint16_t                  *methodStrings;
	const char                    *strings;
};


#endif //SLIM_H


#ifndef _APPS_MEM_SLIM_H
#define _APPS_MEM_SLIM_H
#include "remote.h"
#include <stdint.h>

#ifndef __QAIC_SLIM
#define __QAIC_SLIM(ff) ff
#endif
#ifndef __QAIC_SLIM_EXPORT
#define __QAIC_SLIM_EXPORT
#endif

static const Parameter parameters[6] = {{0x4, {{(const uintptr_t)0, (const uintptr_t)1}}, 2, 0x4, 0, 0}, {0x4, {{(const uintptr_t)0, (const uintptr_t)0}}, 2, 0x4, 0, 0}, {0x4, {{(const uintptr_t)0, (const uintptr_t)0}}, 2, 0x4, 3, 0}, {0x8, {{(const uintptr_t)0, (const uintptr_t)0}}, 2, 0x8, 0, 0}, {0x8, {{(const uintptr_t)0, (const uintptr_t)1}}, 2, 0x8, 0, 0}, {0x8, {{(const uintptr_t)0, (const uintptr_t)0}}, 2, 0x8, 3, 0}};
static const Parameter *const parameterArrays[14] = {(&(parameters[0])), (&(parameters[1])), (&(parameters[1])), (&(parameters[3])), (&(parameters[4])), (&(parameters[5])), (&(parameters[5])), (&(parameters[0])), (&(parameters[1])), (&(parameters[1])), (&(parameters[1])), (&(parameters[0])), (&(parameters[2])), (&(parameters[2]))};
static const Method methods[4] = {{REMOTE_SCALARS_MAKEX(0, 0, 0x1, 0x1, 0x0, 0x0), 0x14, 0x8, 7, 7, (&(parameterArrays[7])), 0x4, 0x4}, {REMOTE_SCALARS_MAKEX(0, 0, 0x1, 0x0, 0x0, 0x0), 0x8, 0x0, 2, 2, (&(parameterArrays[10])), 0x4, 0x0}, {REMOTE_SCALARS_MAKEX(0, 0, 0x1, 0x1, 0x0, 0x0), 0x20, 0x10, 11, 7, (&(parameterArrays[0])), 0x8, 0x8}, {REMOTE_SCALARS_MAKEX(0, 0, 0x1, 0x0, 0x0, 0x0), 0x10, 0x0, 6, 2, (&(parameterArrays[3])), 0x8, 0x0}};
static const Method *const methodArrays[4] = {&(methods[0]), &(methods[1]), &(methods[2]), &(methods[3])};
static const char strings[100] = "request_unmap64\0request_map64\0request_unmap\0request_map\0ion_flags\0rflags\0heapid\0vadsp\0vapps\0len\0vin\0";
static const uint16_t methodStrings[22] = {16, 73, 56, 66, 96, 92, 86, 80, 44, 73, 56, 66, 96, 92, 86, 80, 0, 80, 92, 30, 80, 92};
static const uint16_t methodStringsArrays[4] = {8, 19, 0, 16};
__QAIC_SLIM_EXPORT const Interface __QAIC_SLIM(apps_mem_slim) = {4, &(methodArrays[0]), 0, 0, &(methodStringsArrays [0]), methodStrings, strings};
#endif //_APPS_MEM_SLIM_H
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#ifndef __QAIC_REMOTE
#define __QAIC_REMOTE(ff) ff
#endif //__QAIC_REMOTE

#ifndef __QAIC_HEADER
#define __QAIC_HEADER(ff) ff
#endif //__QAIC_HEADER

#ifndef __QAIC_HEADER_EXPORT
#define __QAIC_HEADER_EXPORT
#endif // __QAIC_HEADER_EXPORT

#ifndef __QAIC_HEADER_ATTRIBUTE
#define __QAIC_HEADER_ATTRIBUTE
#endif // __QAIC_HEADER_ATTRIBUTE

#ifndef __QAIC_IMPL
#define __QAIC_IMPL(ff) ff
#endif //__QAIC_IMPL

#ifndef __QAIC_IMPL_EXPORT
#define __QAIC_IMPL_EXPORT
#endif // __QAIC_IMPL_EXPORT

#ifndef __QAIC_IMPL_ATTRIBUTE
#define __QAIC_IMPL_ATTRIBUTE
#endif // __QAIC_IMPL_ATTRIBUTE

#ifndef __QAIC_STUB
#define __QAIC_STUB(ff) ff
#endif //__QAIC_STUB

#ifndef __QAIC_STUB_EXPORT
#define __QAIC_STUB_EXPORT
#endif // __QAIC_STUB_EXPORT

#ifndef __QAIC_STUB_ATTRIBUTE
#define __QAIC_STUB_ATTRIBUTE
#endif // __QAIC_STUB_ATTRIBUTE

#ifndef __QAIC_SKEL
#define __QAIC_SKEL(ff) ff
#endif //__QAIC_SKEL__

#ifndef __QAIC_SKEL_EXPORT
#define __QAIC_SKEL_EXPORT
#endif // __QAIC_SKEL_EXPORT

#ifndef __QAIC_SKEL_ATTRIBUTE
#define __QAIC_SKEL_ATTRIBUTE
#endif // __QAIC_SKEL_ATTRIBUTE

#ifdef __QAIC_DEBUG__
#ifndef __QAIC_DBG_PRINTF__
#define __QAIC_DBG_PRINTF__( ee ) do { printf ee ; } while(0)
#endif
#else
#define __QAIC_DBG_PRINTF__( ee ) (void)0
#endif


#define _OFFSET(src, sof)  ((void*)(((char*)(src)) + (sof)))

#define _COPY(dst, dof, src, sof, sz)  \
   do {\
         struct __copy { \
            char ar[sz]; \
         };\
         *(struct __copy*)_OFFSET(dst, dof) = *(struct __copy*)_OFFSET(src, sof);\
   } while (0)

#define _ASSIGN(dst, src, sof)  \
   do {\
      dst = OFFSET(src, sof); \
   } while (0)

#define _STD_STRLEN_IF(str) (str == 0 ? 0 : strlen(str))

#include "AEEStdErr.h"

#define _TRY(ee, func) \
   do { \
      if (AEE_SUCCESS != ((ee) = func)) {\
         __QAIC_DBG_PRINTF__((__FILE_LINE__  ": error: %d\n", (int)(ee)));\
         goto ee##bail;\
      } \
   } while (0)

#define _CATCH(exception) exception##bail: if (exception != AEE_SUCCESS)

#define _ASSERT(nErr, ff) _TRY(nErr, 0 == (ff) ? AEE_EBADPARM : AEE_SUCCESS)

#ifdef __QAIC_DEBUG__
#define _ALLOCATE(nErr, pal, size, alignment, pv) _TRY(nErr, allocator_alloc(pal, __FILE_LINE__, size, alignment, (void**)&pv))
#else
#define _ALLOCATE(nErr, pal, size, alignment, pv) _TRY(nErr, allocator_alloc(pal, 0, size, alignment, (void**)&pv))
#endif


#ifdef __cplusplus
extern "C" {
#endif
static __inline int _skel_method(int (*_pfn)(uint64_t, uint64_t), uint32_t _sc, remote_arg *_pra)
{
	remote_arg *_praEnd;
	uint64_t _in0[1];
	uint64_t _in1[1];
	uint64_t *_primIn;
	int _nErr = 0;
	_praEnd = ((_pra + REMOTE_SCALARS_INBUFS(_sc)) + REMOTE_SCALARS_OUTBUFS(_sc));
	_ASSERT(_nErr, (_pra + 1) <= _praEnd);
	_ASSERT(_nErr, _pra[0].buf.nLen >= 16);
	_primIn = _pra[0].buf.pv;
	_COPY(_in0, 0, _primIn, 0, 8);
	_COPY(_in1, 0, _primIn, 8, 8);
	_TRY(_nErr, _pfn(*_in0, *_in1));
	_CATCH(_nErr) {}
	return _nErr;
}
static __inline int _skel_method_1(int (*_pfn)(uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t *, uint64_t *), uint32_t _sc, remote_arg *_pra)
{
	remote_arg *_praEnd;
	uint32_t _in0[1];
	uint32_t _in1[1];
	uint32_t _in2[1];
	uint64_t _in3[1];
	uint64_t _in4[1];
	uint64_t _rout5[1];
	uint64_t _rout6[1];
	uint64_t *_primIn;
	int _numIn[1];
	uint64_t *_primROut;
	int _nErr = 0;
	_praEnd = ((_pra + REMOTE_SCALARS_INBUFS(_sc)) + REMOTE_SCALARS_OUTBUFS(_sc));
	_ASSERT(_nErr, (_pra + 2) <= _praEnd);
	_numIn[0] = (REMOTE_SCALARS_INBUFS(_sc) - 1);
	_ASSERT(_nErr, _pra[0].buf.nLen >= 32);
	_primIn = _pra[0].buf.pv;
	_ASSERT(_nErr, _pra[(_numIn[0] + 1)].buf.nLen >= 16);
	_primROut = _pra[(_numIn[0] + 1)].buf.pv;
	_COPY(_in0, 0, _primIn, 0, 4);
	_COPY(_in1, 0, _primIn, 4, 4);
	_COPY(_in2, 0, _primIn, 8, 4);
	_COPY(_in3, 0, _primIn, 16, 8);
	_COPY(_in4, 0, _primIn, 24, 8);
	_TRY(_nErr, _pfn(*_in0, *_in1, *_in2, *_in3, *_in4, _rout5, _rout6));
	_COPY(_primROut, 0, _rout5, 0, 8);
	_COPY(_primROut, 8, _rout6, 0, 8);
	_CATCH(_nErr) {}
	return _nErr;
}
static __inline int _skel_method_2(int (*_pfn)(uint32_t, uint32_t), uint32_t _sc, remote_arg *_pra)
{
	remote_arg *_praEnd;
	uint32_t _in0[1];
	uint32_t _in1[1];
	uint32_t *_primIn;
	int _nErr = 0;
	_praEnd = ((_pra + REMOTE_SCALARS_INBUFS(_sc)) + REMOTE_SCALARS_OUTBUFS(_sc));
	_ASSERT(_nErr, (_pra + 1) <= _praEnd);
	_ASSERT(_nErr, _pra[0].buf.nLen >= 8);
	_primIn = _pra[0].buf.pv;
	_COPY(_in0, 0, _primIn, 0, 4);
	_COPY(_in1, 0, _primIn, 4, 4);
	_TRY(_nErr, _pfn(*_in0, *_in1));
	_CATCH(_nErr) {}
	return _nErr;
}
static __inline int _skel_method_3(int (*_pfn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t *, uint32_t *), uint32_t _sc, remote_arg *_pra)
{
	remote_arg *_praEnd;
	uint32_t _in0[1];
	uint32_t _in1[1];
	uint32_t _in2[1];
	uint32_t _in3[1];
	uint32_t _in4[1];
	uint32_t _rout5[1];
	uint32_t _rout6[1];
	uint32_t *_primIn;
	int _numIn[1];
	uint32_t *_primROut;
	int _nErr = 0;
	_praEnd = ((_pra + REMOTE_SCALARS_INBUFS(_sc)) + REMOTE_SCALARS_OUTBUFS(_sc));
	_ASSERT(_nErr, (_pra + 2) <= _praEnd);
	_numIn[0] = (REMOTE_SCALARS_INBUFS(_sc) - 1);
	_ASSERT(_nErr, _pra[0].buf.nLen >= 20);
	_primIn = _pra[0].buf.pv;
	_ASSERT(_nErr, _pra[(_numIn[0] + 1)].buf.nLen >= 8);
	_primROut = _pra[(_numIn[0] + 1)].buf.pv;
	_COPY(_in0, 0, _primIn, 0, 4);
	_COPY(_in1, 0, _primIn, 4, 4);
	_COPY(_in2, 0, _primIn, 8, 4);
	_COPY(_in3, 0, _primIn, 12, 4);
	_COPY(_in4, 0, _primIn, 16, 4);
	_TRY(_nErr, _pfn(*_in0, *_in1, *_in2, *_in3, *_in4, _rout5, _rout6));
	_COPY(_primROut, 0, _rout5, 0, 4);
	_COPY(_primROut, 4, _rout6, 0, 4);
	_CATCH(_nErr) {}
	return _nErr;
}
__QAIC_SKEL_EXPORT int __QAIC_SKEL(apps_mem_skel_invoke)(uint32_t _sc, remote_arg *_pra) __QAIC_SKEL_ATTRIBUTE
{
	switch (REMOTE_SCALARS_METHOD(_sc))
	{
		case 0:
			return _skel_method_3((void *)__QAIC_IMPL(apps_mem_request_map), _sc, _pra);
		case 1:
			return _skel_method_2((void *)__QAIC_IMPL(apps_mem_request_unmap), _sc, _pra);
		case 2:
			return _skel_method_1((void *)__QAIC_IMPL(apps_mem_request_map64), _sc, _pra);
		case 3:
			return _skel_method((void *)__QAIC_IMPL(apps_mem_request_unmap64), _sc, _pra);
	}
	return AEE_EUNSUPPORTED;
}
#ifdef __cplusplus
}
#endif
#endif //_APPS_MEM_SKEL_H
