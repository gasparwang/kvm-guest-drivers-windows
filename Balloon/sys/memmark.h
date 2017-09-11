/*
This file is provided under a dual BSD license. 
BSD LICENSE
Copyright(c) 2016 Baibantech Corporation.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.
* Neither the name of Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File: Memtag.h 

Contact Information:
info-linux <info@baibantech.com.cn>
*/

#ifndef MEMMARK_H
#define MEMMARK_H

struct virt_release_mark
{
	char desc[64];
	int pool_id;
	unsigned long long alloc_id;
};

struct virt_mem_pool
{
        unsigned long long magic;
        int  pool_id;
        unsigned long long mem_ind;
        unsigned long long hva;
        unsigned long long pool_max;
        struct virt_mem_args args;
        unsigned long long desc_max;
        unsigned long long alloc_idx;
        unsigned long long debug_r_begin;
        unsigned long long debug_r_end;
        unsigned long long debug_a_begin;
        unsigned long long debug_a_end;
        unsigned long long mark_release_ok;
        unsigned long long mark_release_err_conflict;
        unsigned long long mark_release_err_state;
        unsigned long long mark_alloc_ok;
        unsigned long long mark_alloc_err_conflict;
        unsigned long long mark_alloc_err_state;

        unsigned long long desc[0];
};

VOID MemMarkSet (
	PDEVICE_CONTEXT ctx,
	PMDL pMdl
	);
VOID MemMarkClr (
	PDEVICE_CONTEXT ctx,
	PMDL pMdl
	);
VOID MemMarkInit (VirtIODevice *vdev);

#endif
