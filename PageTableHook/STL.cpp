#include <ntddk.h>
#include <cstddef>
#include <exception>
#include <vcruntime_new.h>
#pragma warning(disable: 4595)
#pragma warning(disable: 4996)
#pragma warning(disable: 4100)

[[noreturn]] DECLSPEC_NORETURN void raise_exception(unsigned long const exception_code) noexcept(false)
{
#pragma warning(disable : __WARNING_USE_OTHER_FUNCTION)
	KeBugCheck(exception_code);
#pragma warning(default : __WARNING_USE_OTHER_FUNCTION)
}

_NODISCARD void __CRTDECL _invalid_parameter_noinfo_noreturn()
{
	raise_exception(KMODE_EXCEPTION_NOT_HANDLED);
}

_NODISCARD _ACRTIMP void __CRTDECL _invoke_watson(
	_In_opt_z_ wchar_t const* const expression,
	_In_opt_z_ wchar_t const* const function_name, _In_opt_z_ wchar_t const* const file_name,
	_In_ unsigned int const line_number, _In_ uintptr_t const reserved
)
{
	raise_exception(KMODE_EXCEPTION_NOT_HANDLED);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(_Size) _VCRT_ALLOCATOR void* __CRTDECL operator new(size_t _Size)
{
	_Size = (_Size != 0) ? _Size : 1;
	auto const p = ExAllocatePool(NonPagedPool, _Size);
	if (p == nullptr)
	{
		raise_exception(MUST_SUCCEED_POOL_EMPTY);
	}
	return p;
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(_Size) _VCRT_ALLOCATOR void* __CRTDECL operator new[](size_t _Size)
{
	_Size = (_Size != 0) ? _Size : 1;
	auto const p = ExAllocatePool(NonPagedPool, _Size);
	if (p == nullptr)
	{
		raise_exception(MUST_SUCCEED_POOL_EMPTY);
	}
	return p;
}

void __CRTDECL operator delete(void* _Block) noexcept
{
	if (_Block)
	{
		ExFreePool(_Block);
	}
}

void __CRTDECL operator delete(void* _Block, size_t _Size) noexcept
{
	if (_Block)
	{
		ExFreePool(_Block);
	}
}

void __CRTDECL operator delete[](void* _Block, size_t _Size) noexcept
{
	if (_Block)
	{
		ExFreePool(_Block);
	}
}

namespace std
{
	[[noreturn]] DECLSPEC_NORETURN void __CLRCALL_PURE_OR_CDECL _Xbad_alloc()
	{
		raise_exception(KMODE_EXCEPTION_NOT_HANDLED);
	}

	[[noreturn]] DECLSPEC_NORETURN void __CLRCALL_PURE_OR_CDECL _Xinvalid_argument(_In_z_ char const*)
	{
		raise_exception(KMODE_EXCEPTION_NOT_HANDLED);
	}

	[[noreturn]] DECLSPEC_NORETURN void __CLRCALL_PURE_OR_CDECL _Xlength_error(_In_z_ char const*)
	{
		raise_exception(KMODE_EXCEPTION_NOT_HANDLED);
	}

	[[noreturn]] DECLSPEC_NORETURN void __CLRCALL_PURE_OR_CDECL _Xout_of_range(_In_z_ char const*)
	{
		raise_exception(KMODE_EXCEPTION_NOT_HANDLED);
	}

	[[noreturn]] DECLSPEC_NORETURN void __CLRCALL_PURE_OR_CDECL _Xoverflow_error(_In_z_ char const*)
	{
		raise_exception(KMODE_EXCEPTION_NOT_HANDLED);
	}

	[[noreturn]] DECLSPEC_NORETURN void __CLRCALL_PURE_OR_CDECL _Xruntime_error(_In_z_ char const*)
	{
		raise_exception(KMODE_EXCEPTION_NOT_HANDLED);
	}

	[[noreturn]] DECLSPEC_NORETURN void __CLRCALL_PURE_OR_CDECL _Xbad_function_call()
	{
		raise_exception(KMODE_EXCEPTION_NOT_HANDLED);
	}

	void(__CRTDECL* _Raise_handler)(std::exception const&);
}

using _PVFV = void(__CRTDECL*)(void); // PVFV = Pointer to Void Func(Void)
using _PIFV = int(__CRTDECL*)(void); // PIFV = Pointer to Int Func(Void)

extern "C"
{
	int _fltused = 0;

	float _ceilf(float _X)
	{
		return static_cast<float>(static_cast<int>(_X + 1));
	}

	int __CRTDECL __init_on_exit_array()
	{
		return 0;
	}

	int __CRTDECL atexit(_PVFV)
	{
		return 0;
	}
}


#define _CRTALLOC(x) __declspec(allocate(x))

#pragma section(".CRT$XIA", long, read)
_CRTALLOC(".CRT$XIA") _PIFV __xi_a[] = { 0 };
#pragma section(".CRT$XIZ", long, read)
_CRTALLOC(".CRT$XIZ") _PIFV __xi_z[] = { 0 };

// C++ initializers:
#pragma section(".CRT$XCA", long, read)
_CRTALLOC(".CRT$XCA") _PVFV __xc_a[] = { 0 };
#pragma section(".CRT$XCZ", long, read)
_CRTALLOC(".CRT$XCZ") _PVFV __xc_z[] = { 0 };

// C pre-terminators:
#pragma section(".CRT$XPA", long, read)
_CRTALLOC(".CRT$XPA") _PVFV __xp_a[] = { 0 };
#pragma section(".CRT$XPZ", long, read)
_CRTALLOC(".CRT$XPZ") _PVFV __xp_z[] = { 0 };

// C terminators:
#pragma section(".CRT$XTA", long, read)
_CRTALLOC(".CRT$XTA") _PVFV __xt_a[] = { 0 };
#pragma section(".CRT$XTZ", long, read)
_CRTALLOC(".CRT$XTZ") _PVFV __xt_z[] = { 0 };


#pragma data_seg()

#pragma comment(linker, "/merge:.CRT=.rdata")

#pragma warning(default: 4996)
#pragma warning(default: 4100)
#pragma warning(default: 4595)
