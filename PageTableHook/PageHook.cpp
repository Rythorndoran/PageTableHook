#include <ntddk.h>
#include <cstdint>
#include <intrin.h>
#include <tuple>
#include "IA32/ia32.hpp"
#include "hde/hde.h"

namespace PageHook
{
	union virt_helper
	{
		struct pt_index
		{
			uint64_t reserved : 12;
			uint64_t pte : 9;
			uint64_t pde : 9;
			uint64_t pdpte : 9;
			uint64_t pml4e : 9;
		}index;
		uint64_t all;
	};

	static const uint8_t jmp_code[] =
	{
		0x68, 0x00, 0x00, 0x00, 0x00,					//push low 32bit +1
		0xC7, 0x44 ,0x24 ,0x04, 0x00, 0x00, 0x00, 0x00, //mov dword[rsp + 4] +9
		0xC3											//ret
	};

	static uint64_t pte_base = 0;
	static uint64_t pde_base = 0;
	static uint64_t ppe_base = 0;
	static uint64_t pxe_base = 0;

	auto phys_to_virt(std::uint64_t phys) -> void* {
		PHYSICAL_ADDRESS phys_addr = { .QuadPart = (int64_t)(phys) };
		return reinterpret_cast<void*>(MmGetVirtualForPhysical(phys_addr));
	}

	auto pfn_to_virt(std::uint64_t pfn) -> void* {
		return reinterpret_cast<void*>(phys_to_virt(pfn << 12));
	}

	auto virt_to_phys(void* virt) -> void* { return (void*)MmGetPhysicalAddress(virt).QuadPart; }

	auto virt_to_pfn(void* virt) -> std::uint64_t { return reinterpret_cast<std::uint64_t>(virt_to_phys(virt)) >> 12; }

	constexpr auto cr3_pfn(std::uint64_t _cr3) -> std::uint64_t { return ((_cr3 & 0xFFFFFFFFF000) >> 12); }

	constexpr auto page_align(std::uint64_t _virt) -> std::uint64_t { return (_virt & 0xFFFFFFFFFFFFF000); }

	constexpr auto page_offset(std::uint64_t _virt) -> std::uint64_t { return (_virt & 0xFFF); }

	void init_pte_base()
	{
		cr3 system_cr3 = { .flags = __readcr3() };
		uint64_t dirbase_phys = system_cr3.address_of_page_directory << 12;
		pt_entry_64* pt_entry = reinterpret_cast<pt_entry_64*>(phys_to_virt(dirbase_phys));
		for (uint64_t idx = 0; idx < PML4E_ENTRY_COUNT_64; idx++)
		{
			if (pt_entry[idx].page_frame_number == system_cr3.address_of_page_directory)
			{
				pte_base = (idx + 0x1FFFE00ui64) << 39ui64;
				pde_base = (idx << 30ui64) + pte_base;
				ppe_base = (idx << 30ui64) + pte_base + (idx << 21ui64);
				pxe_base = (idx << 12ui64) + ppe_base;
				break;
			}
		}
		DbgPrintEx(77, 0, "PTE_BASE:%p\n", pte_base);
	}

	auto get_pml4e(std::uint64_t virt) -> pml4e_64*
	{
		//PML4E的index*8+PXE基质
		auto pml4e_idx = (virt >> 39) & 0x1FF;
		return reinterpret_cast<pml4e_64*>((pml4e_idx << 3) + pxe_base);
	}

	auto get_pdpte(std::uint64_t virt) -> pdpte_64*
	{
		//PDPTE的index*8+PPE基质
		auto pdpte_idx = (virt >> 30) & 0x3FFFF;
		return reinterpret_cast<pdpte_64*>((pdpte_idx << 3) + ppe_base);
	}

	auto get_pde(std::uint64_t virt) -> pde_64*
	{
		//PDE的index*8+PDE基质
		auto pde_idx = (virt >> 21) & 0x7FFFFFF;
		return reinterpret_cast<pde_64*>((pde_idx << 3) + pde_base);
	}

	auto get_pte(std::uint64_t virt) -> pte_64*
	{
		//PTE的index*8+PTE基质 
		auto pte_idx = (virt >> 12) & 0xFFFFFFFFF;
		return reinterpret_cast<pte_64*>((pte_idx << 3) + pte_base);
	}

	//大页拆分小页
	auto split_large_page(pde_64* large_page) -> std::tuple<pt_entry_64*, uint64_t>
	{
		if (!large_page->large_page)
			return { nullptr,0 };
		pt_entry_64* new_pte = new pt_entry_64[PTE_ENTRY_COUNT_64];
		for (auto idx = 0; idx < PTE_ENTRY_COUNT_64; idx++)
		{
			new_pte[idx].flags = large_page->flags;
			new_pte->large_page = 0;
			new_pte->page_frame_number = large_page->page_frame_number + idx;
		}
		return { new_pte,virt_to_pfn(new_pte) };
	}

	auto split_large_page(pde_64* pde_virt, pt_entry_64* pt_virt) -> void
	{
		auto start_pfn = pde_virt->page_frame_number;
		for (int idx = 0; idx < 512; idx++)
		{
			pt_virt[idx].flags = pde_virt->flags;
			pt_virt[idx].large_page = 0;
			pt_virt[idx].page_frame_number = start_pfn + idx;
		}
	}

