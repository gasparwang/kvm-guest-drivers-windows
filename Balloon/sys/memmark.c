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

File: Memtag.c 
  This file contains balloon memtag & memuntag routines.

Contact Information:
info-linux <info@baibantech.com.cn>
*/

#include "precomp.h"
#include "virtio_pci_common.h"
#include "memmark.h"
#include "atomic64.h"

#define HOST_MEM_POOL_REG ((PULONG) 0xb000)
#define PC_RESERVE_MEM_SIZE (0x4000000ULL)

UCHAR zero_page [PAGE_SIZE];
struct virt_mem_pool * guest_mem_pool = NULL;


static void init_guest_mem_pool(void *addr,int len)
{
	struct virt_mem_pool *pool = addr;
	memset(addr,0,len);
	pool->magic= 0xABABABABABABABAB;
	pool->pool_id = -1;
	pool->desc_max = (len - sizeof(struct virt_mem_pool))/sizeof(unsigned long long);
}

VOID printk(const char *fmt, ...)
{
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_HW_ACCESS, fmt, __VA_ARGS__);
}

void print_virt_mem_pool(struct virt_mem_pool *pool)
{
	printk("magic is 0x%llx\r\n",pool->magic);
	printk("pool_id is %d\r\n",pool->pool_id);
	printk("mem_ind  is %s\r\n",pool->mem_ind);
	printk("hva is 0x%llx\r\n",pool->hva);
	printk("args mm  is %p\r\n",pool->reserved_args [0]);
	printk("args vma  is %p\r\n",pool->reserved_args [1]);
	printk("args task  is %p\r\n",pool->reserved_args [2]);
	printk("args kvm  is %p\r\n",pool->reserved_args [3]);
	printk("alloc id is %llx\r\n",pool->alloc_idx);
	printk("desc_max is %llx\r\n",pool->desc_max);
	printk("debug r begin is %lld\r\n",pool->debug_r_begin);
	printk("debug r end is %lld\r\n",pool->debug_r_end);
	printk("debug a begin is %lld\r\n",pool->debug_a_begin);
	printk("debug a begin is %lld\r\n",pool->debug_a_end);
	printk("mark release ok  is %lld\r\n",pool->mark_release_ok);
	printk("mark release err conflict  is %lld\r\n",pool->mark_release_err_conflict);
	printk("mark release err state  is %lld\r\n",pool->mark_release_err_state);
	printk("mark alloc ok  is %lld\r\n",pool->mark_alloc_ok);
	printk("mark alloc err conflict  is %lld\r\n",pool->mark_alloc_err_conflict);
	printk("mark alloc err state  is %lld\r\n",pool->mark_alloc_err_state);
}


static int virt_mark_page_release (struct virt_release_mark *mark, PFN_NUMBER pfn)
{
	int pool_id ;
	unsigned long long alloc_id;
	unsigned long long state;
	unsigned long long idx ;
	
	if(!guest_mem_pool)
	{
		return -1;
	}
	atomic64_add(1,(atomic64_t*)&guest_mem_pool->debug_r_begin);
	pool_id = guest_mem_pool->pool_id;
	alloc_id = atomic64_add_return(1,(atomic64_t*)&guest_mem_pool->alloc_idx)-1;
	idx = alloc_id%guest_mem_pool->desc_max;
	state = guest_mem_pool->desc[idx];
	
	if(0 == state)
	{
		if(0 != atomic64_cmpxchg((atomic64_t*)&guest_mem_pool->desc[idx],0,pfn))
		{
			pool_id = guest_mem_pool->pool_max +1;
			idx = guest_mem_pool->desc_max +1;
			atomic64_add(1,(atomic64_t*)&guest_mem_pool->mark_release_err_conflict);
		}
		else
		{
			atomic64_add(1,(atomic64_t*)&guest_mem_pool->mark_release_ok);
		}
		mark->pool_id = pool_id;
		mark->alloc_id = idx;
		MemoryBarrier ();
		mark->desc = guest_mem_pool->mem_ind;
		MemoryBarrier ();

		atomic64_add(1,(atomic64_t*)&guest_mem_pool->debug_r_end);
		return 0;
	}
	else
	{
		mark->pool_id = guest_mem_pool->pool_max +1;
		mark->alloc_id = guest_mem_pool->desc_max +1;
		MemoryBarrier ();
		mark->desc = guest_mem_pool->mem_ind;
		MemoryBarrier ();
	}
	atomic64_add(1,(atomic64_t*)&guest_mem_pool->mark_release_err_state);
	atomic64_add(1,(atomic64_t*)&guest_mem_pool->debug_r_end);
	return -1;
}

