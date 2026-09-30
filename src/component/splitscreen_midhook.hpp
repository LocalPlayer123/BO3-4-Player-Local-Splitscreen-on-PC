#pragma once

// Mid-function hooks: C++ at a point inside a game function.
//
// Where the game offers no function or call to hook - the value needed lives only
// in a register, or the instruction to change sits between two others - the site's
// stock bytes become a jump to a trampoline. It saves every general register and the
// flags into a `context`, calls the C++ callback with it, restores the (possibly
// changed) registers and continues at context.rip. rip starts at a copy of the stock
// bytes followed by a jump back behind the site, so a callback that changes nothing
// leaves the game as it was; a callback that replaces the stock instructions sets rip
// to the address behind the site (or anywhere else) to skip them.
//
// The copied stock bytes must be position-independent (no rip-relative operand, no
// relative branch); install() is only given sites where they are.
//
// The trampoline (built once per hook, reviewed byte by byte below):
//   lea rsp,[rsp-8]            slot for context.rip
//   pushfq ; cld               flags; the C++ ABI needs DF = 0
//   push rax,rcx,rdx,rbx,rbp,rsi,rdi,r8-r15
//   mov rax, stock_copy ; mov [rsp+0x80], rax      context.rip = the stock bytes
//   mov rcx, rsp ; mov rbx, rsp                     &context; rbx keeps rsp
//   and rsp,-16 ; sub rsp,0x80                      aligned, 0x20 home + xmm0-5
//   movups [rsp+0x20..0x70], xmm0-5                 the volatile vector registers
//   mov rax, callback ; call rax
//   movups xmm0-5, [rsp+0x20..0x70] ; mov rsp, rbx
//   pop r15-r8,rdi,rsi,rbp,rbx,rdx,rcx,rax ; popfq
//   ret                                             to context.rip (the process is
//                                                   not CET-compatible: no shadow stack)
// followed by the stock copy: <stock bytes> ; jmp [rip+0] ; dq site + len.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <vector>

namespace splitscreen::midhook
{
	// The stack image the trampoline builds; the callback reads and writes it.
	struct context
	{
		uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
		uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
		uint64_t rflags;
		uint64_t rip;   // where execution continues
	};
	static_assert(sizeof(context) == 0x88 && offsetof(context, rip) == 0x80);

	using callback = void (*)(context&);

	namespace detail
	{
		inline void put(std::vector<uint8_t>& v, std::initializer_list<uint8_t> bytes)
		{
			v.insert(v.end(), bytes);
		}

		inline void put64(std::vector<uint8_t>& v, const uint64_t x)
		{
			for (int i = 0; i < 8; ++i)
			{
				v.push_back(static_cast<uint8_t>(x >> (8 * i)));
			}
		}