	auto create_pagetable() -> std::tuple <std::uint64_t, pt_entry_64*>
	{
		auto new_page = new pt_entry_64[0x200];
		RtlZeroMemory(new_page, PAGE_SIZE);
		auto pfn = virt_to_pfn(new_page);
		return { pfn,reinterpret_cast<pt_entry_64*>(new_page) };
	}

	auto create_page() -> std::tuple <std::uint64_t, void*>
	{
		auto new_page = new char[PAGE_SIZE];
		RtlZeroMemory(new_page, PAGE_SIZE);
		auto pfn = virt_to_pfn(new_page);
		return { pfn,new_page };
	}

	auto copy_page(std::uint64_t  _virt) -> std::tuple<std::uint64_t, void*>
	{
		auto new_page = new char[PAGE_SIZE];
		RtlCopyMemory(new_page, (void*)_virt, PAGE_SIZE);
		return { virt_to_pfn(new_page),new_page };
	}

	auto copy_pagetable(pt_entry_64* dst_virt, pt_entry_64* scr_virt) -> void
	{
		for (size_t idx = 0; idx < 512; idx++)
			dst_virt[idx] = scr_virt[idx];
	}

	/// <summary>
	/// 记得切换到目标进程的地址空间后再进行page hook
	/// </summary>
	/// <param name="target function pointer"></param>
	/// <param name="hook function pointer"></param>
	/// <param name="original function pointer"></param>
	/// <returns></returns>
	auto add_page_hook(void* target_function, void* hook_function, void** original_function) -> void
	{
		init_pte_base();

		//获取pxe
		auto pml4e = get_pml4e(reinterpret_cast<std::uint64_t>(target_function));
		auto pdpte = get_pdpte(reinterpret_cast<std::uint64_t>(target_function));
		auto pde = get_pde(reinterpret_cast<std::uint64_t>(target_function));

		pt_entry_64* pml4 = nullptr;
		pt_entry_64* pdpt = nullptr;
		pt_entry_64* pd = nullptr;
		pt_entry_64* pt = nullptr;

		pml4 = (pt_entry_64*)pfn_to_virt(cr3_pfn(__readcr3()));
		pdpt = (pt_entry_64*)pfn_to_virt(pml4e->page_frame_number);
		pd = (pt_entry_64*)pfn_to_virt(pdpte->page_frame_number);
		if (pde->present && !pde->large_page)
			pt = (pt_entry_64*)pfn_to_virt(pde->page_frame_number);

		//从pml4e开始构造页表
		auto [new_pdpt_pfn, new_pdpt_virt] = create_pagetable();
		copy_pagetable(new_pdpt_virt, pdpt);

		auto [new_pd_pfn, new_pd_virt] = create_pagetable();
		copy_pagetable(new_pd_virt, pd);

		auto [new_pt_pfn, new_pt_virt] = create_pagetable();
		pde->large_page ? split_large_page(pde, new_pt_virt) : copy_pagetable(new_pt_virt, pt);

		auto [hook_page_pfn, hook_page_virt] = copy_page(page_align(reinterpret_cast<std::uint64_t>(target_function)));

		//定位足够长的代码来写jmp code
		size_t code_len = 0;
		hde64s hde64_code;
		while (code_len < 14) {
			HdeDisassemble(((uint8_t*)target_function + code_len), &hde64_code);
			code_len += hde64_code.len;
		}

		//生成trampline函数
		auto trampline = new unsigned char[0x100];
		ULARGE_INTEGER jmp_to_back = { .QuadPart = (uint64_t)(target_function)+code_len };
		RtlCopyMemory(trampline, target_function, code_len);
		RtlCopyMemory(&trampline[code_len], jmp_code, sizeof(jmp_code));
		RtlCopyMemory(&trampline[code_len + 1], &jmp_to_back.LowPart, sizeof(uint32_t));
		RtlCopyMemory(&trampline[code_len + 9], &jmp_to_back.HighPart, sizeof(uint32_t));

		//在新的页面上hook
		uint64_t page_offset = (uint64_t)(target_function) & 0xFFF;
		uint8_t* hook_page = reinterpret_cast<uint8_t*>(hook_page_virt);
		ULARGE_INTEGER jmp_to_detour = { .QuadPart = (uint64_t)(hook_function) };
		RtlCopyMemory(&hook_page[page_offset], jmp_code, sizeof(jmp_code));
		RtlCopyMemory(&hook_page[page_offset + 1], &jmp_to_detour.LowPart, sizeof(uint32_t));
		RtlCopyMemory(&hook_page[page_offset + 9], &jmp_to_detour.HighPart, sizeof(uint32_t));


		virt_helper helper = { .all = reinterpret_cast<std::uint64_t>(target_function) };

		//将新的页面链接起来
		new_pdpt_virt[helper.index.pdpte].page_frame_number = new_pd_pfn;
		new_pd_virt[helper.index.pde].page_frame_number = new_pt_pfn;
		new_pd_virt[helper.index.pde].large_page = 0;
		new_pt_virt[helper.index.pte].page_frame_number = hook_page_pfn;

		//最后一步,修改pml4e
		pml4e->page_frame_number = new_pdpt_pfn;

		__invlpg(pml4e);

		*original_function = trampline;
	}
}
