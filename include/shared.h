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
#ifndef SHARED_H
#define SHARED_H

#ifdef _WIN32
#include <windows.h>

typedef int __so_cb(void);
static __so_cb *__so_get_ctor();
static __so_cb *__so_get_dtor();

typedef void __so_func(void);
static void __so_ctor()
{
	(void)(__so_get_ctor())();
}

static void __so_dtor()
{
	(void)(__so_get_dtor())();
}

#pragma data_seg(".CRT$XIU")
static __so_func *__autostart[] = { (__so_func *)__so_ctor };
#pragma data_seg(".CRT$XPU")
static __so_func *__autoexit[] = { (__so_func *)__so_dtor };
#pragma data_seg()

#define SHARED_OBJECT_API_ENTRY(ctor, dtor)\
   static __so_cb *__so_get_ctor() { return (__so_cb*)ctor; }\
   static __so_cb *__so_get_dtor() { return (__so_cb*)dtor; }

#else //better be gcc

#define SHARED_OBJECT_API_ENTRY(ctor, dtor)\
__attribute__((constructor)) \
static void __ctor__##ctor(void) {\
   (void)ctor();\
}\
\
__attribute__((destructor))\
static void  __dtor__##dtor(void) {\
   (void)dtor();\
}

#endif //ifdef _WIN32

#endif // SHARED_H