		inline bool write_code(void* at, const void* bytes, const size_t n)
		{
			DWORD old{};
			if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old))
			{
				return false;
			}
			std::memcpy(at, bytes, n);
			DWORD tmp{};
			VirtualProtect(at, n, old, &tmp);
			FlushInstructionCache(GetCurrentProcess(), at, n);
			return true;
		}
	}

	// The trampoline for `fn` at `site`, whose `len` stock bytes are `stock`, to be
	// placed at `memory` (executable, within rel32 reach of the site).
	inline std::vector<uint8_t> trampoline(const uintptr_t memory, const uintptr_t site, const uint8_t* stock,
	                                       const size_t len, const callback fn)
	{
		using detail::put;
		std::vector<uint8_t> t;
		put(t, {0x48, 0x8D, 0x64, 0x24, 0xF8});                 // lea rsp, [rsp-8]
		put(t, {0x9C, 0xFC});                                   // pushfq ; cld
		put(t, {0x50, 0x51, 0x52, 0x53, 0x55, 0x56, 0x57});     // push rax,rcx,rdx,rbx,rbp,rsi,rdi
		put(t, {0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53, // push r8-r11
		        0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57}); // push r12-r15
		put(t, {0x48, 0xB8});                                   // mov rax, stock copy (patched below)
		const size_t copy_imm = t.size();
		detail::put64(t, 0);
		put(t, {0x48, 0x89, 0x84, 0x24, 0x80, 0x00, 0x00, 0x00}); // mov [rsp+0x80], rax   context.rip
		put(t, {0x48, 0x8B, 0xCC});                             // mov rcx, rsp          &context
		put(t, {0x48, 0x8B, 0xDC});                             // mov rbx, rsp
		put(t, {0x48, 0x83, 0xE4, 0xF0});                       // and rsp, -16
		put(t, {0x48, 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00});     // sub rsp, 0x80
		put(t, {0x0F, 0x11, 0x44, 0x24, 0x20});                 // movups [rsp+0x20], xmm0
		put(t, {0x0F, 0x11, 0x4C, 0x24, 0x30});                 // movups [rsp+0x30], xmm1
		put(t, {0x0F, 0x11, 0x54, 0x24, 0x40});                 // movups [rsp+0x40], xmm2
		put(t, {0x0F, 0x11, 0x5C, 0x24, 0x50});                 // movups [rsp+0x50], xmm3
		put(t, {0x0F, 0x11, 0x64, 0x24, 0x60});                 // movups [rsp+0x60], xmm4
		put(t, {0x0F, 0x11, 0x6C, 0x24, 0x70});                 // movups [rsp+0x70], xmm5
		put(t, {0x48, 0xB8});                                   // mov rax, callback
		detail::put64(t, reinterpret_cast<uint64_t>(fn));
		put(t, {0xFF, 0xD0});                                   // call rax
		put(t, {0x0F, 0x10, 0x44, 0x24, 0x20});                 // movups xmm0, [rsp+0x20]
		put(t, {0x0F, 0x10, 0x4C, 0x24, 0x30});                 // movups xmm1, [rsp+0x30]
		put(t, {0x0F, 0x10, 0x54, 0x24, 0x40});                 // movups xmm2, [rsp+0x40]
		put(t, {0x0F, 0x10, 0x5C, 0x24, 0x50});                 // movups xmm3, [rsp+0x50]
		put(t, {0x0F, 0x10, 0x64, 0x24, 0x60});                 // movups xmm4, [rsp+0x60]
		put(t, {0x0F, 0x10, 0x6C, 0x24, 0x70});                 // movups xmm5, [rsp+0x70]
		put(t, {0x48, 0x8B, 0xE3});                             // mov rsp, rbx
		put(t, {0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, // pop r15-r12
		        0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58}); // pop r11-r8
		put(t, {0x5F, 0x5E, 0x5D, 0x5B, 0x5A, 0x59, 0x58});     // pop rdi,rsi,rbp,rbx,rdx,rcx,rax
		put(t, {0x9D});                                         // popfq
		put(t, {0xC3});                                         // ret -> context.rip
		const uint64_t copy_at = memory + t.size();
		std::memcpy(t.data() + copy_imm, &copy_at, sizeof(copy_at));
		t.insert(t.end(), stock, stock + len);                  // the stock bytes
		put(t, {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});           // jmp [rip+0]
		detail::put64(t, site + len);                           //   back behind the site
		return t;
	}

	// Builds the trampoline for `site` and returns the `len` bytes that make the site
	// jump to it (`jmp rel32` + NOPs) in `patch`, without writing the site - for a
	// caller that writes several sites all-or-nothing. `alloc_near` returns executable
	// memory within rel32 reach of the game image. False (nothing usable) when the
	// site's bytes are not `stock` or the memory is out of reach.
	inline bool prepare(const uintptr_t site, const uint8_t* stock, const size_t len, const callback fn,
	                    void* (*alloc_near)(size_t), uint8_t* patch)
	{
		if (len < 5 || len > 32 || std::memcmp(reinterpret_cast<const void*>(site), stock, len) != 0)
		{
			return false;
		}
		auto* memory = static_cast<uint8_t*>(alloc_near(0x100));
		if (!memory)
		{
			return false;
		}
		const auto code = trampoline(reinterpret_cast<uintptr_t>(memory), site, stock, len, fn);
		const auto rel = static_cast<int64_t>(reinterpret_cast<intptr_t>(memory)) - static_cast<int64_t>(site + 5);
		if (code.size() > 0x100 || rel < INT32_MIN || rel > INT32_MAX
			|| !detail::write_code(memory, code.data(), code.size()))
		{
			return false;
		}
		std::memset(patch, 0x90, len);
		patch[0] = 0xE9;
		const auto rel32 = static_cast<int32_t>(rel);
		std::memcpy(patch + 1, &rel32, sizeof(rel32));
		return true;
	}

	// prepare() and the site write in one.
	inline bool install(const uintptr_t site, const uint8_t* stock, const size_t len, const callback fn,
	                    void* (*alloc_near)(size_t))
	{
		uint8_t patch[32];
		return prepare(site, stock, len, fn, alloc_near, patch)
			&& detail::write_code(reinterpret_cast<void*>(site), patch, len);
	}
}