int virt_mark_page_alloc (struct virt_release_mark *mark, PFN_NUMBER pfn)
{
	int pool_id ;
	unsigned long long alloc_id;
	unsigned long long guest_pfn;
	unsigned long long idx ;
		
	if(!guest_mem_pool)
	{
		return 0;
	}

	atomic64_add(1,(atomic64_t*)&guest_mem_pool->debug_a_begin);

	if(0 == strcmp(mark->desc,guest_mem_pool->mem_ind))
	{
		if(mark->pool_id == guest_mem_pool->pool_id)
		{
			if(mark->alloc_id < guest_mem_pool->desc_max)
			{
				idx = mark->alloc_id;
				guest_pfn = guest_mem_pool->desc[mark->alloc_id];
				if(guest_pfn == pfn)
				{
					if(guest_pfn == atomic64_cmpxchg((atomic64_t*)&guest_mem_pool->desc[idx],guest_pfn,0))
					{
						atomic64_add(1,(atomic64_t*)&guest_mem_pool->mark_alloc_ok);
						atomic64_add(1,(atomic64_t*)&guest_mem_pool->debug_a_end);
					}
					else
					{
						while(mark->desc[0] != '0')
						{
							MemoryBarrier ();
						}
					}
				}
			}
		}
		memset(mark,0,sizeof(struct virt_release_mark));
		MemoryBarrier ();
		return 0;
	}
	else
	{
	}
	atomic64_add(1,(atomic64_t*)&guest_mem_pool->mark_alloc_err_state);
	atomic64_add(1,(atomic64_t*)&guest_mem_pool->debug_a_end);
	return -1;
}


VOID MemMarkSet (
	PDEVICE_CONTEXT ctx,
	PMDL pMdl
	)
{
	PUCHAR pBuf = NULL;
	ULONG num;
	ULONG i = 0;
	
	pBuf = MmGetSystemAddressForMdlSafe (pMdl, NormalPagePriority);
	num = MmGetMdlByteCount(pMdl) / PAGE_SIZE;

	for (i = 0; i < num; i++)
	{
		virt_mark_page_release ((struct virt_release_mark *)(pBuf + i * PAGE_SIZE), ctx->pfns_table [i]);
	}
}

VOID MemMarkClr (
	PDEVICE_CONTEXT ctx,
	PMDL pMdl
	)
{
	PUCHAR pBuf = NULL;
	ULONG num;
	ULONG i = 0;

	pBuf = MmGetSystemAddressForMdlSafe (pMdl, NormalPagePriority);
	num = MmGetMdlByteCount(pMdl) / PAGE_SIZE;
	
	for (i = 0; i < num; i++)
	{
		virt_mark_page_alloc ((struct virt_release_mark *)(pBuf + i * PAGE_SIZE), ctx->pfns_table [i]);
	}
}
       
/*
	int virt_mem_guest_init(void)
*/
VOID MemMarkInit (
	VirtIODevice *vdev
	)
{
	volatile PVOID ptr = NULL;
	PULONG reg = HOST_MEM_POOL_REG;
	PHYSICAL_ADDRESS io_reserve_mem;
	ULONGLONG pfn = 0;
	
	pfn = READ_PORT_ULONG (reg);
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_HW_ACCESS, "Mem mark pfn addr: %llx \r\n", pfn);

	if (pfn != 0xffffffff)
	{
		io_reserve_mem.QuadPart = pfn << 12;
		ptr = MmMapIoSpace (io_reserve_mem, PC_RESERVE_MEM_SIZE, MmNonCached);
		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_HW_ACCESS, "Mem mark pool vitual addr: %p\r\n", ptr);

		if (ptr != NULL)
		{
			init_guest_mem_pool(ptr,PC_RESERVE_MEM_SIZE); 
			print_virt_mem_pool((struct virt_mem_pool *) ptr);
			WRITE_PORT_ULONG (reg, (ULONG) pfn); /* inform qemu to init guest mem mark pool. */
			print_virt_mem_pool((struct virt_mem_pool *) ptr);
			guest_mem_pool = (struct virt_mem_pool *) ptr;
		}			
	}
	else
	{
		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_HW_ACCESS, "%s: pfn is invalid! \n", __FUNCTION__);
	}

	memset (zero_page, 0, sizeof (zero_page));
}


