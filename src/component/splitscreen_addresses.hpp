// Every game address the splitscreen component uses, for BlackOps3.exe build
// 0x06531394 (the exe ezz BOIII 3.0 runs), together with the stock and patch bytes
// of the patch tables and the layout values (strides, counts) those tables use.
// (Byte sequences that the mod assembles into its own code caves stay with that
// code in component/splitscreen/.)
// The code in component/ refers to these by name and holds no address of its own;
// release/build.ps1 refuses one. A game update ports this one file (tools/port_*.py
// translate the RVAs and re-read the expected bytes).
//
// Entries are grouped by the part of component/splitscreen/ that uses them; what
// each patch does and why is documented there, at the code that applies it.
// The reference sets (reloc_ref / signin_ref tables) came from tools/pe_xref.py,
// which scans the unpacked image byte by byte and cannot miss a rip-relative
// reference.
#pragma once

#include <cstdint>
#include <cstddef>
#include <iterator>

namespace splitscreen
{
	// ======== reference sets of the relocated per-client arrays ========

	struct reloc_ref
	{
		uint32_t insn_rva;      // start of the instruction
		uint8_t length;         // its total length
		uint8_t disp_offset;    // where the 32-bit field sits inside it
		uint32_t target_rva;    // what it currently points at
		bool rip_relative;      // true = displacement from insn end, false = ABS32
	};

	struct reloc_table
	{
		const char* name;
		uint32_t base_rva;
		uint32_t old_size;
		uint32_t new_size;
		const reloc_ref* refs;
		size_t count;
		// END-BOUND references. Some loops do not carry a count - they walk
		// `for (p = &array[0]; p < &array[N]; p++)` with BOTH addresses baked
		// in as immediates. Those refs must be rewritten to new_base +
		// NEW_SIZE, not shifted like the rest, or the array moves and every
		// loop still stops after the old number of elements.
		const reloc_ref* end_refs;
		size_t end_count;
	};

	// cg_fakeEntitiesInuseBitArray - PS4 bitarray<768>[4], [2] on PC. Slot 2 would clear 96 bytes of neighbouring globals.
	// 33 references: 8 rip-relative + 25 ABS32
	inline constexpr reloc_ref bit_array_refs[] = {
		{0x000DE43C, 7, 3, 0x04C98B80, true},
		{0x000DE4E2, 7, 3, 0x04C98B80, true},
		{0x00843A8E, 7, 3, 0x04C98B80, true},
		{0x0085D28E, 7, 3, 0x04C98B80, true},
		{0x00CFD201, 7, 3, 0x04C98B80, true},
		{0x010612CD, 7, 3, 0x04C98B80, true},
		{0x01062B55, 7, 3, 0x04C98B80, true},
		{0x02CCDEB2, 7, 3, 0x04C98B80, true},
		{0x000F2F35, 7, 3, 0x04C98B80, false},
		{0x000F2FF7, 7, 3, 0x04C98B80, false},
		{0x000F9CC3, 8, 4, 0x04C98B80, false},
		{0x000F9D7A, 7, 3, 0x04C98B80, false},
		{0x0019AE48, 8, 4, 0x04C98B80, false},
		{0x004617BA, 8, 4, 0x04C98B80, false},
		{0x007039EB, 8, 4, 0x04C98B80, false},
		{0x00703A17, 8, 4, 0x04C98B80, false},
		{0x00703A3F, 7, 3, 0x04C98B80, false},
		{0x00703A6A, 8, 4, 0x04C98B80, false},
		{0x00703A96, 8, 4, 0x04C98B80, false},
		{0x00703AC2, 8, 4, 0x04C98B80, false},
		{0x00703B25, 8, 4, 0x04C98B80, false},
		{0x00703BC5, 7, 3, 0x04C98B80, false},
		{0x00760B81, 8, 4, 0x04C98B80, false},
		{0x00FFF4F2, 8, 4, 0x04C98B80, false},
		{0x00FFF513, 7, 3, 0x04C98B80, false},
		{0x01045DAC, 8, 4, 0x04C98B80, false},
		{0x01056CC8, 8, 4, 0x04C98B80, false},
		{0x01056CE5, 7, 3, 0x04C98B80, false},
		{0x0105D815, 8, 4, 0x04C98B80, false},
		{0x01062C45, 8, 4, 0x04C98B80, false},
		{0x011122AA, 7, 3, 0x04C98B80, false},
		{0x011122D8, 7, 3, 0x04C98B80, false},
	};

	// s_storage - PS4 Storage[4] (xuid, readOnLoginProcessed[4], StorageFile files[64] stride 0x220, StorageScratch at +0x8810). Slot 2 has no loadout buffer because AllocateMemory only ever ran for two controllers.
	// 25 references: 25 rip-relative + 0 ABS32
	inline constexpr reloc_ref storage_refs[] = {
		{0x02218E37, 7, 3, 0x17897308, true},
		{0x02218F8D, 7, 3, 0x1788EAC0, true},
		{0x0221903D, 7, 3, 0x1788EAC0, true},
		{0x0221975D, 7, 3, 0x1788EAC0, true},
		{0x0221999B, 7, 3, 0x1788EAC0, true},
		{0x02219A74, 7, 3, 0x1788EAC0, true},
		{0x02219C60, 7, 3, 0x1788EAC0, true},
		{0x02219D1D, 7, 3, 0x1788EAC0, true},
		{0x02219D5F, 7, 3, 0x178972D0, true},
		{0x02219FA7, 7, 3, 0x1788EAC0, true},
		{0x0221A093, 7, 3, 0x1788EAC0, true},
		{0x0221A1C6, 7, 3, 0x1788EAC0, true},
		{0x0221A320, 7, 3, 0x1788EAC0, true},
		{0x0221A3B7, 7, 3, 0x17897311, true},
		{0x0221A405, 7, 3, 0x1788EAC0, true},
		{0x0221A495, 7, 3, 0x17897310, true},
		{0x0221A6B2, 7, 3, 0x1788EAC0, true},
		{0x0221A83D, 7, 3, 0x1788EAC0, true},
		{0x0221A9BD, 7, 3, 0x1788EAC0, true},
		{0x0221AA85, 7, 3, 0x1788EAC0, true},
		{0x0221AB22, 7, 3, 0x1788EAC0, true},
		{0x0221AC70, 7, 3, 0x1788EAC0, true},
		{0x0221AE45, 7, 3, 0x1788EAC0, true},
		{0x02EB3952, 7, 3, 0x1788EAC0, true},
		{0x02EFCD07, 7, 3, 0x1788EAC0, true},
	};

	// per-client object pointer table, accessors at 0x01EC6DC0..0x01EC70E1. Dereferenced at +0x30 by the sign-in path; slot 2 read a float constant.
	// 19 references: 19 rip-relative + 0 ABS32
	inline constexpr reloc_ref client_objs_refs[] = {
		{0x01EBA330, 7, 3, 0x03390190, true},
		{0x01EBA343, 7, 3, 0x03390190, true},
		{0x01EBA373, 7, 3, 0x03390190, true},
		{0x01EBA393, 7, 3, 0x03390190, true},
		{0x01EBA3D3, 7, 3, 0x03390190, true},
		{0x01EBA3F3, 7, 3, 0x03390190, true},
		{0x01EBA403, 7, 3, 0x03390190, true},
		{0x01EBA473, 7, 3, 0x03390190, true},
		{0x01EBA4A1, 7, 3, 0x03390190, true},
		{0x01EBA513, 7, 3, 0x03390190, true},
		{0x01EBA533, 7, 3, 0x03390190, true},
		{0x01EBA553, 7, 3, 0x03390190, true},
		{0x01EBA5A3, 7, 3, 0x03390190, true},
		{0x01EBA5C3, 7, 3, 0x03390190, true},
		{0x01EBA5F5, 7, 3, 0x03390190, true},
		{0x01EBA651, 7, 3, 0x03390190, true},
		{0x01EBA683, 7, 3, 0x03390190, true},
		{0x01EBA6A8, 7, 3, 0x03390190, true},
		{0x01EBA6D3, 7, 3, 0x03390190, true},
	};

	// end-bound references for client_objs: rewritten to base + NEW size
	inline constexpr reloc_ref client_objs_end_refs[] = {
		{0x01EBA34A, 7, 3, 0x033901A0, true},
		{0x01EBA39A, 7, 3, 0x033901A0, true},
		{0x01EBA6AF, 7, 3, 0x033901A0, true},
	};

	// userData_t[2] -> [4]. RE-ENABLED 2026-08-07. Measured at a fresh boot: controller 1 is already a signed-in secondary profile (+0x30 = 2) with storage and local-file stats readiness BEFORE it signs in, so a guest has to exist that way before Storage_Init. Run chain4 with BO3_OHNE=profile so it does not also move this table.
	// 2 references: 2 rip-relative + 0 ABS32
	inline constexpr reloc_ref client_ui_refs[] = {
		{0x02E8FC42, 7, 3, 0x14F344B0, true},
		{0x02EFA957, 7, 3, 0x14F344B0, true},
	};

	// per-local-client netchan/netstats records, stride 0x128, [2] -> [4]. This is what LobbyHost_AddLocalClients walks once its controller loop is widened past two; index 2 read past the end and faulted at 0x02173AD0.
	// 15 references: 13 rip-relative + 2 ABS32
	inline constexpr reloc_ref netchan_refs[] = {
		{0x0211AFF2, 7, 3, 0x16DEAFD0, true},
		{0x0211B5EE, 7, 3, 0x16DEAEB0, true},
		{0x0211B690, 7, 3, 0x16DEAF40, true},
		{0x0211B697, 7, 3, 0x16DEAFD0, true},
		{0x0211B7BE, 7, 3, 0x16DEAF40, true},
		{0x0211BB2D, 7, 3, 0x16DEAEB0, true},
		{0x0211BCE8, 7, 3, 0x16DEAEB0, true},
		{0x0211BF51, 7, 3, 0x16DEAEB0, true},
		{0x0211C7E8, 7, 3, 0x16DEAF40, true},
		{0x0211CA68, 7, 3, 0x16DEAEB0, false},
		{0x0211CEB3, 7, 3, 0x16DEAF40, true},
		{0x0211D025, 7, 3, 0x16DEAFD0, false},
		{0x0211D35C, 7, 3, 0x16DEAEB0, true},
		{0x0211D687, 7, 3, 0x16DEAF40, true},
		{0x0211E64E, 7, 3, 0x16DEAEB0, true},
	};

	// per-local-client object pointer table, lazily allocated 0x630 objects, [2] -> [4]. Faulting read at 0x029154E2 came from here.
	// 28 references: 28 rip-relative + 0 ABS32
	inline constexpr reloc_ref objtab_a_refs[] = {
		{0x0144A5C9, 7, 3, 0x09B35878, true},
		{0x0144A6B5, 7, 3, 0x09B35878, true},
		{0x0144A8AF, 7, 3, 0x09B35878, true},
		{0x0144A957, 7, 3, 0x09B35878, true},
		{0x0144A9D7, 7, 3, 0x09B35878, true},
		{0x0144AAC5, 7, 3, 0x09B35878, true},
		{0x0144AB77, 7, 3, 0x09B35878, true},
		{0x0144ABE7, 7, 3, 0x09B35878, true},
		{0x0144AC67, 7, 3, 0x09B35878, true},
		{0x0144ADD7, 7, 3, 0x09B35878, true},
		{0x0144B207, 7, 3, 0x09B35878, true},
		{0x0144B287, 7, 3, 0x09B35878, true},
		{0x0144B307, 7, 3, 0x09B35878, true},
		{0x0144B377, 7, 3, 0x09B35878, true},
		{0x0144B3F7, 7, 3, 0x09B35878, true},
		{0x0144B477, 7, 3, 0x09B35878, true},
		{0x0144B4E7, 7, 3, 0x09B35878, true},
		{0x0144B5C5, 7, 3, 0x09B35878, true},
		{0x0144B6C7, 7, 3, 0x09B35878, true},
		{0x0144B737, 7, 3, 0x09B35878, true},
		{0x0144B807, 7, 3, 0x09B35878, true},
		{0x0144B877, 7, 3, 0x09B35878, true},
		{0x0144B907, 7, 3, 0x09B35878, true},
		{0x0144B977, 7, 3, 0x09B35878, true},
		{0x0144B9F7, 7, 3, 0x09B35878, true},
		{0x0144BA67, 7, 3, 0x09B35878, true},
		{0x0144BAD7, 7, 3, 0x09B35878, true},
		{0x0144BB47, 7, 3, 0x09B35878, true},
	};

	// per-local-client record table written by 0x0143A4E0 (array[i].field8 = i), [2] -> [4]. An out-of-range index WRITES here.
	// 3 references: 3 rip-relative + 0 ABS32
	inline constexpr reloc_ref objtab_b_refs[] = {
		{0x0143A58D, 7, 3, 0x09931F00, true},
		{0x02D39B3B, 7, 3, 0x09931F00, true},
		{0x02EF977C, 7, 3, 0x09931F00, true},
	};

	// the two globals behind cg_entitiesArray[2] on the PC, evicted so the array can be [4] in place. 0x70 = Com_AdjustMaxFPS record base (89 readers, writers 0x00843A48/0x00856EDB), 0x78 = one movups init.
	// 92 references: 92 rip-relative + 0 ABS32
	inline constexpr reloc_ref fps_ctx_refs[] = {
		{0x001A44DC, 7, 3, 0x04C98B70, true},
		{0x001A5EEA, 7, 3, 0x04C98B70, true},
		{0x001A81EB, 7, 3, 0x04C98B70, true},
		{0x00470C6F, 7, 3, 0x04C98B70, true},
		{0x00472C42, 7, 3, 0x04C98B70, true},
		{0x0047A8B5, 7, 3, 0x04C98B70, true},
		{0x0048503D, 7, 3, 0x04C98B70, true},
		{0x00493263, 7, 3, 0x04C98B70, true},
		{0x004964A3, 7, 3, 0x04C98B70, true},
		{0x00497F61, 7, 3, 0x04C98B70, true},
		{0x0049CB31, 7, 3, 0x04C98B70, true},
		{0x004D7A22, 7, 3, 0x04C98B70, true},
		{0x00557603, 7, 3, 0x04C98B70, true},
		{0x0061BDE8, 7, 3, 0x04C98B70, true},
		{0x006349D6, 7, 3, 0x04C98B70, true},
		{0x006BEC9C, 7, 3, 0x04C98B70, true},
		{0x006C066F, 7, 3, 0x04C98B70, true},
		{0x006C268C, 7, 3, 0x04C98B70, true},
		{0x006C406C, 7, 3, 0x04C98B70, true},
		{0x006C75C5, 7, 3, 0x04C98B70, true},
		{0x006D3D55, 7, 3, 0x04C98B70, true},
		{0x006F8809, 7, 3, 0x04C98B70, true},
		{0x0071B529, 7, 3, 0x04C98B70, true},
		{0x00740309, 7, 3, 0x04C98B70, true},
		{0x0074F23C, 7, 3, 0x04C98B70, true},
		{0x00763CDC, 7, 3, 0x04C98B70, true},
		{0x00843A48, 7, 3, 0x04C98B70, true},
		{0x00853E27, 7, 3, 0x04C98B70, true},
		{0x00856EDB, 7, 3, 0x04C98B70, true},
		{0x008C99D7, 7, 3, 0x04C98B70, true},
		{0x008DC4A5, 7, 3, 0x04C98B70, true},
		{0x008EEEC4, 7, 3, 0x04C98B70, true},
		{0x008F9629, 7, 3, 0x04C98B70, true},
		{0x008FAFDA, 7, 3, 0x04C98B70, true},
		{0x00901599, 7, 3, 0x04C98B70, true},
		{0x00910765, 7, 3, 0x04C98B70, true},
		{0x0091213F, 7, 3, 0x04C98B70, true},
		{0x00913B1D, 7, 3, 0x04C98B70, true},
		{0x00918C9D, 7, 3, 0x04C98B70, true},
		{0x0092526C, 7, 3, 0x04C98B70, true},
		{0x0092B73E, 7, 3, 0x04C98B70, true},
		{0x0094CEAF, 7, 3, 0x04C98B70, true},
		{0x0097B9F3, 7, 3, 0x04C98B70, true},
		{0x00A1422F, 7, 3, 0x04C98B70, true},
		{0x00A4EA1E, 7, 3, 0x04C98B70, true},
		{0x00A66295, 7, 3, 0x04C98B70, true},
		{0x00EA43E9, 7, 3, 0x04C98B70, true},
		{0x00F2ECD8, 7, 3, 0x04C98B70, true},
		{0x00F45028, 7, 3, 0x04C98B70, true},
		{0x00F4A122, 7, 3, 0x04C98B70, true},
		{0x00F4F2EF, 7, 3, 0x04C98B70, true},
		{0x00F6B8C2, 7, 3, 0x04C98B70, true},
		{0x00F7223F, 7, 3, 0x04C98B70, true},
		{0x00F7B609, 7, 3, 0x04C98B70, true},
		{0x00F7CFD5, 7, 3, 0x04C98B70, true},
		{0x00F81B67, 7, 3, 0x04C98B70, true},
		{0x00F857C9, 7, 3, 0x04C98B70, true},
		{0x00F88F13, 7, 3, 0x04C98B70, true},
		{0x00F8DA8F, 7, 3, 0x04C98B70, true},
		{0x00F9AAB3, 7, 3, 0x04C98B70, true},
		{0x00FC3EA5, 7, 3, 0x04C98B70, true},
		{0x00FF0C35, 7, 3, 0x04C98B70, true},
		{0x00FF7196, 7, 3, 0x04C98B70, true},
		{0x01014D68, 7, 3, 0x04C98B70, true},
		{0x01019BCD, 7, 3, 0x04C98B70, true},
		{0x0104DF40, 7, 3, 0x04C98B70, true},
		{0x0105F0AB, 7, 3, 0x04C98B70, true},
		{0x010A06FF, 7, 3, 0x04C98B70, true},
		{0x010B15D5, 7, 3, 0x04C98B70, true},
		{0x010B9B19, 7, 3, 0x04C98B70, true},
		{0x010F4E65, 7, 3, 0x04C98B70, true},
		{0x01322417, 7, 3, 0x04C98B70, true},
		{0x01F1F7E6, 7, 3, 0x04C98B70, true},
		{0x01F21B20, 7, 3, 0x04C98B70, true},
		{0x01F3A8E7, 7, 3, 0x04C98B70, true},
		{0x01FBAF35, 7, 3, 0x04C98B70, true},
		{0x020021CB, 7, 3, 0x04C98B70, true},
		{0x0259BCE9, 7, 3, 0x04C98B70, true},
		{0x025A62AC, 7, 3, 0x04C98B70, true},
		{0x025BFDBC, 7, 3, 0x04C98B70, true},
		{0x025C2FA4, 7, 3, 0x04C98B70, true},
		{0x02689F35, 7, 3, 0x04C98B70, true},
		{0x026D6A24, 7, 3, 0x04C98B70, true},
		{0x026FD844, 7, 3, 0x04C98B70, true},
		{0x02735E5B, 7, 3, 0x04C98B70, true},
		{0x0273906B, 7, 3, 0x04C98B70, true},
		{0x0274ED1B, 7, 3, 0x04C98B70, true},
		{0x02765611, 7, 3, 0x04C98B70, true},
		{0x02768E8A, 7, 3, 0x04C98B70, true},
		{0x0277BF8F, 7, 3, 0x04C98B70, true},
		{0x0278D7DA, 7, 3, 0x04C98B70, true},
		{0x02CCDE42, 7, 3, 0x04C98B78, true},
	};

	// cl_voiceCommunication - PS4 voiceCommunication_t[4] elem 0xA34, [2] on the PC, packed onto clientUIActives slot 2. Widened [2] -> [4]; the vacated original is zeroed by the component so clientUIActives[2] starts as the console's initial state.
	// 12 references: 12 rip-relative + 0 ABS32
	inline constexpr reloc_ref voice_comm_refs[] = {
		{0x013E3689, 7, 3, 0x0535BCB0, true},
		{0x013E3808, 7, 3, 0x0535BCB0, true},
		{0x01EE9172, 7, 3, 0x0535BCB0, true},
		{0x01EE943B, 7, 3, 0x0535BCB0, true},
		{0x013E385F, 7, 3, 0x0535BCB1, true},
		{0x01359631, 7, 3, 0x0535C6D8, true},
		{0x013E3837, 6, 2, 0x0535C6D8, true},
		{0x013E38AC, 7, 3, 0x0535C6D8, true},
		{0x013E38D6, 7, 3, 0x0535C6D8, true},
		{0x013E38F1, 6, 2, 0x0535C6D8, true},
		{0x013E390C, 6, 2, 0x0535C6D8, true},
		{0x013E3BD4, 7, 2, 0x0535C6D8, true},
	};

	// Cbuf per-local-client records, element 0x10, [2] on the PC. Slot 2 lands on a FOREIGN qword pointer global at 0x1689DF58 (11+ loads/stores). Read as the record's 'bytes used' dword those pointer bytes are large junk, so 0x020EC489 `sub [rsi+0xC], ebx` UNDERFLOWS and 0x020EC48C movsxd sign-extends the negative result into a ~2 GB memcpy length - the 0xC0000005 at ext.dll+0x2F580 that was EVERY crash of 2026-08-18. Only 4 lea refs and no value loads/stores, so the array moves rather than the neighbour. [2] -> [4] zero-filled, which makes the engine's own `cmp [rsi+0xC], 0 / je` gate take its clean exit for an untouched client.
	// 4 references: 4 rip-relative + 0 ABS32
	inline constexpr reloc_ref cbuf_refs[] = {
		{0x020DFBC8, 7, 3, 0x1681EFB8, true},
		{0x020E003A, 7, 3, 0x1681EFB8, true},
		{0x1CC412B1, 7, 3, 0x1681EFB8, true},
		{0x020DFEE1, 7, 3, 0x1681EFC0, true},
	};

	// playersKb - PS4 kbutton_t playersKb[4][47] (dwarf_localclient.txt line 172, element 0x18); [2] x 0x498 on the PC. Cleared per local client by CL_ClearKeys (PC 0x012F34D0, PS4 0x3DF980) from the tail of CL_Disconnect. Slot 2 lands on foreign globals - 11 lea + 264 loads/stores, 219 of them to a hot qword pointer at 0x052F3320 - and the memset at 0x012F34F5 is UNGUARDED, so it zeroes them before the /GS check at 0x012F34FA fires. [2] -> [4].
	// 213 references: 95 rip-relative + 118 ABS32
	inline constexpr reloc_ref players_kb_refs[] = {
		{0x012F327D, 7, 3, 0x052739F0, true},
		{0x012F34F9, 7, 3, 0x052739F0, true},
		{0x012F719B, 7, 3, 0x052739F0, true},
		{0x012F7224, 7, 3, 0x052739F0, true},
		{0x012F72AD, 7, 3, 0x052739F0, true},
		{0x012F72E9, 7, 3, 0x052739F0, true},
		{0x012F733F, 7, 3, 0x05273C60, true},
		{0x012F734B, 7, 3, 0x05273C60, true},
		{0x012F736E, 7, 3, 0x05273C78, true},
		{0x012F737A, 7, 3, 0x05273C78, true},
		{0x012F7402, 7, 3, 0x052739F0, true},
		{0x012F7482, 7, 3, 0x052739F0, true},
		{0x012F74FD, 7, 3, 0x05273CC0, true},
		{0x012F7570, 7, 3, 0x05273CC0, true},
		{0x012F7733, 7, 3, 0x052739F0, true},
		{0x012F77CE, 7, 3, 0x052739F0, true},
		{0x012F7809, 7, 3, 0x05273B58, true},
		{0x012F7815, 7, 3, 0x05273B58, true},
		{0x012F7835, 7, 3, 0x05273B70, true},
		{0x012F7841, 7, 3, 0x05273B70, true},
		{0x012F7864, 7, 3, 0x05273B40, true},
		{0x012F7870, 7, 3, 0x05273B40, true},
		{0x012F7893, 7, 3, 0x05273C00, true},
		{0x012F789F, 7, 3, 0x05273C00, true},
		{0x012F78C2, 7, 3, 0x05273D08, true},
		{0x012F78CE, 7, 3, 0x05273D08, true},
		{0x012F78F1, 7, 3, 0x05273D20, true},
		{0x012F78FD, 7, 3, 0x05273D20, true},
		{0x012F7920, 7, 3, 0x05273D38, true},
		{0x012F792C, 7, 3, 0x05273D38, true},
		{0x012F794F, 7, 3, 0x05273D50, true},
		{0x012F795B, 7, 3, 0x05273D50, true},
		{0x012F797E, 7, 3, 0x05273D68, true},
		{0x012F798A, 7, 3, 0x05273D68, true},
		{0x012F79AD, 7, 3, 0x05273D80, true},
		{0x012F79B9, 7, 3, 0x05273D80, true},
		{0x012F79DC, 7, 3, 0x05273DC8, true},
		{0x012F79E8, 7, 3, 0x05273DC8, true},
		{0x012F7A0B, 7, 3, 0x05273D98, true},
		{0x012F7A17, 7, 3, 0x05273D98, true},
		{0x012F7A3A, 7, 3, 0x05273DB0, true},
		{0x012F7A46, 7, 3, 0x05273DB0, true},
		{0x012F7A69, 7, 3, 0x05273A20, true},
		{0x012F7A75, 7, 3, 0x05273A20, true},
		{0x012F7A98, 7, 3, 0x05273A38, true},
		{0x012F7AA4, 7, 3, 0x05273A38, true},
		{0x012F7AC7, 7, 3, 0x05273A80, true},
		{0x012F7AD3, 7, 3, 0x05273A80, true},
		{0x012F7AF6, 7, 3, 0x05273A98, true},
		{0x012F7B02, 7, 3, 0x05273A98, true},
		{0x012F7B25, 7, 3, 0x05273AF8, true},
		{0x012F7B31, 7, 3, 0x05273AF8, true},
		{0x012F7B57, 7, 3, 0x052739F0, true},
		{0x012F7B7D, 7, 3, 0x052739F0, true},
		{0x012F7B9D, 7, 3, 0x05273A08, true},
		{0x012F7BA9, 7, 3, 0x05273A08, true},
		{0x012F7BCC, 7, 3, 0x05273A50, true},
		{0x012F7BD8, 7, 3, 0x05273A50, true},
		{0x012F7BFB, 7, 3, 0x05273A68, true},
		{0x012F7C07, 7, 3, 0x05273A68, true},
		{0x012F7C2A, 7, 3, 0x05273AB0, true},
		{0x012F7C36, 7, 3, 0x05273AB0, true},
		{0x012F7C59, 7, 3, 0x05273BA0, true},
		{0x012F7C65, 7, 3, 0x05273BA0, true},
		{0x012F7CD2, 7, 3, 0x05273C90, true},
		{0x012F7CDE, 7, 3, 0x05273C90, true},
		{0x012F7D04, 7, 3, 0x052739F0, true},
		{0x012F7D29, 7, 3, 0x052739F0, true},
		{0x012F7D58, 7, 3, 0x05273CF0, true},
		{0x012F7D61, 7, 3, 0x05273CF0, true},
		{0x012F7D84, 7, 3, 0x05273E70, true},
		{0x012F7DAA, 7, 3, 0x05273E70, true},
		{0x012F9E34, 7, 3, 0x052739F0, true},
		{0x012FF4E8, 7, 3, 0x052739F0, true},
		{0x012FFB64, 7, 3, 0x052739F0, true},
		{0x013000FB, 7, 3, 0x052739F0, true},
		{0x01300167, 7, 3, 0x052739F0, true},
		{0x01303A5D, 7, 3, 0x052739F0, true},
		{0x01308881, 7, 3, 0x05273BE8, true},
		{0x013088F3, 7, 3, 0x05273BE8, true},
		{0x0130899A, 7, 3, 0x052739F0, true},
		{0x01308A04, 7, 3, 0x052739F0, true},
		{0x01308AA6, 7, 3, 0x05273C18, true},
		{0x01308D41, 7, 3, 0x05273E40, true},
		{0x0131B3F3, 7, 3, 0x05273BB8, true},
		{0x0131B480, 7, 3, 0x05273E80, true},
		{0x0131B790, 7, 3, 0x05273C00, true},
		{0x0131B7F3, 7, 3, 0x05273C00, true},
		{0x0131B936, 7, 3, 0x05273C30, true},
		{0x0131BAA3, 7, 3, 0x05273BD0, true},
		{0x0131BB5E, 7, 3, 0x05273AC8, true},
		{0x0131BBAE, 7, 3, 0x05273AC8, true},
		{0x0131C124, 7, 3, 0x052739F0, true},
		{0x0131C1DF, 7, 3, 0x052739F0, true},
		{0x0131C2C6, 7, 3, 0x05273C48, true},
		{0x012F3754, 8, 4, 0x05273D1C, false},
		{0x01306C6A, 7, 3, 0x05273BC0, false},
		{0x01306C87, 7, 3, 0x05273BD8, false},
		{0x01308B80, 8, 4, 0x05273C18, false},
		{0x01308B8C, 8, 4, 0x05273C18, false},
		{0x01308B96, 8, 4, 0x05273C1C, false},
		{0x01308BA0, 8, 4, 0x05273C1C, false},
		{0x01308BAC, 8, 4, 0x05273C1C, false},
		{0x01308BB6, 8, 4, 0x05273C28, false},
		{0x01308BC2, 8, 4, 0x05273C20, false},
		{0x01308BCA, 8, 4, 0x05273C24, false},
		{0x01308BDC, 8, 4, 0x05273C24, false},
		{0x01308BE4, 8, 4, 0x05273C2C, false},
		{0x01308BEC, 8, 4, 0x05273C28, false},
		{0x01308BF4, 8, 4, 0x05273C2A, false},
		{0x01308D84, 8, 4, 0x05273B9C, false},
		{0x01308D8C, 8, 4, 0x05273B88, false},
		{0x01308D98, 8, 4, 0x05273B8C, false},
		{0x01308DA8, 8, 4, 0x05273B88, false},
		{0x01308DB6, 8, 4, 0x05273B8C, false},
		{0x01308DBE, 8, 4, 0x05273B98, false},
		{0x01308DC8, 8, 4, 0x05273B90, false},
		{0x0131B2B1, 8, 4, 0x05273B88, false},
		{0x0131B2BD, 8, 4, 0x05273B88, false},
		{0x0131B2C7, 8, 4, 0x05273B8C, false},
		{0x0131B2D1, 8, 4, 0x05273B8C, false},
		{0x0131B2DD, 8, 4, 0x05273B8C, false},
		{0x0131B2E7, 8, 4, 0x05273B98, false},
		{0x0131B2F7, 8, 4, 0x05273B90, false},
		{0x0131B305, 8, 4, 0x05273B94, false},
		{0x0131B30D, 8, 4, 0x05273B9C, false},
		{0x0131B315, 8, 4, 0x05273B98, false},
		{0x0131B31D, 8, 4, 0x05273B9A, false},
		{0x0131B325, 8, 4, 0x05273E40, false},
		{0x0131B331, 8, 4, 0x05273E40, false},
		{0x0131B33B, 8, 4, 0x05273E44, false},
		{0x0131B345, 8, 4, 0x05273E44, false},
		{0x0131B351, 8, 4, 0x05273E44, false},
		{0x0131B35B, 8, 4, 0x05273E50, false},
		{0x0131B368, 8, 4, 0x05273E48, false},
		{0x0131B370, 8, 4, 0x05273E4C, false},
		{0x0131B37C, 8, 4, 0x05273E4C, false},
		{0x0131B384, 8, 4, 0x05273E54, false},
		{0x0131B38C, 8, 4, 0x05273E50, false},
		{0x0131B394, 8, 4, 0x05273E52, false},
		{0x0131BA10, 8, 4, 0x05273C30, false},
		{0x0131BA1C, 8, 4, 0x05273C30, false},
		{0x0131BA26, 8, 4, 0x05273C34, false},
		{0x0131BA30, 8, 4, 0x05273C34, false},
		{0x0131BA3C, 8, 4, 0x05273C34, false},
		{0x0131BA46, 8, 4, 0x05273C40, false},
		{0x0131BA52, 8, 4, 0x05273C38, false},
		{0x0131BA5A, 8, 4, 0x05273C3C, false},
		{0x0131BA6C, 8, 4, 0x05273C3C, false},
		{0x0131BA74, 8, 4, 0x05273C44, false},
		{0x0131BA7C, 8, 4, 0x05273C40, false},
		{0x0131BA84, 8, 4, 0x05273C42, false},
		{0x0131BC24, 8, 4, 0x05273CEC, false},
		{0x0131BC2C, 8, 4, 0x05273CD8, false},
		{0x0131BC38, 8, 4, 0x05273CDC, false},
		{0x0131BC48, 8, 4, 0x05273CD8, false},
		{0x0131BC56, 8, 4, 0x05273CDC, false},
		{0x0131BC69, 8, 4, 0x05273CE0, false},
		{0x0131BC94, 8, 4, 0x05273B0C, false},
		{0x0131BC9C, 8, 4, 0x05273AF8, false},
		{0x0131BCAC, 8, 4, 0x05273AFC, false},
		{0x0131BCC0, 8, 4, 0x05273AF8, false},
		{0x0131BCD2, 8, 4, 0x05273AFC, false},
		{0x0131BCE9, 8, 4, 0x05273B00, false},
		{0x0131BE4F, 8, 4, 0x05273CD8, false},
		{0x0131BE5E, 8, 4, 0x05273CD8, false},
		{0x0131BE68, 8, 4, 0x05273CDC, false},
		{0x0131BE72, 8, 4, 0x05273CDC, false},
		{0x0131BE7E, 8, 4, 0x05273CDC, false},
		{0x0131BE88, 8, 4, 0x05273CE8, false},
		{0x0131BE98, 8, 4, 0x05273CE0, false},
		{0x0131BEAA, 8, 4, 0x05273CE4, false},
		{0x0131BEB2, 8, 4, 0x05273CEC, false},
		{0x0131BEBA, 8, 4, 0x05273CE8, false},
		{0x0131BEC2, 8, 4, 0x05273CEA, false},
		{0x0131BEE5, 8, 4, 0x05273AF8, false},
		{0x0131BEF7, 8, 4, 0x05273AF8, false},
		{0x0131BF01, 8, 4, 0x05273AFC, false},
		{0x0131BF0B, 8, 4, 0x05273AFC, false},
		{0x0131BF17, 8, 4, 0x05273AFC, false},
		{0x0131BF21, 8, 4, 0x05273B08, false},
		{0x0131BF2F, 8, 4, 0x05273B00, false},
		{0x0131BF3D, 8, 4, 0x05273B04, false},
		{0x0131BF45, 8, 4, 0x05273B0C, false},
		{0x0131BF4D, 8, 4, 0x05273B08, false},
		{0x0131BF55, 8, 4, 0x05273B0A, false},
		{0x0131BF5D, 8, 4, 0x05273DB0, false},
		{0x0131BF69, 8, 4, 0x05273DB0, false},
		{0x0131BF73, 8, 4, 0x05273DB4, false},
		{0x0131BF81, 8, 4, 0x05273DB4, false},
		{0x0131BF91, 8, 4, 0x05273DB4, false},
		{0x0131BF9F, 8, 4, 0x05273DC0, false},
		{0x0131BFAB, 8, 4, 0x05273DB8, false},
		{0x0131BFB3, 8, 4, 0x05273DBC, false},
		{0x0131BFBF, 8, 4, 0x05273DBC, false},
		{0x0131BFC7, 8, 4, 0x05273DC4, false},
		{0x0131BFCF, 8, 4, 0x05273DC0, false},
		{0x0131BFD7, 8, 4, 0x05273DC2, false},
		{0x0131BFE1, 8, 4, 0x05273CA0, false},
		{0x0131BFEB, 8, 4, 0x05273B08, false},
		{0x0131C0BA, 8, 4, 0x05273CD4, false},
		{0x0131C3A0, 8, 4, 0x05273C48, false},
		{0x0131C3AC, 8, 4, 0x05273C48, false},
		{0x0131C3B6, 8, 4, 0x05273C4C, false},
		{0x0131C3C0, 8, 4, 0x05273C4C, false},
		{0x0131C3CC, 8, 4, 0x05273C4C, false},
		{0x0131C3D6, 8, 4, 0x05273C58, false},
		{0x0131C3E2, 8, 4, 0x05273C50, false},
		{0x0131C3EA, 8, 4, 0x05273C54, false},
		{0x0131C3FC, 8, 4, 0x05273C54, false},
		{0x0131C404, 8, 4, 0x05273C5C, false},
		{0x0131C40C, 8, 4, 0x05273C58, false},
		{0x0131C414, 8, 4, 0x05273C5A, false},
	};

	// matchScoreBoardData - PS4 has four records; PC has two 0xDE8-byte records. CG_ResetScoreboard(2) otherwise starts at the adjacent command globals and zeroes CG_SetFocusScoreboardCmd_VAR's linked-list name, causing the repeatable round-load crash at 0x020ECCC3. [2] -> [4].
	// 70 references: 41 rip-relative + 29 ABS32
	inline constexpr reloc_ref match_scoreboard_refs[] = {
		{0x00A07A2C, 7, 3, 0x04C9F8F0, true},
		{0x00A07AE0, 7, 3, 0x04C9F8F0, true},
		{0x00A07B7D, 7, 3, 0x04C9F8F0, true},
		{0x00A09517, 7, 3, 0x04C9EC90, true},
		{0x00A0953D, 7, 3, 0x04C9EC90, true},
		{0x00A09556, 7, 3, 0x04C9EC90, true},
		{0x00A095AB, 7, 3, 0x04C9EC90, true},
		{0x00A095F6, 7, 3, 0x04C9ECB8, true},
		{0x00A09613, 7, 3, 0x04C9FA4C, true},
		{0x00A0966D, 7, 3, 0x04C9EC90, true},
		{0x00A0972D, 7, 3, 0x04C9EC90, true},
		{0x00A097D7, 7, 3, 0x04C9EC90, true},
		{0x00A098CA, 7, 3, 0x04C9F914, true},
		{0x00A09BD3, 7, 3, 0x04C9FA0C, true},
		{0x00A09BFB, 7, 3, 0x04C9EC90, true},
		{0x00A09C6B, 7, 3, 0x04C9EC90, true},
		{0x00A09CDB, 7, 3, 0x04C9EC90, true},
		{0x00A09D56, 7, 3, 0x04C9EC90, true},
		{0x00A09DCB, 7, 3, 0x04C9EC90, true},
		{0x00A09E4E, 7, 3, 0x04C9EC90, true},
		{0x00A09EC0, 7, 3, 0x04C9ECE0, true},
		{0x00A09EF3, 7, 3, 0x04C9FA70, true},
		{0x00A09F20, 7, 3, 0x04C9F928, true},
		{0x00A09F49, 7, 3, 0x04C9EC90, true},
		{0x00A09FDF, 7, 3, 0x04C9EC90, true},
		{0x00A0A11B, 7, 3, 0x04C9EC90, true},
		{0x00A0A173, 7, 3, 0x04C9FA74, true},
		{0x00A0A19D, 7, 3, 0x04C9EC90, true},
		{0x00A0D406, 7, 3, 0x04C9F8F0, true},
		{0x00A1057E, 7, 3, 0x04C9EC90, true},
		{0x00A105C2, 7, 3, 0x04C9EC90, true},
		{0x00A12126, 7, 3, 0x04C9EC90, true},
		{0x00A12265, 7, 3, 0x04C9EC90, true},
		{0x00A1241B, 7, 3, 0x04C9EC90, true},
		{0x00A126E9, 7, 3, 0x04C9EC90, true},
		{0x00A12769, 7, 3, 0x04C9EC90, true},
		{0x00A12879, 7, 3, 0x04C9EC90, true},
		{0x00A141AE, 7, 3, 0x04C9EC90, true},
		{0x00A15C44, 7, 3, 0x04C9EC90, true},
		{0x00A161D6, 7, 3, 0x04C9EC90, true},
		{0x00A181B9, 7, 3, 0x04C9ECE0, true},
		{0x00A099A2, 7, 3, 0x04C9ECE0, false},
		{0x00A099FD, 8, 4, 0x04C9EC90, false},
		{0x00A09A26, 8, 4, 0x04C9ECA4, false},
		{0x00A09A30, 8, 4, 0x04C9ECA0, false},
		{0x00A09A42, 8, 4, 0x04C9EC9C, false},
		{0x00A09A54, 8, 4, 0x04C9ECB0, false},
		{0x00A09AA1, 8, 4, 0x04C9EC90, false},
		{0x00A09AB3, 7, 3, 0x04C9ECB8, false},
		{0x00A09ACE, 7, 3, 0x04C9ECB8, false},
		{0x00A09ADA, 8, 4, 0x04C9F8F0, false},
		{0x00A09AEE, 8, 4, 0x04C9F8F4, false},
		{0x00A09B05, 8, 4, 0x04C9F8F8, false},
		{0x00A09B1C, 8, 4, 0x04C9F8FC, false},
		{0x00A09B33, 8, 4, 0x04C9F900, false},
		{0x00A09B78, 8, 4, 0x04C9ECA8, false},
		{0x00A0A29C, 8, 4, 0x04C9F8F0, false},
		{0x00A180C4, 8, 4, 0x04C9F914, false},
		{0x00A180D5, 7, 3, 0x04C9ECA8, false},
		{0x00A18128, 8, 4, 0x04C9EC90, false},
		{0x00A1813A, 7, 3, 0x04C9ECB8, false},
		{0x00A18155, 7, 3, 0x04C9ECB8, false},
		{0x00A18176, 8, 4, 0x04C9ECA8, false},
		{0x00A181AD, 8, 4, 0x04C9F8F0, false},
		{0x00A181EA, 8, 4, 0x04C9F914, false},
		{0x00A181F4, 8, 4, 0x04C9EC90, false},
		{0x00A18215, 8, 4, 0x04C9ECA4, false},
		{0x00A1821F, 8, 4, 0x04C9ECA0, false},
		{0x00A18230, 8, 4, 0x04C9EC9C, false},
		{0x00A18255, 8, 4, 0x04C9ECB0, false},
	};

	// Per-local-client registration list heads, stride 8, [2] on the PC. Accessor 0x02834F50 (&base[lc], walked by 0x023B4550 with the destructor at 0x02834EF0); count at 0x1A880478; objects 0x390 bytes keyed by byte [obj+2]. Slot 2 (0x1A8804D0) is float data written by the movups at 0x02F6A932 and slot 3 (0x1A8804D8) is a live pointer global with 11 refs, so lc=2 read 0x3F3504F3 as a list head and faulted at 0x023B456D. [2] -> [4], zero-filled = a NULL head the walker exits on.
	// 7 references: 6 rip-relative + 1 ABS32
	inline constexpr reloc_ref lc_subscribers_refs[] = {
		{0x027BBD02, 7, 3, 0x1A801540, true},
		{0x027BBD3B, 7, 3, 0x1A801540, true},
		{0x027BBDA4, 7, 3, 0x1A801540, true},
		{0x027BBDE3, 7, 3, 0x1A801540, true},
		{0x02EEB922, 7, 3, 0x1A801540, true},
		{0x02EEB929, 7, 3, 0x1A801548, true},
		{0x027AFC5C, 8, 4, 0x1A801540, false},
	};

	// DELAYED: s_gamePads - two 0x70-byte records on PC, expanded to four only after the first guest has created a real two-player lobby.  Activating this at startup reaches uninitialised gamepad state and crashes at 0x022E9550.
	// 38 references: 21 rip-relative + 17 ABS32
	inline constexpr reloc_ref gamepads_refs[] = {
		{0x02284E23, 7, 3, 0x17DEF3E4, true},
		{0x02285169, 7, 3, 0x17DEF3E0, true},
		{0x02285413, 7, 3, 0x17DEF3E0, true},
		{0x02285473, 7, 3, 0x17DEF3E4, true},
		{0x022854E2, 7, 3, 0x17DEF400, true},
		{0x02285703, 7, 3, 0x17DEF3E0, true},
		{0x02285723, 7, 3, 0x17DEF3E0, true},
		{0x022857C3, 7, 3, 0x17DEF3E0, true},
		{0x0228584A, 7, 3, 0x17DEF3E0, true},
		{0x0228589A, 7, 3, 0x17DEF3E0, true},
		{0x02285971, 7, 3, 0x17DEF3E0, true},
		{0x02285B43, 7, 3, 0x17DEF434, true},
		{0x02285B73, 7, 3, 0x17DEF430, true},
		{0x02285D76, 7, 3, 0x17DEF400, true},
		{0x02285F5C, 7, 3, 0x17DEF3E0, true},
		{0x0228603A, 7, 3, 0x17DEF3E0, true},
		{0x022863FF, 7, 3, 0x17DEF3E4, true},
		{0x1D440BAD, 7, 3, 0x17DEF3E0, true},
		{0x1BF16F82, 7, 3, 0x17DEF438, true},
		{0x1B8DB901, 7, 3, 0x17DEF3E0, true},
		{0x1BBDBC14, 7, 3, 0x17DEF3E4, true},
		{0x0228488A, 8, 4, 0x17DEF3E4, false},
		{0x022848A6, 8, 4, 0x17DEF3E4, false},
		{0x02284C55, 7, 3, 0x17DEF3E4, false},
		{0x02284D45, 7, 3, 0x17DEF3E4, false},
		{0x02284D9E, 7, 3, 0x17DEF3E4, false},
		{0x02284FCA, 7, 3, 0x17DEF3E4, false},
		{0x02285C80, 7, 3, 0x17DEF438, false},
		{0x02285C8F, 8, 4, 0x17DEF43A, false},
		{0x02285C9B, 8, 4, 0x17DEF3E0, false},
		{0x02285CA5, 8, 4, 0x17DEF3E4, false},
		{0x02286193, 7, 3, 0x17DEF438, false},
		{0x022861AC, 8, 4, 0x17DEF430, false},
		{0x022861B6, 8, 4, 0x17DEF434, false},
		{0x022861E2, 8, 4, 0x17DEF43A, false},
		{0x022861F0, 8, 4, 0x17DEF3E4, false},
		{0x1CB15D82, 7, 3, 0x17DEF3E0, false},
		{0x1CB15D96, 8, 4, 0x17DEF3E4, false},
	};

	inline constexpr reloc_table gamepads_reloc_table = {"gamepads", 0x17DEF3E0, 0xE0, 0x1C0, gamepads_refs, 38, nullptr, 0};

	inline constexpr reloc_table reloc_tables[] = {
		{"bit_array", 0x04C98B80, 0xC0, 0x180, bit_array_refs, 32, nullptr, 0},
		{"storage", 0x1788EAC0, 0x112B0, 0x22560, storage_refs, 25, nullptr, 0},
		{"client_objs", 0x03390190, 0x10, 0x20, client_objs_refs, 19, client_objs_end_refs, 3},
		{"client_ui", 0x14F344B0, 0x2340, 0x45C0, client_ui_refs, 2, nullptr, 0},
		{"netchan", 0x16DEAEB0, 0x250, 0x4A0, netchan_refs, 15, nullptr, 0},
		{"objtab_a", 0x09B35878, 0x10, 0x20, objtab_a_refs, 28, nullptr, 0},
		{"objtab_b", 0x09931F00, 0x20, 0x40, objtab_b_refs, 3, nullptr, 0},
		{"fps_ctx", 0x04C98B70, 0x10, 0x10, fps_ctx_refs, 92, nullptr, 0},
		{"voice_comm", 0x0535BCB0, 0x1468, 0x28D0, voice_comm_refs, 12, nullptr, 0},
		{"cbuf", 0x1681EFB8, 0x20, 0x40, cbuf_refs, 4, nullptr, 0},
		{"players_kb", 0x052739F0, 0x930, 0x1260, players_kb_refs, 213, nullptr, 0},
		{"match_scoreboard", 0x04C9EC90, 0x1BD0, 0x37A0, match_scoreboard_refs, 70, nullptr, 0},
		{"lc_subscribers", 0x1A801540, 0x10, 0x20, lc_subscribers_refs, 7, nullptr, 0},
	};

	// ======== clientGameStates (signin) references ========
	// Moves clientGameStates (stride 0x24, two slots) to a 4-slot copy (signin_old_base ->
	// signin_new_base), so Com_ControllerIndex_GetLocalClientNum stops returning -1 for
	// controllers 2/3 (their storage reads completed with localClient -1 before).

	struct signin_ref
	{
		uint32_t disp_rva;   // the 4-byte field to rewrite
		uint8_t old_bytes[4];
		uint8_t new_bytes[4];
	};

	inline constexpr uint32_t signin_old_base = 0x1684FA30;
	inline constexpr uint32_t signin_new_base = 0x1A828500;
	inline constexpr uint32_t signin_stride   = 0x24;
	inline constexpr uint32_t signin_old_slots = 2;
	inline constexpr uint32_t signin_new_slots = 4;

	// 70 references, from signin_reloc.txt
	inline constexpr signin_ref signin_refs[] = {
		{0x020E3045, {0xEF,0xC9,0x76,0x14}, {0xBF,0x54,0x74,0x18}}, // slot
		{0x020E304C, {0x30,0xCA,0x76,0x14}, {0x48,0x55,0x74,0x18}}, // end marker
		{0x020E306D, {0xC3,0xC9,0x76,0x14}, {0x93,0x54,0x74,0x18}}, // slot
		{0x020E3085, {0xAF,0xC9,0x76,0x14}, {0x7F,0x54,0x74,0x18}}, // slot
		{0x020E308C, {0xF0,0xC9,0x76,0x14}, {0x08,0x55,0x74,0x18}}, // end marker
		{0x020E30AF, {0x8D,0xC9,0x76,0x14}, {0x5D,0x54,0x74,0x18}}, // slot
		{0x020E30C5, {0x6F,0xC9,0x76,0x14}, {0x3F,0x54,0x74,0x18}}, // slot
		{0x020E30CC, {0xB0,0xC9,0x76,0x14}, {0xC8,0x54,0x74,0x18}}, // end marker
		{0x020E30ED, {0x4B,0xC9,0x76,0x14}, {0x1B,0x54,0x74,0x18}}, // slot
		{0x020E310A, {0x22,0xC9,0x76,0x14}, {0xF2,0x53,0x74,0x18}}, // slot
		{0x020E317D, {0xCF,0xC8,0x76,0x14}, {0x9F,0x53,0x74,0x18}}, // slot
		{0x020E31BA, {0x72,0xC8,0x76,0x14}, {0x42,0x53,0x74,0x18}}, // slot
		{0x020E31DA, {0x52,0xC8,0x76,0x14}, {0x22,0x53,0x74,0x18}}, // slot
		{0x020E31FA, {0x32,0xC8,0x76,0x14}, {0x02,0x53,0x74,0x18}}, // slot
		{0x020E3222, {0x0A,0xC8,0x76,0x14}, {0xDA,0x52,0x74,0x18}}, // slot
		{0x020E3270, {0xBC,0xC7,0x76,0x14}, {0x8C,0x52,0x74,0x18}}, // slot
		{0x020E32CA, {0x7A,0xC7,0x76,0x14}, {0x4A,0x52,0x74,0x18}}, // slot
		{0x020E32FA, {0x52,0xC7,0x76,0x14}, {0x22,0x52,0x74,0x18}}, // slot
		{0x020E33FA, {0x4A,0xC6,0x76,0x14}, {0x1A,0x51,0x74,0x18}}, // slot
		{0x020E3429, {0x03,0xC6,0x76,0x14}, {0xD3,0x50,0x74,0x18}}, // slot
		{0x020E3586, {0xA6,0xC4,0x76,0x14}, {0x76,0x4F,0x74,0x18}}, // slot
		{0x020E369E, {0x8E,0xC3,0x76,0x14}, {0x5E,0x4E,0x74,0x18}}, // slot
		{0x020E378A, {0xAA,0xC2,0x76,0x14}, {0x7A,0x4D,0x74,0x18}}, // slot
		{0x020E37C4, {0x74,0xC2,0x76,0x14}, {0x44,0x4D,0x74,0x18}}, // slot
		{0x020E37CA, {0x92,0xC2,0x76,0x14}, {0x62,0x4D,0x74,0x18}}, // slot
		{0x020E37D2, {0x62,0xC2,0x76,0x14}, {0x32,0x4D,0x74,0x18}}, // slot
		{0x020E37DC, {0x7C,0xC2,0x76,0x14}, {0x4C,0x4D,0x74,0x18}}, // slot
		{0x020E3807, {0x25,0xC2,0x76,0x14}, {0xF5,0x4C,0x74,0x18}}, // slot
		{0x020E3887, {0xAD,0xC1,0x76,0x14}, {0x7D,0x4C,0x74,0x18}}, // slot
		{0x020E3905, {0x27,0xC1,0x76,0x14}, {0xF7,0x4B,0x74,0x18}}, // slot
		{0x020E3945, {0xE7,0xC0,0x76,0x14}, {0xB7,0x4B,0x74,0x18}}, // slot
		{0x020E3993, {0xA9,0xC0,0x76,0x14}, {0x79,0x4B,0x74,0x18}}, // slot
		{0x020E399A, {0xEA,0xC0,0x76,0x14}, {0x02,0x4C,0x74,0x18}}, // end marker
		{0x020E39BD, {0x73,0xC0,0x76,0x14}, {0x43,0x4B,0x74,0x18}}, // slot
		{0x020E3CBB, {0x30,0xFA,0x84,0x16}, {0x00,0x85,0x82,0x1A}}, // slot
		{0x020E3CC3, {0x38,0xFA,0x84,0x16}, {0x08,0x85,0x82,0x1A}}, // slot
		{0x020E3CCB, {0x3C,0xFA,0x84,0x16}, {0x0C,0x85,0x82,0x1A}}, // slot
		{0x020E3CD3, {0x40,0xFA,0x84,0x16}, {0x10,0x85,0x82,0x1A}}, // slot
		{0x020E3CDB, {0x44,0xFA,0x84,0x16}, {0x14,0x85,0x82,0x1A}}, // slot
		{0x020E3CE3, {0x48,0xFA,0x84,0x16}, {0x18,0x85,0x82,0x1A}}, // slot
		{0x020E3CEB, {0x4C,0xFA,0x84,0x16}, {0x1C,0x85,0x82,0x1A}}, // slot
		{0x020E3CF3, {0x50,0xFA,0x84,0x16}, {0x20,0x85,0x82,0x1A}}, // slot
		{0x020E3D0E, {0x30,0xFA,0x84,0x16}, {0x00,0x85,0x82,0x1A}}, // slot
		{0x020E3D1D, {0x30,0xFA,0x84,0x16}, {0x00,0x85,0x82,0x1A}}, // slot
		{0x020E3D2C, {0x34,0xFA,0x84,0x16}, {0x04,0x85,0x82,0x1A}}, // slot
		{0x020E3D3B, {0x34,0xFA,0x84,0x16}, {0x04,0x85,0x82,0x1A}}, // slot
		{0x020E3D4A, {0x38,0xFA,0x84,0x16}, {0x08,0x85,0x82,0x1A}}, // slot
		{0x020E3D59, {0x38,0xFA,0x84,0x16}, {0x08,0x85,0x82,0x1A}}, // slot
		{0x020E3D68, {0x3C,0xFA,0x84,0x16}, {0x0C,0x85,0x82,0x1A}}, // slot
		{0x020E3D77, {0x3C,0xFA,0x84,0x16}, {0x0C,0x85,0x82,0x1A}}, // slot
		{0x020E3D86, {0x40,0xFA,0x84,0x16}, {0x10,0x85,0x82,0x1A}}, // slot
		{0x020E3D95, {0x40,0xFA,0x84,0x16}, {0x10,0x85,0x82,0x1A}}, // slot
		{0x020E3DA4, {0x44,0xFA,0x84,0x16}, {0x14,0x85,0x82,0x1A}}, // slot
		{0x020E3DB3, {0x44,0xFA,0x84,0x16}, {0x14,0x85,0x82,0x1A}}, // slot
		{0x020E3DC2, {0x48,0xFA,0x84,0x16}, {0x18,0x85,0x82,0x1A}}, // slot
		{0x020E3DD1, {0x48,0xFA,0x84,0x16}, {0x18,0x85,0x82,0x1A}}, // slot
		{0x020E3DE0, {0x4C,0xFA,0x84,0x16}, {0x1C,0x85,0x82,0x1A}}, // slot
		{0x020E3DEF, {0x4C,0xFA,0x84,0x16}, {0x1C,0x85,0x82,0x1A}}, // slot
		{0x020E3DFE, {0x50,0xFA,0x84,0x16}, {0x20,0x85,0x82,0x1A}}, // slot
		{0x020E3E0D, {0x50,0xFA,0x84,0x16}, {0x20,0x85,0x82,0x1A}}, // slot
		{0x020E3E1C, {0x30,0xFA,0x84,0x16}, {0x00,0x85,0x82,0x1A}}, // slot
		{0x020E3E24, {0x38,0xFA,0x84,0x16}, {0x08,0x85,0x82,0x1A}}, // slot
		{0x020E3E2C, {0x3C,0xFA,0x84,0x16}, {0x0C,0x85,0x82,0x1A}}, // slot
		{0x020E3E34, {0x40,0xFA,0x84,0x16}, {0x10,0x85,0x82,0x1A}}, // slot
		{0x020E3E3C, {0x44,0xFA,0x84,0x16}, {0x14,0x85,0x82,0x1A}}, // slot
		{0x020E3E44, {0x48,0xFA,0x84,0x16}, {0x18,0x85,0x82,0x1A}}, // slot
		{0x020E3E4C, {0x4C,0xFA,0x84,0x16}, {0x1C,0x85,0x82,0x1A}}, // slot
		{0x020E3E54, {0x50,0xFA,0x84,0x16}, {0x20,0x85,0x82,0x1A}}, // slot
		{0x020E3E64, {0x34,0xFA,0x84,0x16}, {0x04,0x85,0x82,0x1A}}, // slot
		{0x020E3E73, {0x34,0xFA,0x84,0x16}, {0x04,0x85,0x82,0x1A}}, // slot
	};

	// slot-count bounds, written to signin_new_slots
	inline constexpr uint32_t signin_bounds[] = {
		0x020E33CD,
		0x020E382D,
		0x020E384D,
		0x020E386A,
		0x020E392D,
		0x020E396A,
		0x027C16EB,
		0x027C1A45,
	};

	// ======== descriptor types ========

	// from splitscreen/01_core.inl
	// A one-byte code patch: `expect` is the original byte, `value` the new one.
	struct byte_patch
	{
		size_t rva;
		uint8_t expect;
		uint8_t value;
		const char* what;
	};

	// from splitscreen/02_guest_storage.inl
	struct targets_lea
	{
		uint32_t insn_rva;  // 7-byte rip-relative lea, disp32 at +3
		uint32_t offset;    // offset into the table it points at
	};

	// from splitscreen/03_signin_seats.inl
	// Device-type selector. Both gamepad loops (poll, per-frame update) reuse the
	// loop-bound register as the constant 2 of the device-type selector, so the
	// widened bound 4 made devices 4..7 (the non-XInput API) type 4, which nothing
	// handles. The same 11 bytes, rewritten without the register:
	//     lea eax,[rdx-4]; cmp eax,4; sbb ecx,ecx; and ecx,2
	// give 2 for devices 4..7, else 0, whatever the bound. The length must not
	// change (both ends are jump targets). Valid with the stock bound too, so it
	// is applied at startup.
	struct type_selector_site
	{
		uint32_t rva;
		uint8_t expected[11];
	};

	// from splitscreen/04_panes.inl
	struct vp_patch
	{
		uint32_t rva;
		uint8_t len;
		uint8_t off;        // byte to change (or disp offset for a base lea)
		bool is_base;       // true = rip-disp to the table, false = single byte
		uint8_t to;         // new byte value when !is_base
		uint8_t expect[7];
	};
	struct flat24_site
	{
		uint32_t rva;       // lea instruction start (7 bytes, disp at +3)
		uint8_t expect[7];
	};
	struct pane_bound
	{
		uint32_t rva;
		uint8_t offset;
		uint8_t from;
		uint8_t to;
		uint8_t expect[5];
		uint8_t expect_len;
	};

	// from splitscreen/05_renderer_scene.inl
	// ============ cgEntCollWorld / cgEntCollNodes: entity collision ========
	// Both are [2] with foreign slots 2/3, cleared for lc 2 by the inlined
	// CG_ClearEntityCollWorld, so both are relocated. That function (PS4
	// 0x189890, called from CG_SetInitialSnapshot) builds the free-list, so
	// the engine initializes slots 2/3 itself.
	// The site tables come from tools/gen_entcoll_sites.py; never edit them by
	// hand (a hand-built table missed 8 field accessors and hung the game).
	// target_off: a site may point at a field of element 0.
	// History: LOG.md, ae57c92
	struct entcoll_site
	{
		uint32_t rva;         // instruction start
		uint8_t disp_off;     // byte offset of the disp32 inside it
		uint8_t insn_len;     // total instruction length
		bool rip;             // true: disp is rip-relative; false: absolute RVA
		uint32_t target_off;  // target's offset inside element 0
	};

	// from splitscreen/06_relocations_a.inl
	// Clientfield pending-callback buffer, [2] -> [4] clients. Client 2 faulted
	// at 0x00132F94. Layout: entries 2 x 2048 x 32 bytes, then counts [2] x 4
	// at +0x20000 (total 0x20008). Growing it alone would put lc 2's entries on
	// the counts, so the count offset moves to 0x40000 (6 sites) and the size
	// to 0x40010 (3 sites). Consumers read the buffer through cf_pointer_rva.
	// All or nothing with rollback.
	struct cf_imm
	{
		uint32_t rva;
		uint8_t off;
		uint32_t was;
		uint32_t want;
	};
	// size = immediate width in bytes (4 = imm32, 1 = imm8)
	struct entword_imm { uint32_t rva; uint8_t off; uint8_t size; uint32_t was; uint32_t want; };
	// Lua Engine.GetClientNum / GetPredictedClientNum returned -1 for controllers
	// 2/3 (no MP HUD in panes 3/4): both start with `cmp ecx,1 / ja -> -1`, while
	// PS4 0xD18000 accepts < 4. Behind the check the PC only indexes cg globals
	// while lc < cl_maxLocalClients, so widening to 3 is safe. Eight other Lua
	// bindings with this check are frontend/online and stay.
	struct ctrl_check_patch
	{
		uint32_t rva;           // the cmp
		uint8_t stock[9];       // cmp ecx,1 ; ja rel32
		const char* what;
	};
	struct sst_imm { uint32_t rva; uint8_t off; uint8_t size; uint32_t was; uint32_t want; };
	// DWARF-map batch 1b: PS4 [4] globals the cgame frame touches, matched to the
	// PC by stride and confirmed by the index register (lc, or the cg_t index).
	// Sites from tools/gen_reloc_sites.py v2.2 (data/reloc_sites/sites_<name>.txt),
	// each verified against the old address before anything is written.
	struct perclient_array
	{
		const char* name;
		uint32_t base;              // old RVA of slot 0
		uint32_t stride;            // bytes per local client
		const entcoll_site* sites;
		size_t count;
		uint32_t ctor_rva;          // 0: zero-fill IS the initial state
		uint8_t ctor_sig[8];        // the constructor's first bytes, verified
	};

	// from splitscreen/09_script_bounds.inl
	// ---- Builtins with their own form of the check ----
	// These compare lc against a register holding 1, or use `cmp eax,2 / jl`, so they are not
	// in the table above and raised a script error for players 3/4 (no screen filters, ...).
	// PS4 accepts 0..3 in all of them (CScr_GetLocalClientNum 0x1D926A0, CPlayerCmd_HasPerk
	// 0x28C830, CPlayerCmd_GetPerks 0x28C930). Each window is rewritten once to
	// `cmp eax,1 / jbe|ja` - same length, same instruction boundaries and jump targets, stock
	// bytes verified first - and sync_csc_lc_bound owns the imm8 from then on. Every array
	// they index for lc 2/3 is per cl_maxLocalClients (cgArray, entity pools) or moved by
	// reloc_tables, which cl_maxLocalClients > 2 already requires; the two flags below name
	// the relocations that are not part of that (LOG.md 2026-09-29, "odd lc checks").
	struct odd_lc_check
	{
		const char* name;
		uint32_t rva;             // first byte of the rewritten window
		uint8_t len;
		uint8_t imm;              // offset of the bound imm8 in the window
		uint8_t stock[15];
		uint8_t patched[15];
		bool needs_signin;        // reads clientGameStates[lc] (signin_relocated)
		bool needs_lui_roots;     // reads s_rootData[controller] (lui_roots_relocated)
	};

	// from splitscreen/10_relocations_b.inl
	struct con_rel_site
	{
		uint32_t rva;
		uint8_t off;         // where the disp32 / imm32 sits in the instruction
		uint32_t old_value;  // offset from con it holds
	};
	// ---- Umbra occlusion culling: per-client state [2] -> [4] ---------------
	// Symptom: pane 3 drew no world geometry. The heap object sUmbra holds
	// per-client arrays (PS4: UmbraQueryParameters[4], R_Umbra_SelectTome
	// 0x945980). The PC keeps [2], and client 2 aliases the next fields:
	//   +0x12DC0C  params[2] x 0x14
	//   +0x12DC48  tome trigger[2] x 4   (-1 = none)
	//   +0x12DC50  persistent tome trigger[2] x 4
	// Fix: grow the allocation 0x470210 -> 0x470300 and put [4] copies at the
	// new tail; params at a multiple of 0x14, since the distance-scale setter
	// indexes (lc + 0xF167) * 0x14. Only possible while sUmbra is still NULL.
	struct umbra_disp
	{
		uint32_t rva;
		uint8_t off;       // where the imm32/disp32 sits in the instruction
		uint32_t old_value;
		uint32_t new_value;
	};
	// init-loop end bounds: `lea reg, [base + disp8]`, disp8 at +3
	struct umbra_bound
	{
		uint32_t rva;
		uint8_t old_value;
		uint8_t new_value;
	};
	// An FxLensFlaresManager entry point and its stock first bytes (gate_lensflares_for_extra_clients).
	struct lc_gate
	{
		uint32_t rva;
		uint8_t prologue[9];
		uint8_t len;
	};

	// from splitscreen/12_relocations_ui.inl
	struct percg_ref
	{
		uint32_t rva;
		uint8_t len;
		uint8_t disp_off;
		uint32_t delta;   // byte offset within element 0
		bool rip;         // true = rip-relative, false = ABS32 off the image base
		uint8_t expected[12];
	};
	struct uiroot_ref
	{
		uint32_t rva;
		uint8_t len;
		uint8_t disp_off;
		uint8_t delta;    // byte offset within element 0
		bool rip;         // true = rip-relative, false = ABS32 off the image base
		uint8_t expected[9];
	};
	struct lui_bound
	{
		uint32_t rva;
		uint8_t stock[4];
		uint8_t value;
	};
	enum le_array_id : uint8_t { le_active = 0, le_free = 1, le_pool = 2 };
	struct le_ref
	{
		uint32_t rva;
		uint8_t len;
		uint8_t disp_off;
		uint8_t arr;
		uint32_t delta;   // byte offset within that array
		bool rip;         // true = rip-relative, false = ABS32 off the image base
		uint8_t expected[9];
	};

	// from splitscreen/14_player4_fixes.inl
	// clientUIActives walker end-bounds: twelve loops end at
	// `lea reg, [clientUIActives[2]]` (four at field +8), among them the inlined
	// CL_AnyLocalClientsRunning. Unwidened, clients 2/3 miss the per-frame
	// upkeep. A thirteenth reference is an Arxan code copy and is left alone.
	struct end_bound_fix
	{
		uint32_t insn_rva;
		uint8_t expect[7];
	};

	// from splitscreen/12_relocations_ui.inl
	// One per-client LUI target table (relocate_lui_target_tables).
	struct lui_table
	{
		const char* name;
		uint32_t base;
		uint32_t stride;
		uint32_t old_count;
		uint32_t new_count;
		const entcoll_site* sites;
		size_t site_count;
	};

	// ======== splitscreen/01_core.inl ========

	inline constexpr size_t stride_site_rva = 0x01F23651;
	inline constexpr uint8_t stride_site_bytes[] = {0x48, 0x69, 0xC9, 0x40, 0xE9, 0x01, 0x00};
	inline constexpr size_t alloc_floor_rva = 0x0135D68C;
	inline constexpr byte_patch local_client_count_patches[] = {
		// The in-game allocation pass (flags bit 2) discards that result and
		// hard-codes local = 2 with `lea r14d, [rsi-0x10]` (rsi = 18 maxClients).
		// PS4 (0x416A10) has no such override. NOP the lea; maxClients stays 18.
		{0x0135D6A4, 0x44, 0x90, "flag-4 alloc: drop the hard local=2 (1/4)"},
		{0x0135D6A5, 0x8D, 0x90, "flag-4 alloc: drop the hard local=2 (2/4)"},
		{0x0135D6A6, 0x76, 0x90, "flag-4 alloc: drop the hard local=2 (3/4)"},
		{0x0135D6A7, 0xF0, 0x90, "flag-4 alloc: drop the hard local=2 (4/4)"},

		// Boot bind loop 0x0135DDC8..0x0135DE24: the only boot code that sets a
		// slot's controllerIndex and beingUsed flag.
		{0x0135DE43, 0x02, 0x04, "boot bind loop: local clients 2 -> 4"},

		// ...and its controllerIndex clamp min(i, 1); PS4 clamps min(i, 3)
		// (0xE49DC5). Otherwise slots 2 and 3 would bind controllerIndex 1 too.
		{0x0135DE2F, 0x01, 0x03, "boot bind clamp: controllerIndex min(i,1) -> min(i,3)"},

		// Lua GetCountUsedAndSignedInLocalClients (0x01FC73E0) counts local
		// clients 0..1; PS4 (0xD2EE40) counts 0..3. The body only calls
		// predicates, so widening it cannot write anywhere.
		{0x01FBAC99, 0x02, 0x04, "GetCountUsedAndSignedInLocalClients: 2 -> 4"},

		// The lobby panel draws its rows from Engine.GetUsedControllerCount() and
		// Engine.IsControllerBeingUsed(i). GetUsedControllerCount (0x01FE4260) and
		// GetNonUsedControllerCount (0x01FE35C0) loop over two controllers; PS4
		// (0xD5B970, 0xD5BBA0) loops over four.
		// Disabled: suspected in a crash on the third sign-in (CRT fastfail in
		// WndProc 0x02334790, Dvar_GetInt on a NULL dvar). They only feed Lua
		// counters. History: LOG.md, "GetUsedControllerCount".
		// {0x01FE428E, 0x02, 0x04, "GetUsedControllerCount: 2 -> 4"},
		// {0x01FE35F9, 0x02, 0x04, "GetNonUsedControllerCount: 2 -> 4"},

		// GetMaxControllerCount (0x01FE3370) returns the constant 2.0f; PS4
		// (0xD5BCF0) returns 4. The stock datasources.lua creates per-controller
		// UI models (scriptNotify, hudItems.*, ...) for 0..GetMaxControllerCount()-1
		// at UI init, so at 2 player 3's HUD never received a script notify.
		// Never raise it past the controllers that have seats: once that let Lua
		// touch a seatless controller and killed the boot with
		// __report_rangecheckfailure (0xC0000409 subcode 8).
		// One byte: 2.0f = 0x40000000, 4.0f = 0x40800000.
		{0x01FD6BFD, 0x00, 0x80, "GetMaxControllerCount: 2.0f -> 4.0f (controllers with seats; 4 since player 4)"},

		// GetMaxLocalControllers (0x01FE3390), same shape and value. It caps
		// lobby_maxLocalPlayers (Lobby_SetMaxLocalPlayers: 4 offline, capped here),
		// which LobbyAddLocalClient checks when an unused controller presses its
		// join button. CoDMenu also subscribes the button models of controllers
		// 0..GetMaxLocalControllers()-1.
		{0x01FD6C1D, 0x00, 0x80, "GetMaxLocalControllers: 2.0f -> 4.0f (controllers with seats; 4 since player 4)"},

		// Engine.GetPlayerStats (0x01FCAD00), which the gobblegum row is built from,
		// returned nil for controller 2 because of its first gate `cmp r14d, 1 / ja`.
		// PS4 (0xD36D20) bounds the same argument at 4. The second gate, the stats
		// walk 0x01EA9A30, passes for controller 2.
		{0x01FBE6DE, 0x01, 0x03, "Engine.GetPlayerStats: controller bound 1 -> 3"},

		// LobbyHost_AddLocalClients (0x01ED7560) decides who is in the lobby. PS4
		// (0xCA5CA0) loops ci 0..3 and adds every controller that passes
		// ShouldAddController (seat in use; offline, or signed in to Demonware).
		// Controller 2 passes that; only the PC bound `cmp ebx, 2` kept it out.
		// Needs the netchan relocation first: without it, adding controller 2
		// crashed at 0x02173AD0 reading past a per-index array (0x16E69E20, stride
		// 0x128). This table is only applied when every relocation succeeded,
		// netchan included.
		{0x01ECACEB, 0x02, 0x04, "LobbyHost_AddLocalClients: controllers 2 -> 4"},

		// Activation loop, the PC twin (inlined at 0x0283AB30) of PS4
		// CL_LocalClients_SetAllUsedActive (0x1517020): at every launch,
		// SetActive(i, IsBeingUsed(i)). With a bound of two, client 2 stayed
		// used-but-inactive. Three, not four: clientUIActives slot 2 is real once
		// voice_comm has moved, slot 3 is still foreign.
		{0x027C1A45, 0x02, 0x03, "SetAllUsedActive loop: local clients 2 -> 3"},

		// Connect loop of PS4 CL_MapLoading (0x40CB40): for each active local
		// client, CL_Disconnect, SetActive, connectionState 5/6 and the connected
		// flag. The PC twin ends at 0x01359DB9 and walks clientUIActives by byte
		// offset, so its bound is a size: `cmp rsi, 0x20F0` (2 * 0x1078). Without
		// it client 2 was activated but never connected.
		// Index 3 only touches the owned head of clientUIActives[3], seat record 3
		// and [4] arrays; the body skips a client that is not in use.
		{0x01359DDC, 0xF0, 0xE0, "connect loop end: 2*0x1078 -> 4*0x1078 (low)"},
		{0x01359DDD, 0x20, 0x41, "connect loop end: 2*0x1078 -> 4*0x1078 (high)"},

		// The same loop's counter (`cmp ebx, 2` at 0x01359DDA) is a second bound
		// and the one that actually ends it. Both must move.
		{0x01359DFC, 0x02, 0x04, "connect loop counter: local clients 2 -> 4"},

		// Per-client reset loop in the same function (clears flag bit 6 and
		// keyCatchers). Must cover the same clients, or client 2 carries a stale
		// keyCatcher state into the round.
		{0x01359BF8, 0x02, 0x04, "map-load reset loop: local clients 2 -> 4"},

		// /GS range check on the per-client byte array 0x052F29C4 (index >= 2 ->
		// __report_rangecheckfailure, 0xC0000409), hit via CL_ClearKeys. Widened in
		// place: slots 2 and 3 (0x052F29C6/7) have no code references, they are
		// padding.
		{0x012F351D, 0x02, 0x04, "per-client byte array 0x052F29C4 range check: 2 -> 4"},

		// IN_Attack_Up (0x0131B260; PS4 0x3E0D00) clears gAttackEdgeDetected[lc]
		// (the byte array above) and releases two kbuttons in playersKb[lc].
		// Player 3 firing hit this /GS check. playersKb is already [4].
		{0x0131B28A, 0x02, 0x04, "IN_Attack_Up range check: local clients 2 -> 4"},
		// 0x0131C050: per-frame analog-trigger edge; on release it clears byte
		// 0x052F3360[lc] behind `cmp rbx, 2`. Slots 2/3 are unreferenced padding.
		{0x0131C0CA, 0x02, 0x04, "trigger-edge byte array 0x052F3360 range check: 2 -> 4"},

		// The CG frame function (0x00A129A9) guards a per-client byte array at
		// 0x04D1DC94 with `cmp r15, 2 / jae __report_rangecheckfailure`. That
		// fail-fast bypasses SEH: no dialog and no BOIII dump, only a WER dump.
		// Slots 2/3 have no references (padding), so it widens in place.
		{0x00A15B68, 0x02, 0x04, "CG frame per-client byte array 0x04D1DC94 range check: 2 -> 4"},

		// Netchan poll: not applied from this table. PS4 Com_ClientPacketEvent
		// (0xE491A0) polls each local client's own netchan for all four clients;
		// the PC twin (0x020F7AC7..) stops at two (`cmp ebx, 2` at 0x020F7BA2), so
		// client 2's replies were never read and it parked at CA_CONFIRMLOADING.
		// It is written at map load (netchan_poll_imm_rva): in the frontend
		// cl_maxLocalClients is 2, the clientConnection array (stride 0x25780) is
		// carved for two only, and polling index 2 there crashed the lobby.
		// {0x020F7BA4, 0x02, 0x03, "netchan poll"},   applied dynamically

		// CL_Frame pump, also not in this table. Com_Frame calls CL_Frame(lc) only
		// for lc < 2 (`cmp ebx, 2` at 0x020F95DA; PS4 0xE4D38D loops to 4), so
		// client 2's handshake is never advanced. run_cl_init_for_local_client2()
		// writes that bound only after CL_Init has run for client 2: CL_Frame runs
		// per-client code (error popup, pending-error slot) before its own gate,
		// and IsBeingUsed(2) is true as soon as player 3 signs in.

		// CL_Init's range check (0x01359468, guarding cl_waitingOnServerToLoadMap
		// at 0x053D4988) and Cbuf_Execute's are widened by
		// run_cl_init_for_local_client2() only around its own CL_Init(2) call.
		// A permanently widened /GS check leaves the stock engine running against a
		// bound that no longer matches its array.
		// {0x0135946B, 0x02, 0x03, "CL_Init range check"},   scoped instead

		// Cbuf_Execute's range check (0x020EC1AD). For client 2 that call is
		// inert: every per-client byte and dword it reads for lc 2 is unreferenced.
		// {0x020EC1B0, 0x02, 0x03, "Cbuf_Execute range check"},  scoped too

		// GetLobbyLocalClientCount (count loop 0x01EFF910, `cmp ebx, 2`). A
		// three-player lobby listed client 2 in the roster but counted two local
		// clients, so its row had no loadout data and DEACTIVATE SPLITSCREEN could
		// not release it. The body only indexes client_objs (0x0340F180), which
		// this component relocates to [4]; an empty slot returns false.
		{0x01EF31FA, 0x02, 0x04, "GetLobbyLocalClientCount loop: 2 -> 4"},

		// DEACTIVATE SPLITSCREEN: LobbyRemoveAllLocalSplitscreenClient (0x01F16D60),
		// `cmp ebx, 2` at 0x01F16E02. The body reaches the index only through
		// client_objs (relocated) and the seat lookup 0x020EF7C0, which already
		// covers index 2. Effect not confirmed. The seat table must stay
		// contiguous: the engine never produces a gap such as 0 and 2 in use.
		{0x01F0A684, 0x02, 0x04, "LobbyRemoveAllLocalSplitscreenClient loop: 2 -> 4"},

		// LiveUser_IsUserGuest (0x01EC70C0) returns false for every ci >= 2 before
		// it reads the isGuest byte. Storage_Pump's guest branch (0x02277376) lets
		// a guest inherit its loadout files from the primary, so this bound left
		// player 3 without gobblegums. The only array it indexes,
		// s_UserDataForControllerMap (0x0340F180), is relocated to four entries.
		{0x01EBA642, 0x01, 0x03, "LiveUser_IsUserGuest bound: ci<=1 -> ci<=3"},

		// Console commands disableallbutprimaryclients (0x0134C300),
		// disableallclients (0x0134C340) and a provisional variant (0x0134C390)
		// loop over two clients; PS4 CL_Command_DisableAllButPrimaryClients
		// (0x40B550) loops over four. They drop the guests on the way back to the
		// frontend; after game over client 2 stayed active and the frontend hung.
		// The bodies write only clientUIActives and the relocated seat table.
		{0x0134C352, 0x02, 0x04, "disableallbutprimaryclients loop: local clients 2 -> 4"},
		{0x0134C396, 0x02, 0x04, "disableallclients loop: local clients 2 -> 4"},
		{0x0134C3EF, 0x02, 0x04, "provisional disable-all loop: local clients 2 -> 4"},

		// IN_GamepadsMove (0x022F3EF0) polls pads for ci < 2 (`cmp edi, 2` at
		// 0x022F40F2); PS4 (0xDBA1F0) polls four. It feeds sticks, triggers and
		// buttons to usercmds and also to the menus (PS4: ->
		// CL_GamepadButtonEventForPort -> UI_CoD_KeyEvent), so without it players
		// 3/4 cannot move or press A. Per-client targets: gaGlobs, playerKeys,
		// s_gamePads (all [4]), the seat table and clientUIActives keyCatchers.
		{0x02287024, 0x02, 0x04, "IN_GamepadsMove: poll controllers 2 -> 4 (players 3/4 sticks/buttons, menus too)"},

		// Netchan thread (0x02176E80.., `cmp ebx, 2` at 0x02176EE2): transmit,
		// keepalives, acks and stale-message cleanup ran for controllers 0 and 1
		// only; PS4 Netchan_Thread (0xE7E650) does four. A stale fragment left for
		// controller 2 swallowed the host's next message, so player 3 got stuck
		// loading from the second round on. Indexed arrays: s_netchan rows and
		// clientGameStates, both four deep.
		{0x0211E444, 0x02, 0x04, "Netchan_Thread pump: controllers 2 -> 4 (transmit/acks/stale cleanup)"},

		// Client setup for a level load (0x0135DCD0), frontend branch: `cmp ebx, 2`
		// at 0x0135DD35 (the in-game branch is the boot bind loop row above).
		// PS4 Com_LocalClients_AssignUIContextsForFrontEnd (0xE35BD0) covers four.
		// Without it client 2 stayed active on the way back to the frontend. The
		// body writes userData, the seat table and clientUIActives flags only.
		{0x0135DD57, 0x02, 0x04, "frontend client setup loop: local clients 2 -> 4"},
	};
	inline constexpr byte_patch storage_patches[] = {
		{0x02218DF5, 0x51, 0x91, "storage pool: SIB scale x2 -> x4"},
		{0x02218F53, 0x02, 0x04, "AllocateMemory: controllers 2 -> 4"},
		{0x02219A0E, 0x02, 0x04, "clear-all loop: controllers 2 -> 4"},
		{0x02219AF3, 0x02, 0x04, "file lookup A: controllers 2 -> 4"},
		{0x02219B95, 0x02, 0x04, "file lookup B: controllers 2 -> 4"},
		{0x02219D1A, 0x02, 0x04, "file lookup C: controllers 2 -> 4"},
		{0x02219FA4, 0x02, 0x04, "file lookup D: controllers 2 -> 4"},
		{0x0221A1C3, 0x02, 0x04, "file lookup E: controllers 2 -> 4"},
		{0x0221A30B, 0x02, 0x04, "controller gate 0x02276E30: 2 -> 4"},
		{0x0221AB16, 0x02, 0x04, "controller gate 0x02277640: 2 -> 4"},

		// The rows below must be applied here at boot: controller 1 gets storage
		// and stats readiness from the boot pass, and a controller patched later
		// misses it.
		//
		// 0x0135C8AD: the per-controller update loop that drives Storage_Pump
		// (`call 0x01E26570 / inc ebx / cmp ebx, 2`), outside the storage TU.
		// Never widen a controller bound past the seats that exist: at 4 with three
		// seats the engine reached controller 3's uninitialised command buffer and
		// crashed. Four is safe now that controller 3 has a seat and the Cbuf
		// records are [4].
		{0x0135C8CD, 0x02, 0x04, "per-controller update loop (Storage_Pump): 2 -> 4"},

		// Storage_Read refuses every controller >= 2 (`cmp edi, 2 / jge false`),
		// so no per-controller file was ever read for a guest.
		{0x0221AA7F, 0x02, 0x04, "Storage_Read controller bound: 2 -> 4"},

		// Two more of the identical shape, found with tools/bound_scan.py.
		{0x0221AC63, 0x02, 0x04, "storage fn 0x02277780 controller bound: 2 -> 4"},
		{0x0221AE3F, 0x02, 0x04, "storage fn 0x02277960 controller bound: 2 -> 4"},

		// TaskManager2_ProcessTasks per-controller loop (0x020F91D0, `cmp ebx, 2`).
		// Finished tasks were reaped only for controllers 0 and 1, so controller
		// 2's gamer-profile read stayed DONE. The 'hdd' busy query (0x02274D30)
		// checks one global task, so that unreaped task blocked storage for every
		// controller. Letting the game reap in its own frames is the fix; forcing
		// the reap from a detour took the renderer down.
		// Only safe together with the guest storage read filter: reaping a guest's
		// SETTINGS read runs autoexec and Settings_RunCallbacks with localClient
		// -1 and blacks out the client. With the filter, guests only read stats.
		{0x020ECA5B, 0x02, 0x04, "TaskManager2_ProcessTasks per-controller loop: 2 -> 4"},
	};
	inline constexpr uint32_t storage_pump_rva = 0x0221A680;
	inline constexpr uint8_t storage_pump_prologue[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x40};

	inline constexpr size_t base_table_rva = 0x17ADC958;
	inline constexpr size_t storage_pool_rva = 0x1789FD78;
	inline constexpr size_t lobby_pool_rva = 0x155FD410;
	inline constexpr size_t launch_sequence_rva = 0x156CA510;
	// fill_guests_when_ready: the session state word and Com_SessionMode_SetNetworkMode
	inline constexpr uint32_t session_state_rva = 0x1686E874;
	inline constexpr uint32_t set_network_mode_rva = 0x020EAE30;
	// ======== splitscreen/02_guest_storage.inl ========

	inline constexpr size_t targets_rva = 0x033BCDF0;
	inline constexpr targets_lea targets_leas[] = {
		{0x0221B146, 0x00}, {0x0221B28F, 0x00}, {0x0221B2E2, 0x00}, {0x0221B50A, 0x00},
		{0x0221B1A4, 0x08},
		{0x0221B223, 0x20}, {0x0221B203, 0x28},
		{0x0221B2BE, 0x30}, {0x0221B5D1, 0x30},
	};
	inline constexpr uint32_t targets_strides[] = {
		0x0221B152, 0x0221B1C4, 0x0221B20D, 0x0221B22D, 0x0221B28B, 0x0221B5E2,
	};
	inline constexpr uint32_t localfileop_lea = 0x022180CE; // 7-byte lea, disp32 at +3
	inline constexpr uint32_t local_client_num_rva = 0x020E3040;
	inline constexpr uint32_t cl_max_local_clients_rva = 0x05323720;

	inline constexpr size_t localfileop_rva = 0x17889DF0;
	// ======== splitscreen/03_signin_seats.inl ========

	inline constexpr uint32_t splitscreen_player_count_dvar_rva = 0x05355A00;
	inline constexpr uint32_t is_being_used_rva = 0x020E3210;
	inline constexpr uint32_t lc_controller_index_rva = 0x020E31B0;
	inline constexpr uint32_t live_user_is_signed_in_rva = 0x01EBA5A0;
	inline constexpr uint32_t settings_read_result_rva = 0x0164DC50;
	inline constexpr uint8_t settings_read_result_prologue[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x20};
	inline constexpr uint32_t storage_reset_rva = 0x0221AB10;
	inline constexpr uint32_t shoutcaster_read_result_rva = 0x01650640;
	inline constexpr uint8_t shoutcaster_read_result_prologue[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20};
	inline constexpr uint32_t storage_read_rva = 0x0221AA70;
	inline constexpr uint8_t storage_read_prologue[] = {0x48, 0x89, 0x5C, 0x24, 0x08};
	inline constexpr uint32_t splitscreen_player_count_rva = 0x027C1AB0;
	inline constexpr uint8_t splitscreen_player_count_prologue[] = {
		0x48, 0x8B, 0x0D, 0x49, 0x3F, 0xB9, 0x02, // mov rcx, [rip -> splitscreen_playerCount]
		0x48, 0x85, 0xC9,                         // test rcx, rcx
	};
	// CL_AllocatePerLocalClientMemory (0x0135D670, PS4 0x416A10) asks the count first:
	// `call splitscreen_player_count_rva` here. The count detour recognises the allocator
	// by this call's return address (count per match, docs/SIGNIN_REDESIGN.md D3).
	inline constexpr uint32_t alloc_count_call_rva = 0x0135D685;
	inline constexpr uint32_t alloc_count_return_rva = alloc_count_call_rva + 5;
	static_assert(alloc_count_return_rva + 0x01464426 == splitscreen_player_count_rva);
	inline constexpr uint32_t cl_init_rva = 0x01359410;
	inline constexpr uint32_t cl_init_range_imm_rva = 0x0135948B;
	inline constexpr uint32_t cbuf_execute_range_imm_rva = 0x020DFA30;
	inline constexpr uint32_t cl_frame_pump_imm_rva = 0x020ECE5E;
	inline constexpr uint32_t netchan_poll_imm_rva = 0x020EB424;
	inline constexpr uint32_t cg_frame_imms[] = {
		0x013E10D4,   // cmp r13d,2 - the r_num_viewports counting loop
		0x013E11D2,   // cmp ebx,2  - cgame frame loop, copy A exit 1
		0x013E11DF,   // cmp ebx,2  - cgame frame loop, copy A exit 2
		0x013E1264,   // cmp ebx,2  - cgame frame loop, copy B tail
		0x013E12A6,   // cmp ebx,2  - the loading-screen scan
	};
	inline constexpr uint32_t guest_signin_rva = 0x01DFFED0;
	inline constexpr uint32_t is_user_guest_imm_rva = 0x01EBA642;
	inline constexpr uint32_t gamepad_bound_rvas[] = {
		0x02284ADB,
		0x02284BCB,
		0x02285981,
		0x02285D89,
		0x01FD6E79,
		0x01FD7B0E,
	};
	inline constexpr uint32_t gamepad_rescan_rva = 0x02286010;
	inline constexpr uint8_t gamepad_rescan_bytes[] = {
		0x48, 0x83, 0xEC, 0x28,                      // sub rsp, 28h
		0xE8, 0xE7, 0xEF, 0xFF, 0xFF,                // call enumerate
		0xE8, 0xD2, 0xE9, 0xFF, 0xFF,                // call assign
		0x48, 0x83, 0xC4, 0x28,                      // add rsp, 28h
		0xE9, 0x49, 0xD1, 0xE5, 0xFF,                // jmp seat-model refresh
	};
	inline constexpr type_selector_site gamepad_type_selector_sites[] = {
		{0x022859A6, {0x33, 0xC9, 0x8D, 0x42, 0xFC, 0x83, 0xF8, 0x03, 0x0F, 0x46, 0xCD}},
		{0x02285DAA, {0x33, 0xC9, 0x8D, 0x42, 0xFC, 0x83, 0xF8, 0x03, 0x0F, 0x46, 0xCE}},
	};
	inline constexpr uint8_t gamepad_type_selector_fixed[11] = {
		0x8D, 0x42, 0xFC,                            // lea eax, [rdx-4]
		0x83, 0xF8, 0x04,                            // cmp eax, 4
		0x1B, 0xC9,                                  // sbb ecx, ecx
		0x83, 0xE1, 0x02,                            // and ecx, 2
	};

	// ======== splitscreen/04_panes.inl ========

	inline constexpr uint32_t uia_base_rva = 0x05359BC0;
	inline constexpr uint32_t uia_stride = 0x1078;   // clientUIActives: bytes per local client
	// relocate_flat24 arrays (try_apply): slot 0 and bytes per local client
	inline constexpr uint32_t scrplaceview_rva = 0x0577B800;
	inline constexpr uint32_t scrplaceview_stride = 0x7C;
	inline constexpr uint32_t perclient54_rva = 0x04CB32C0;
	inline constexpr uint32_t perclient54_stride = 0x54;
	inline constexpr uint32_t uielemhandles_rva = 0x1795CED8;
	inline constexpr uint32_t uielemhandles_stride = 0x2;
	// retarget_uielem_reader: movzx ecx, word [r15+r12*2+uiElemHandles]
	inline constexpr uint32_t uielem_reader_rva = 0x026F89D2;
	inline constexpr uint8_t uielem_reader_bytes[] = {0x43, 0x0F, 0xB7, 0x8C, 0x67, 0xD8, 0xCE, 0x95, 0x17};
	// widen_ui_registrar_bound: cmp edi, 2
	inline constexpr uint32_t ui_registrar_bound_rva = 0x01F26A52;
	inline constexpr uint8_t ui_registrar_bound_bytes[] = {0x83, 0xFF, 0x02};
	inline constexpr vp_patch viewparams_patches[] = {
		// wide scale on the total==0 path: 0x40 -> 0x100
		{0x010CB56F, 4, 3, false, 0x08, {0x48, 0xC1, 0xE0, 0x06}},
		// shared index lea: scale 2 -> 4
		{0x010CB5F0, 4, 3, false, 0x91, {0x48, 0x8D, 0x14, 0x51}},
		// horizontal path index lea: scale 2 -> 4
		{0x010CB5F6, 4, 3, false, 0x96, {0x48, 0x8D, 0x0C, 0x56}},
		// non-horizontal path index lea: scale 2 -> 4
		{0x010CB652, 4, 3, false, 0x96, {0x48, 0x8D, 0x04, 0x56}},
		// the two table base leas
		{0x010CB5FA, 7, 3, true, 0x00, {0x48, 0x8D, 0x15, 0xEF, 0xB0, 0x19, 0x02}},
		{0x010CB664, 7, 3, true, 0x00, {0x48, 0x8D, 0x15, 0x85, 0xB0, 0x19, 0x02}},
	};
	inline constexpr uint8_t ps4_view_params[0x200] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x80, 0x3E, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
		0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F,
	};
	inline constexpr flat24_site scrplace_sites[] = {
		{0x013E55C9, {0x48, 0x8D, 0x0D, 0x30, 0x62, 0x39, 0x04}}, // GetView
		{0x013E55E3, {0x48, 0x8D, 0x0D, 0x16, 0x62, 0x39, 0x04}}, // GetViewUIContext
		{0x013E5609, {0x48, 0x8D, 0x0D, 0xF0, 0x61, 0x39, 0x04}}, // GetViewWritable
	};
	inline constexpr flat24_site perclient54_sites[] = {
		{0x010E5056, {0x48, 0x8D, 0x05, 0x63, 0xE2, 0xBC, 0x03}}, // reset
		{0x0111715D, {0x48, 0x8D, 0x05, 0x5C, 0xC1, 0xB9, 0x03}}, // expiry check
	};
	inline constexpr flat24_site uielem_sites[] = {
		{0x01F269D6, {0x4C, 0x8D, 0x35, 0xFB, 0x64, 0xA3, 0x15}},
	};
	inline constexpr uint32_t isactive_rva = 0x027C18E0;
	inline constexpr uint8_t isactive_expected[] = {
		0x48, 0x63, 0xC1,                   // movsxd rax, ecx
		0x48, 0x8D, 0x0D,                   // lea rcx, [clientUIActives]
	};
	inline constexpr uint32_t get_active_count_rva = 0x027C18C0;
	inline constexpr uint8_t get_active_count_expected[] = {
		0x33, 0xC0,                                 // xor eax, eax
		0xF6, 0x05, 0xF7, 0x82, 0xB9, 0x02, 0x01,   // test byte [rip+..], 1
		0xB9, 0x01, 0x00, 0x00, 0x00,               // mov ecx, 1
		0x0F, 0x45, 0xC1,                           // cmovne eax, ecx
	};
	inline constexpr pane_bound pane_bounds[] = {
		{0x0132E2E4, 2, 0x02, 0x04, {0x83, 0xFB, 0x02}, 3},
	};
	inline constexpr uint32_t lui_ctx_bound_rva = 0x02683285;
	inline constexpr uint8_t lui_ctx_expected[] = {0x44, 0x3B, 0x3D, 0x94, 0x04, 0xCA, 0x02};
	inline constexpr uint8_t lui_ctx_patched[]  = {0x41, 0x83, 0xFF, 0x03, 0x90, 0x90, 0x90};
	inline constexpr uint32_t snapguard_rva = 0x01F3A82B;
	inline constexpr uint32_t snapguard_ret0 = 0x01F3A858;   // xor eax,eax; ...ret
	inline constexpr uint32_t snapguard_resume = 0x01F3A832; // the jne after the test
	inline constexpr uint8_t snapguard_expected[] = {
		0x48, 0x8B, 0x57, 0x30,   // mov rdx, [rdi+0x30]
		0xF6, 0x02, 0x10,         // test byte [rdx], 0x10
	};
	static_assert(snapguard_rva + sizeof(snapguard_expected) == snapguard_resume);   // the midhook returns to the jne
	inline constexpr uint32_t scene_a_rva = 0x0AE13DC8;
	inline constexpr uint32_t scene_b_rva = 0x0AE13DD8;
	inline constexpr uint32_t scene_size_rva = 0x0F43794C;
	inline constexpr uint32_t scene_b_store_rva = 0x01C87448;
	inline constexpr uint8_t scene_b_store_expect[] = {
		0x48, 0x89, 0x84, 0x33, 0xD8, 0x3D, 0xE1, 0x0A};
	inline constexpr uint32_t pcbuf_clear_rva = 0x01C87480;
	inline constexpr uint8_t pcbuf_expected[] = {0x48, 0x89, 0x5C, 0x24, 0x08};

	inline constexpr uint32_t scene_c_rva = 0x10596AE0;
	// ======== splitscreen/05_renderer_scene.inl ========

	inline constexpr uint32_t entcoll_world_base = 0x04764BA0;
	inline constexpr uint32_t entcoll_nodes_base = 0x032608B0;
	inline constexpr entcoll_site entcoll_world_sites[] = {
		{0x0058B173, 3, 7, true,  0x0028}, // lea rcx,[rip+..]
		{0x0058B1F6, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
		{0x0058B2DE, 3, 7, false, 0x0000}, // lea rdi,[rsi+0x047E3BA0]
		{0x0058B445, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
		{0x0058B60C, 3, 7, false, 0x0000}, // lea rdi,[rsi+0x047E3BA0]
		{0x0058B710, 3, 7, true,  0x0028}, // lea rax,[rip+..]
		{0x0058B886, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
		{0x0129DE34, 3, 7, true,  0x001C}, // lea rcx,[rip+..]
		{0x0129DF28, 3, 7, true,  0x001C}, // lea rcx,[rip+..]
		{0x0129E784, 3, 7, true,  0x001C}, // lea r10,[rip+..]
		{0x0129EA61, 3, 7, true,  0x001C}, // lea r10,[rip+..]
		{0x012AEC27, 3, 7, true,  0x001C}, // lea rdi,[rip+..]
		{0x012AEE47, 3, 7, true,  0x001C}, // lea rdi,[rip+..]
	};
	inline constexpr entcoll_site entcoll_node_sites[] = {
		{0x0058B17A, 3, 7, true,  0x0000}, // lea r10,[rip+..]
		{0x0058B2FD, 4, 8, false, 0x0000}, // mov rcx,[rsi+rbx*8+0x032DF8B0]
		{0x0058B44F, 3, 7, true,  0x0000}, // lea rcx,[rip+..]
		{0x0058B660, 4, 8, false, 0x0000}, // mov rsi,[rsi+rdx*8+..]
		{0x0058B721, 4, 8, false, 0x0000}, // mov r9,[rsi+r9*8+..]
		{0x0058B853, 4, 8, false, 0x0000}, // lea rdx,[r12+..]
		{0x0070DE25, 4, 8, false, 0x0000}, // mov rax,[rcx+rax*8+..]
		{0x0070FB49, 4, 8, false, 0x0000}, // mov rax,[rdx+rax*8+..]
		{0x0072E424, 4, 8, false, 0x0000}, // mov rax,[r8+r14*8+..]
		{0x0072E607, 4, 8, false, 0x0000}, // mov rax,[rcx+r14*8+..]
		{0x00731AA3, 4, 8, false, 0x0000}, // mov rax,[r12+r14*8+..]
		{0x00FEEED2, 3, 7, true,  0x0000}, // lea rsi,[rip+..]
		{0x0129DE45, 3, 7, true,  0x0000}, // lea r8,[rip+..]
		{0x0129DECC, 3, 7, true,  0x0000}, // lea r8,[rip+..]
		{0x0129DF21, 3, 7, true,  0x0000}, // lea r8,[rip+..]
		{0x0129E78B, 3, 7, true,  0x0000}, // lea r8,[rip+..]
		{0x0129E820, 3, 7, true,  0x0000}, // lea r8,[rip+..]
		{0x0129EA5A, 3, 7, true,  0x0000}, // lea r8,[rip+..]
		{0x012AEC63, 3, 7, true,  0x0000}, // lea rax,[rip+..]
	};

	// ======== splitscreen/06_relocations_a.inl ========

	inline constexpr entcoll_site players_kb_completion_sites[] = {
		{0x012F3574, 4, 9, false, 0x01A8}, // cmp byte ptr [rbx + r14 + 0x52f2b98], 0
		{0x012F357F, 4, 9, false, 0x01A9}, // cmp byte ptr [rbx + r14 + 0x52f2b99], 0
		{0x012F3591, 4, 9, false, 0x01A9}, // mov byte ptr [rbx + r14 + 0x52f2b99], 0
		{0x012F359A, 4, 9, false, 0x01C0}, // cmp byte ptr [rbx + r14 + 0x52f2bb0], 0
		{0x012F35A5, 4, 9, false, 0x01C1}, // cmp byte ptr [rbx + r14 + 0x52f2bb1], 0
		{0x012F35B7, 4, 9, false, 0x01C1}, // mov byte ptr [rbx + r14 + 0x52f2bb1], 0
		{0x012F35C0, 4, 9, false, 0x0220}, // cmp byte ptr [rbx + r14 + 0x52f2c10], 0
		{0x012F35CB, 4, 9, false, 0x0221}, // cmp byte ptr [rbx + r14 + 0x52f2c11], 0
		{0x012F35DD, 4, 9, false, 0x0221}, // mov byte ptr [rbx + r14 + 0x52f2c11], 0
		{0x012F35E6, 4, 9, false, 0x0238}, // cmp byte ptr [rbx + r14 + 0x52f2c28], 0
		{0x012F35F1, 4, 9, false, 0x0239}, // cmp byte ptr [rbx + r14 + 0x52f2c29], 0
		{0x012F3603, 4, 9, false, 0x0239}, // mov byte ptr [rbx + r14 + 0x52f2c29], 0
		{0x012F360C, 4, 9, false, 0x0250}, // cmp byte ptr [rbx + r14 + 0x52f2c40], 0
		{0x012F3617, 4, 9, false, 0x0251}, // cmp byte ptr [rbx + r14 + 0x52f2c41], 0
		{0x012F3629, 4, 9, false, 0x0251}, // mov byte ptr [rbx + r14 + 0x52f2c41], 0
		{0x012F3632, 4, 9, false, 0x0268}, // cmp byte ptr [rbx + r14 + 0x52f2c58], 0
		{0x012F363D, 4, 9, false, 0x0269}, // cmp byte ptr [rbx + r14 + 0x52f2c59], 0
		{0x012F364F, 4, 9, false, 0x0269}, // mov byte ptr [rbx + r14 + 0x52f2c59], 0
		{0x012F3658, 4, 9, false, 0x0280}, // cmp byte ptr [rbx + r14 + 0x52f2c70], 0
		{0x012F3663, 4, 9, false, 0x0281}, // cmp byte ptr [rbx + r14 + 0x52f2c71], 0
		{0x012F3675, 4, 9, false, 0x0281}, // mov byte ptr [rbx + r14 + 0x52f2c71], 0
		{0x012F367E, 4, 9, false, 0x0298}, // cmp byte ptr [rbx + r14 + 0x52f2c88], 0
		{0x012F3689, 4, 9, false, 0x0299}, // cmp byte ptr [rbx + r14 + 0x52f2c89], 0
		{0x012F369B, 4, 9, false, 0x0299}, // mov byte ptr [rbx + r14 + 0x52f2c89], 0
		{0x012F36A4, 4, 9, false, 0x02B0}, // cmp byte ptr [rbx + r14 + 0x52f2ca0], 0
		{0x012F36AF, 4, 9, false, 0x02B1}, // cmp byte ptr [rbx + r14 + 0x52f2ca1], 0
		{0x012F36C1, 4, 9, false, 0x02B1}, // mov byte ptr [rbx + r14 + 0x52f2ca1], 0
		{0x012F36CA, 4, 9, false, 0x02C8}, // cmp byte ptr [rbx + r14 + 0x52f2cb8], 0
		{0x012F36D5, 4, 9, false, 0x02C9}, // cmp byte ptr [rbx + r14 + 0x52f2cb9], 0
		{0x012F36E7, 4, 9, false, 0x02C9}, // mov byte ptr [rbx + r14 + 0x52f2cb9], 0
		{0x012F36F0, 4, 9, false, 0x0100}, // cmp byte ptr [rbx + r14 + 0x52f2af0], 0
		{0x012F36FB, 4, 9, false, 0x0101}, // cmp byte ptr [rbx + r14 + 0x52f2af1], 0
		{0x012F370D, 4, 9, false, 0x0101}, // mov byte ptr [rbx + r14 + 0x52f2af1], 0
		{0x012F3716, 4, 9, false, 0x02E0}, // cmp byte ptr [rbx + r14 + 0x52f2cd0], 0
		{0x012F3721, 4, 9, false, 0x02E1}, // cmp byte ptr [rbx + r14 + 0x52f2cd1], 0
		{0x012F3733, 4, 9, false, 0x02E1}, // mov byte ptr [rbx + r14 + 0x52f2cd1], 0
		{0x012F373C, 4, 9, false, 0x0328}, // cmp byte ptr [rbx + r14 + 0x52f2d18], 0
		{0x012F3747, 4, 9, false, 0x0329}, // cmp byte ptr [rbx + r14 + 0x52f2d19], 0
		{0x012F3789, 4, 9, false, 0x0329}, // mov byte ptr [rbx + r14 + 0x52f2d19], 0
		{0x012F3792, 4, 9, false, 0x0340}, // cmp byte ptr [rbx + r14 + 0x52f2d30], 0
		{0x012F379D, 4, 9, false, 0x0341}, // cmp byte ptr [rbx + r14 + 0x52f2d31], 0
		{0x012F37AF, 4, 9, false, 0x0341}, // mov byte ptr [rbx + r14 + 0x52f2d31], 0
		{0x012F37B8, 4, 9, false, 0x0358}, // cmp byte ptr [rbx + r14 + 0x52f2d48], 0
		{0x012F37C3, 4, 9, false, 0x0359}, // cmp byte ptr [rbx + r14 + 0x52f2d49], 0
		{0x012F37D5, 4, 9, false, 0x0359}, // mov byte ptr [rbx + r14 + 0x52f2d49], 0
		{0x012F37DE, 4, 9, false, 0x0370}, // cmp byte ptr [rbx + r14 + 0x52f2d60], 0
		{0x012F37E9, 4, 9, false, 0x0371}, // cmp byte ptr [rbx + r14 + 0x52f2d61], 0
		{0x012F37F8, 4, 9, false, 0x0371}, // mov byte ptr [rbx + r14 + 0x52f2d61], 0
		{0x012F3801, 4, 9, false, 0x0388}, // cmp byte ptr [rbx + r14 + 0x52f2d78], 0
		{0x012F380C, 4, 9, false, 0x0389}, // cmp byte ptr [rbx + r14 + 0x52f2d79], 0
		{0x012F381E, 4, 9, false, 0x0389}, // mov byte ptr [rbx + r14 + 0x52f2d79], 0
		{0x012F3827, 4, 9, false, 0x03A0}, // cmp byte ptr [rbx + r14 + 0x52f2d90], 0
		{0x012F3832, 4, 9, false, 0x03A1}, // cmp byte ptr [rbx + r14 + 0x52f2d91], 0
		{0x012F3844, 4, 9, false, 0x03A1}, // mov byte ptr [rbx + r14 + 0x52f2d91], 0
		{0x012F384D, 4, 9, false, 0x03B8}, // cmp byte ptr [rbx + r14 + 0x52f2da8], 0
		{0x012F3858, 4, 9, false, 0x03B9}, // cmp byte ptr [rbx + r14 + 0x52f2da9], 0
		{0x012F386A, 4, 9, false, 0x03B9}, // mov byte ptr [rbx + r14 + 0x52f2da9], 0
		{0x012F3873, 4, 9, false, 0x03D0}, // cmp byte ptr [rbx + r14 + 0x52f2dc0], 0
		{0x012F387E, 4, 9, false, 0x03D1}, // cmp byte ptr [rbx + r14 + 0x52f2dc1], 0
		{0x012F3890, 4, 9, false, 0x03D1}, // mov byte ptr [rbx + r14 + 0x52f2dc1], 0
		{0x012F3899, 4, 9, false, 0x03E8}, // cmp byte ptr [rbx + r14 + 0x52f2dd8], 0
		{0x012F38A4, 4, 9, false, 0x03E9}, // cmp byte ptr [rbx + r14 + 0x52f2dd9], 0
		{0x012F38B3, 4, 9, false, 0x03E9}, // mov byte ptr [rbx + r14 + 0x52f2dd9], 0
		{0x012F38BC, 4, 9, false, 0x0460}, // cmp byte ptr [rbx + r14 + 0x52f2e50], 0
		{0x012F38C7, 4, 9, false, 0x0461}, // cmp byte ptr [rbx + r14 + 0x52f2e51], 0
		{0x012F38D9, 4, 9, false, 0x0461}, // mov byte ptr [rbx + r14 + 0x52f2e51], 0
		{0x012F38E2, 4, 9, false, 0x02F8}, // cmp byte ptr [rbx + r14 + 0x52f2ce8], 0
		{0x012F38ED, 4, 9, false, 0x02F9}, // cmp byte ptr [rbx + r14 + 0x52f2ce9], 0
		{0x012F38FC, 4, 9, false, 0x02F9}, // mov byte ptr [rbx + r14 + 0x52f2ce9], 0
		{0x012F3905, 4, 9, false, 0x0400}, // cmp byte ptr [rbx + r14 + 0x52f2df0], 0
		{0x012F3910, 4, 9, false, 0x0401}, // cmp byte ptr [rbx + r14 + 0x52f2df1], 0
		{0x012F3922, 4, 9, false, 0x0401}, // mov byte ptr [rbx + r14 + 0x52f2df1], 0
		{0x012F392B, 4, 9, false, 0x0418}, // cmp byte ptr [rbx + r14 + 0x52f2e08], 0
		{0x012F3936, 4, 9, false, 0x0419}, // cmp byte ptr [rbx + r14 + 0x52f2e09], 0
		{0x012F3948, 4, 9, false, 0x0419}, // mov byte ptr [rbx + r14 + 0x52f2e09], 0
		{0x012F3951, 4, 9, false, 0x0430}, // cmp byte ptr [rbx + r14 + 0x52f2e20], 0
		{0x012F395C, 4, 9, false, 0x0431}, // cmp byte ptr [rbx + r14 + 0x52f2e21], 0
		{0x012F396E, 4, 9, false, 0x0431}, // mov byte ptr [rbx + r14 + 0x52f2e21], 0
		{0x012F3977, 4, 9, false, 0x0448}, // cmp byte ptr [rbx + r14 + 0x52f2e38], 0
		{0x012F3982, 4, 9, false, 0x0449}, // cmp byte ptr [rbx + r14 + 0x52f2e39], 0
		{0x012F3996, 4, 9, false, 0x0449}, // mov byte ptr [rbx + r14 + 0x52f2e39], 0
		{0x012F39A4, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
		{0x012F39AF, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
		{0x012F39C1, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
		{0x012F39CA, 4, 9, false, 0x02F8}, // cmp byte ptr [rbx + r14 + 0x52f2ce8], 0
		{0x012F39D5, 4, 9, false, 0x02F9}, // cmp byte ptr [rbx + r14 + 0x52f2ce9], 0
		{0x012F39E7, 4, 9, false, 0x02F9}, // mov byte ptr [rbx + r14 + 0x52f2ce9], 0
		{0x012F39F0, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
		{0x012F39FB, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
		{0x012F3A0D, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
		{0x012F3A16, 4, 9, false, 0x0310}, // cmp byte ptr [rbx + r14 + 0x52f2d00], 0
		{0x012F3A21, 4, 9, false, 0x0311}, // cmp byte ptr [rbx + r14 + 0x52f2d01], 0
		{0x012F3A33, 4, 9, false, 0x0311}, // mov byte ptr [rbx + r14 + 0x52f2d01], 0
		{0x012F3A3C, 4, 9, false, 0x0100}, // cmp byte ptr [rbx + r14 + 0x52f2af0], 0
		{0x012F3A47, 4, 9, false, 0x0101}, // cmp byte ptr [rbx + r14 + 0x52f2af1], 0
		{0x012F3A5E, 4, 9, false, 0x0101}, // mov byte ptr [rbx + r14 + 0x52f2af1], 0
		{0x012F3AB2, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
		{0x012F3ABD, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
		{0x012F3ACF, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
		{0x012F3AE1, 4, 9, false, 0x0130}, // cmp byte ptr [rbx + r14 + 0x52f2b20], 0
		{0x012F3AEC, 4, 9, false, 0x0131}, // cmp byte ptr [rbx + r14 + 0x52f2b21], 0
		{0x012F3AFE, 4, 9, false, 0x0131}, // mov byte ptr [rbx + r14 + 0x52f2b21], 0
		{0x012FEEE7, 3, 8, false, 0x0160}, // cmp byte ptr [rdi + rax + 0x52f2b50], 0
		{0x012FEEF1, 3, 8, false, 0x0161}, // cmp byte ptr [rdi + rax + 0x52f2b51], 0
		{0x012FEF41, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
		{0x012FEF4C, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
		{0x012FEF65, 4, 9, false, 0x0161}, // mov byte ptr [rdi + r14 + 0x52f2b51], 0
		{0x012FEF83, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
		{0x012FEF8E, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
		{0x012FF000, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
		{0x012FF00B, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
		{0x012FF0E3, 4, 9, false, 0x0160}, // cmp byte ptr [rdi + r14 + 0x52f2b50], 0
		{0x012FF0EE, 4, 9, false, 0x0161}, // cmp byte ptr [rdi + r14 + 0x52f2b51], 0
		{0x012FF101, 4, 9, false, 0x0161}, // mov byte ptr [rdi + r14 + 0x52f2b51], 0
		{0x012FF114, 4, 9, false, 0x0190}, // cmp byte ptr [rdi + r14 + 0x52f2b80], 0
		{0x012FF11F, 4, 9, false, 0x0191}, // cmp byte ptr [rdi + r14 + 0x52f2b81], 0
		{0x012FF130, 4, 9, false, 0x0178}, // cmp byte ptr [rdi + r14 + 0x52f2b68], 0
		{0x012FF13B, 4, 9, false, 0x0179}, // cmp byte ptr [rdi + r14 + 0x52f2b69], 0
		{0x012FF178, 4, 9, false, 0x0190}, // cmp byte ptr [rdi + r14 + 0x52f2b80], 0
		{0x012FF183, 4, 9, false, 0x0191}, // cmp byte ptr [rdi + r14 + 0x52f2b81], 0
		{0x012FF196, 4, 9, false, 0x0191}, // mov byte ptr [rdi + r14 + 0x52f2b81], 0
		{0x012FF19F, 4, 9, false, 0x0178}, // cmp byte ptr [rdi + r14 + 0x52f2b68], 0
		{0x012FF1AA, 4, 9, false, 0x0179}, // cmp byte ptr [rdi + r14 + 0x52f2b69], 0
		{0x012FF1BD, 4, 9, false, 0x0179}, // mov byte ptr [rdi + r14 + 0x52f2b69], 0
		{0x012FF1EA, 4, 9, false, 0x0190}, // cmp byte ptr [rdi + r14 + 0x52f2b80], 0
		{0x012FF1F5, 4, 9, false, 0x0191}, // cmp byte ptr [rdi + r14 + 0x52f2b81], 0
		{0x012FF208, 4, 9, false, 0x0191}, // mov byte ptr [rdi + r14 + 0x52f2b81], 0
		{0x012FF211, 4, 9, false, 0x0178}, // cmp byte ptr [rdi + r14 + 0x52f2b68], 0
		{0x012FF21C, 4, 9, false, 0x0179}, // cmp byte ptr [rdi + r14 + 0x52f2b69], 0
		{0x012FF22F, 4, 9, false, 0x0179}, // mov byte ptr [rdi + r14 + 0x52f2b69], 0
		{0x01306C23, 3, 8, false, 0x0208}, // cmp byte ptr [rbx + rsi + 0x52f2bf8], 0
		{0x01306C2D, 3, 8, false, 0x0209}, // cmp byte ptr [rbx + rsi + 0x52f2bf9], 0
		{0x01306C45, 3, 8, false, 0x0209}, // mov byte ptr [rbx + rsi + 0x52f2bf9], 0
		{0x01306C50, 3, 8, false, 0x01D8}, // cmp byte ptr [rbx + rsi + 0x52f2bc8], 0
		{0x01306C5A, 3, 8, false, 0x01D9}, // cmp byte ptr [rbx + rsi + 0x52f2bc9], 0
		{0x01306C71, 3, 8, false, 0x01F0}, // cmp byte ptr [rbx + rsi + 0x52f2be0], 0
		{0x01306C7B, 3, 8, false, 0x01F1}, // cmp byte ptr [rbx + rsi + 0x52f2be1], 0
		{0x01306CBF, 3, 8, false, 0x020A}, // cmp byte ptr [rbx + rsi + 0x52f2bfa], 0
		{0x01306CD0, 3, 8, false, 0x020A}, // mov byte ptr [rbx + rsi + 0x52f2bfa], 1
		{0x01306CF0, 4, 10, false, 0x01D9}, // mov word ptr [rbx + rsi + 0x52f2bc9], 0x100
		{0x01306CFA, 4, 10, false, 0x01F1}, // mov word ptr [rbx + rsi + 0x52f2be1], 0x100
		{0x01306D11, 3, 8, false, 0x020A}, // mov byte ptr [rbx + rsi + 0x52f2bfa], 0
		{0x01306D19, 3, 8, false, 0x01DA}, // cmp byte ptr [rbx + rsi + 0x52f2bca], 0
		{0x01306D28, 3, 8, false, 0x01D8}, // cmp byte ptr [rbx + rsi + 0x52f2bc8], 0
		{0x01306D32, 3, 8, false, 0x01D9}, // cmp byte ptr [rbx + rsi + 0x52f2bc9], 0
		{0x01306D44, 3, 8, false, 0x01D9}, // mov byte ptr [rbx + rsi + 0x52f2bc9], 0
		{0x01306D59, 3, 8, false, 0x01F2}, // cmp byte ptr [rbx + rsi + 0x52f2be2], 0
		{0x01306D68, 3, 8, false, 0x01F0}, // cmp byte ptr [rbx + rsi + 0x52f2be0], 0
		{0x01306D72, 3, 8, false, 0x01F1}, // cmp byte ptr [rbx + rsi + 0x52f2be1], 0
		{0x01306D84, 3, 8, false, 0x01F1}, // mov byte ptr [rbx + rsi + 0x52f2be1], 0
		{0x01308DD0, 4, 9, false, 0x01A8}, // mov byte ptr [rax + r14 + 0x52f2b98], 1
		{0x01308DD9, 4, 9, false, 0x01A9}, // mov byte ptr [rax + r14 + 0x52f2b99], 1
		{0x0131B73B, 4, 9, false, 0x02B0}, // cmp byte ptr [rdx + r8 + 0x52f2ca0], 0
		{0x0131B746, 4, 9, false, 0x0118}, // cmp byte ptr [rdx + r8 + 0x52f2b08], 0
		{0x0131B8BB, 4, 9, false, 0x02B0}, // cmp byte ptr [rdx + r8 + 0x52f2ca0], 0
		{0x0131B8C6, 4, 9, false, 0x0118}, // cmp byte ptr [rdx + r8 + 0x52f2b08], 0
		{0x0131BC5E, 4, 9, false, 0x02F8}, // cmp byte ptr [rdi + r15 + 0x52f2ce8], 0
		{0x0131BC71, 5, 11, false, 0x02F8}, // mov word ptr [rdi + r15 + 0x52f2ce8], 0x101
		{0x0131BCDA, 4, 9, false, 0x0118}, // cmp byte ptr [rdi + r15 + 0x52f2b08], 0
		{0x0131BCF1, 5, 11, false, 0x0118}, // mov word ptr [rdi + r15 + 0x52f2b08], 0x101
		{0x0131BD01, 4, 9, false, 0x02B0}, // cmp byte ptr [rdi + r15 + 0x52f2ca0], 0
		{0x0131BD10, 4, 9, false, 0x0118}, // cmp byte ptr [rdi + r15 + 0x52f2b08], 0
	};
	inline constexpr entcoll_site gamepads_completion_sites[] = {
		{0x02284AF2, 2, 7, true , 0x0074}, // cmp dword ptr [rip + 0x15b7c7bb], 8
		{0x022861A1, 4, 9, false, 0x0000}, // cmp byte ptr [rcx + r9 + 0x17e6e310], 0
	};
	inline constexpr entcoll_site players_kb_probe = {0x0131B2B1, 4, 8, false, 0x0198};   // reloc.hpp players_kb row
	inline constexpr uint32_t cf_buffer_rva = 0x049925A0;
	inline constexpr uint32_t cf_pointer_rva = 0x04992590;
	inline constexpr uint32_t cf_lea_sites[] = {0x008F26E3, 0x008F2DB0}; // 7 B, disp @3
	inline constexpr cf_imm cf_imms[] = {
		{0x00132F59, 3, 0x20000, 0x40000}, // mov r8d,[r9+0x20000]
		{0x00132F7F, 3, 0x20000, 0x40000}, // mov [r9+0x20000],eax
		{0x0013304D, 3, 0x20000, 0x40000}, // mov r8d,[r10+0x20000]
		{0x00133079, 3, 0x20000, 0x40000}, // mov [r10+0x20000],eax
		{0x001337F6, 4, 0x20000, 0x40000}, // lea r15,[r8*4+0x20000]
		{0x00136BD4, 4, 0x20000, 0x40000}, // lea r14,[rbx*4+0x20000]
		{0x0013444B, 2, 0x20008, 0x40010}, // mov r8d,0x20008  (clear)
		{0x00137160, 2, 0x20008, 0x40010}, // mov r8d,0x20008  (clear)
		{0x008F2DB9, 2, 0x20008, 0x40010}, // mov r8d,0x20008  (init memset)
	};
	inline constexpr entcoll_site entword_sites[] = {
		{0x020F3CF4, 3, 7, true,  0}, // lea rcx,[rip+..]
		{0x020F55FD, 4, 8, false, 0}, // mov word [rax+rdx*2+0x16DD3540],di
		{0x020F5689, 3, 7, true,  0}, // lea rcx,[rip+..]
		{0x020F5858, 3, 7, true,  0}, // lea rcx,[rip+..]   (the clear)
		{0x020F5967, 5, 9, false, 0}, // movsx rcx,word [rax+r10+..]  (crash site)
		{0x020F5999, 5, 9, false, 0}, // mov word [rax+r10+..],dx
		{0x020F5CB3, 5, 9, false, 0}, // movzx ecx,word cs:[rcx+rax*2+..]
		{0x020F8F4C, 3, 7, true,  0}, // lea rsi,[rip+..]
	};
	inline constexpr entword_imm entword_imms[] = {
		{0x020F5861, 2, 4, 0x1C08, 0x3820}, // mov r8d,0x1c08  - memset size
		{0x020F598F, 2, 4, 0x1C08, 0x3820}, // cmp rax,0x1c08  - the /GS bound
		{0x020F3CDF, 2, 4, 0x702, 0x704},   // imul ecx,ecx,0x702   ClearAllSkel(lc)
		{0x020F3CE5, 1, 4, 0x702, 0x704},   // mov edi,0x702        ClearAllSkel count
		{0x020F55EC, 3, 4, 0x702, 0x704},   // imul rdx,rdx,0x702   Com_ClientDObjCreate
		{0x020F5680, 2, 4, 0x702, 0x704},   // imul edx,edx,0x702   Com_GetClientDObj
		{0x020F58DE, 2, 4, 0x702, 0x704},   // imul edi,edi,0x702   Com_SafeClientDObjFree
		{0x020F5CA9, 3, 4, 0x702, 0x704},   // imul r13,r13,0x702   rebuild-all row
		{0x020F8D60, 2, 4, 0x702, 0x704},   // cmp esi,0x702        rebuild-all count
		{0x020F8F9F, 2, 4, 0x702, 0x704},   // cmp ebx,0x702        free-all count
		{0x020F8D7D, 3, 1, 0x02, 0x04},     // cmp r12d,2           rebuild-all clients
		{0x020F8FA9, 2, 1, 0x02, 0x04},     // cmp ebp,2            free-all clients
	};
	inline constexpr uint32_t exposure_base = 0x0F64EBB0;
	inline constexpr uint32_t exposure_stride = 0x110;
	inline constexpr uint32_t exposure_old_count = 3;
	inline constexpr entcoll_site exposure_base_sites[] = {
		{0x01CBF7C0, 3, 7, true, 0},   // lea rbx,[base]  free loop
		{0x01CC028F, 3, 7, true, 0},   // lea rcx,[base]  table fill
		{0x01CC06C4, 3, 7, true, 0},   // lea rbx,[base]  create loop
	};
	inline constexpr entcoll_site exposure_fill_end_site[] = {
		{0x01CBFF0C, 3, 7, true, exposure_old_count * exposure_stride},   // lea r13,[end]
	};
	inline constexpr entcoll_site exposure_all_end_sites[] = {
		{0x01CBF7C7, 3, 7, true, exposure_old_count * exposure_stride},   // lea rdi,[end] free
		{0x01CC06D2, 3, 7, true, exposure_old_count * exposure_stride},   // lea rdi,[end] create
	};
	inline constexpr uint32_t exposure_select_rva = 0x01C5FC0C;
	inline constexpr uint8_t exposure_select_stock[] = {
		0xB8, 0x02, 0x00, 0x00, 0x00,                   // mov eax,2
		0x75, 0x06,                                     // jne +6
		0x8B, 0x82, 0x98, 0x03, 0x00, 0x00,             // mov eax,[rdx+0x398]
		0x8B, 0xC8,                                     // mov ecx,eax
		0x48, 0x8B, 0x82, 0xB0, 0x03, 0x00, 0x00,       // mov rax,[rdx+0x3B0]
		0x48, 0x8B, 0xB4, 0xC8, 0xF0, 0x10, 0x00, 0x00, // mov rsi,[rax+rcx*8+0x10F0]
	};
	inline constexpr entcoll_site model_pool_sites[] = {
		{0x0200C70E, 3, 7, true , 0x00},   // lea rdx,[base]            AllocateNode
		{0x0200C75F, 3, 7, true , 0x00},   // lea rdx,[base]            AllocateNode
		{0x0200CA81, 3, 7, true , 0x00},   // lea rbx,[base]            FreeModel
		{0x0200CA9E, 3, 7, true , 0x00},   // lea rsi,[base]            FreeModel
		{0x0200CAD3, 3, 7, true , 0x00},   // lea rbx,[base]            FreeModel
		{0x0200CC9F, 3, 7, true , 0x00},   // lea rax,[base]            GetBool   (leaf, no .pdata)
		{0x0200CCCF, 3, 7, true , 0x08},   // lea rax,[base+8]          GetDataType (leaf)
		{0x0200CCEF, 3, 7, true , 0x00},   // lea rax,[base]            GetFunction (leaf)
		{0x0200CD2F, 3, 7, true , 0x00},   // lea rax,[base]            getter (leaf)
		{0x0200CE4D, 3, 7, true , 0x00},   // lea r11,[base]            GetModel
		{0x0200CFA0, 3, 7, true , 0x00},   // lea rax,[base]            GetReal   (leaf)
		{0x0200CFD4, 3, 7, true , 0x00},   // lea rax,[base]            getter (leaf)
		{0x0200CFFF, 3, 7, true , 0x00},   // lea rax,[base]            getter (leaf)
		{0x0200D20F, 5, 9, false, 0x20},   // movzx ebx,[r13+rax*8+base+0x20]  notify
		{0x0200D413, 3, 7, true , 0x20},   // lea rax,[base+0x20]       Reset (subscription heads)
		{0x0200D438, 3, 7, true , 0x22},   // lea rdi,[base+0x22]       Reset (persistent)
		{0x0200D498, 3, 7, true , 0x00},   // lea r9,[base]             typed get/set
		{0x0200D4FC, 3, 7, true , 0x00},   // lea rax,[base]
		{0x0200D555, 3, 7, true , 0x00},   // lea rax,[base]
		{0x0200D5AC, 3, 7, true , 0x00},   // lea rax,[base]
		{0x0200D5F7, 3, 7, true , 0x00},   // lea rax,[base]
		{0x0200D661, 3, 7, true , 0x00},   // lea r15,[base]            SetString
		{0x0200D74C, 3, 7, true , 0x00},   // lea rax,[base]
		{0x0200D7F1, 4, 8, false, 0x20},   // lea rdx,[rcx*8+base+0x20] Subscribe
		{0x0200D850, 3, 7, false, 0x20},   // lea rdx,[r10+base+0x20]   (leaf)
		{0x0200D97F, 4, 8, false, 0x20},   // movzx ecx,[rax+rbp+base+0x20]  unsubscribe
		{0x0200D9C7, 4, 8, false, 0x1A},   // movzx ecx,[rax+rbp+base+0x1A]
		{0x0200D9E7, 4, 8, false, 0x1E},   // movzx ebx,[rdi+rdx*8+base+0x1E]
	};
	inline constexpr uint32_t cbuf_exec_lea_rva = 0x020DFBC8;   // lea rax,[records] in Cbuf_ExecuteInternal
	inline constexpr uint8_t cbuf_exec_lea_head[] = {0x48, 0x8D, 0x05};
	inline constexpr uint32_t cbuf_range_check_rva = 0x020DFA2D;
	inline constexpr uint8_t cbuf_range_check_stock[] = {0x48, 0x83, 0xFB, 0x02, 0x73, 0x13};
	inline constexpr uint32_t cbuf_frame_bound_rva = 0x020ECDB3;
	inline constexpr uint8_t cbuf_frame_bound_stock[] = {0x83, 0xFE, 0x02, 0x7C, 0xE9};
	inline constexpr entcoll_site joinclient_sites[] = {
		{0x01ED81CF, 3, 7, true , 0x0},     // lea rbx,[base]          reset loop
		{0x01ED8223, 3, 7, true , 0x0},     // lea rcx,[base]          getter (leaf)
		{0x01ED825C, 2, 6, true , 0x0},     // mov [base],eax          init (leaf)
		{0x01ED8262, 3, 7, true , 0xAC},    // mov qword [base+0xAC]   init: slot0 ci, slot1 state
		{0x01ED8252, 2, 10, true, 0x15C},   // mov dword [base+0x15C],1  init: slot1 ci
		{0x01ED8298, 3, 7, true , 0x0},     // lea rax,[base]          agreement request handler
		{0x01ED8438, 3, 7, true , 0x0},     // lea rax,[base]
		{0x01ED86D5, 3, 7, true , 0xA8},    // lea rbx,[base+0xA8]     update loop start
		{0x02E904FF, 3, 7, true , 0x6E},    // lea rbx,[base+0x6E]     static ctor (ran already)
	};
	inline constexpr entcoll_site joinclient_end_sites[] = {
		{0x01ED81D8, 3, 7, true , 0x160},   // lea rsi,[base+2*0xB0]        reset loop end
		{0x01ED86DE, 3, 7, true , 0x208},   // lea r14,[base+0xA8+2*0xB0]   update loop end
	};
	inline constexpr uint32_t lobbymsg_bound_rva = 0x01EEC68E;
	inline constexpr uint8_t lobbymsg_bound_stock[] = {0x83, 0xFB, 0x02, 0x7C, 0xC5};   // cmp ebx,2 / jl
	inline constexpr uint32_t netchan_get_lea_rva = 0x0211BF51;                          // lea rax,[s_netchan]
	inline constexpr ctrl_check_patch lua_ctrl_checks[] = {
		{0x01F427FE, {0x83, 0xF9, 0x01, 0x0F, 0x87, 0x12, 0x18, 0x00, 0x00}, "Engine.GetClientNum"},
		{0x01F4FC2E, {0x83, 0xF9, 0x01, 0x0F, 0x87, 0x17, 0x18, 0x00, 0x00}, "Engine.GetPredictedClientNum"},
	};
	inline constexpr uint32_t hunk_create_rva = 0x02276DA0;
	inline constexpr uint8_t hunk_create_prologue[] = {
		0x49, 0x63, 0xC0,                          // movsxd rax,r8d
		0x4C, 0x8D, 0x1D, 0x46, 0xBB, 0x14, 0x01,  // lea r11,[rip+0x0114B486] (scheme table)
	};
	inline constexpr uint32_t model_pool_bound_rva = 0x0200D40E;
	inline constexpr uint8_t model_pool_bound_stock[] = {0xBE, 0x00, 0x90, 0x00, 0x00};   // mov esi,0x9000
	inline constexpr uint8_t model_pool_bound_new[] = {0xBE, 0xFF, 0xFF, 0x00, 0x00};     // mov esi,0xFFFF
	inline constexpr uint32_t sst_stride = 0x21F0;
	inline constexpr uint32_t sst_old_count = 4;
	inline constexpr uint32_t sst_new_count = 8;
	inline constexpr entcoll_site sst_sites[] = {
		{0x01D0E059, 3, 7, true, 0},            // lea rdx,[base]        alloc
		{0x01D0D598, 3, 7, true, sst_stride},   // lea rbp,[base+0x21F0] buffer creation
		{0x01D0DF0A, 3, 7, true, sst_stride},   // lea rbp,[base+0x21F0] free loop
		{0x02E8A83A, 3, 7, true, 0},            // lea rbx,[base]        static constructor
	};
	inline constexpr entcoll_site sst_end_site[] = {
		{0x01D0D5A6, 3, 7, true, sst_old_count * sst_stride},   // lea r14,[end] creation loop end
	};
	inline constexpr sst_imm sst_imms[] = {
		{0x01D0E056, 2, 1, 3, sst_new_count - 1},   // and eax,3 -> 7
		{0x01D0DF11, 2, 4, 4, sst_new_count},       // mov r14d,4 -> 8
		{0x02E8A841, 1, 4, 3, sst_new_count - 1},   // mov edi,3 (dec/jns) -> 7
	};
	inline constexpr entcoll_site cgdc_sites[] = {
		{0x008F0ABC, 3, 7, false, 0x0000}, // lea rbx, [rbx + 0x4a31cd0]
		{0x010AAC43, 5, 9, false, 0x002C}, // movss xmm0, dword ptr [rax + rcx + 0x4a31cfc]
	};
	inline constexpr uint32_t playerkeys_base = 0x0531D850;
	inline constexpr entcoll_site playerkeys_sites[] = {
		{0x012F2B9B, 3, 7, false, 0x1938}, // mov esi, dword ptr [rax + rsi + 0x539d988]
		{0x0133A477, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x4061bf2]
		{0x0133DE94, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x405e1d5]
		{0x0133F27A, 3, 7, false, 0x0138}, // lea r15, [r8 + 0x539c188]
		{0x0133F2C6, 4, 8, false, 0x0130}, // inc dword ptr [r8 + rbp + 0x539c180]
		{0x0133F2D5, 4, 8, false, 0x0130}, // dec dword ptr [r8 + rbp + 0x539c180]
		{0x0133F2DD, 4, 8, false, 0x0130}, // mov eax, dword ptr [r8 + rbp + 0x539c180]
		{0x0133F2E8, 4, 8, false, 0x0130}, // mov dword ptr [r8 + rbp + 0x539c180], eax
		{0x0133F668, 3, 7, false, 0x0138}, // lea r12, [r8 + 0x539c188]
		{0x0133F6C1, 4, 8, false, 0x0130}, // inc dword ptr [rbx + r8 + 0x539c180]
		{0x0133F6D0, 4, 8, false, 0x0130}, // dec dword ptr [rbx + r8 + 0x539c180]
		{0x0133F6D8, 4, 8, false, 0x0130}, // mov eax, dword ptr [rbx + r8 + 0x539c180]
		{0x0133F6E3, 4, 8, false, 0x0130}, // mov dword ptr [rbx + r8 + 0x539c180], eax
		{0x0133FEC2, 4, 8, false, 0x0138}, // lea r12, [r12 + 0x539c188]
		{0x0133FF22, 3, 7, false, 0x0130}, // inc dword ptr [rdi + rax + 0x539c180]
		{0x013405A4, 3, 7, true , 0x1938}, // lea rax, [rip + 0x405d3fd]
		{0x01340E1F, 3, 7, true , 0x0000}, // lea rax, [rip + 0x405b24a]
		{0x01341D93, 3, 7, true , 0x0138}, // lea rcx, [rip + 0x405a40e]
		{0x01341E42, 3, 7, true , 0x0000}, // lea r12, [rip + 0x405a227]
		{0x01341ED9, 3, 7, true , 0x0000}, // lea r13, [rip + 0x405a190]
		{0x01341F93, 3, 7, true , 0x0000}, // lea r13, [rip + 0x405a0d6]
		{0x01342029, 3, 7, true , 0x0000}, // lea r13, [rip + 0x405a040]
		{0x01342261, 3, 7, false, 0x0138}, // lea r13, [rdx + 0x539c188]
		{0x013422C6, 3, 7, false, 0x0130}, // inc dword ptr [rsi + rdx + 0x539c180]
		{0x013422D4, 3, 7, false, 0x0130}, // dec dword ptr [rsi + rdx + 0x539c180]
		{0x013422DB, 3, 7, false, 0x0130}, // mov eax, dword ptr [rsi + rdx + 0x539c180]
		{0x013422E6, 3, 7, false, 0x0130}, // mov dword ptr [rsi + rdx + 0x539c180], eax
		{0x01343E68, 4, 8, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], eax
		{0x01343E7E, 4, 12, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], 3
		{0x01343E9D, 4, 9, false, 0x1938}, // cmp dword ptr [rsi + r9 + 0x539d988], 2
		{0x01343EB8, 4, 8, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], r8d
		{0x01343EC2, 4, 8, false, 0x1938}, // mov dword ptr [rsi + r9 + 0x539d988], r12d
		{0x0134499C, 4, 8, false, 0x1038}, // mov ebp, dword ptr [rax + r15 + 0x539d088]
		{0x013449A4, 4, 8, false, 0x1020}, // mov edi, dword ptr [rax + r15 + 0x539d070]
		{0x013449AC, 4, 8, false, 0x1008}, // mov r14d, dword ptr [rax + r15 + 0x539d058]
		{0x01345375, 3, 7, true , 0x012C}, // lea rax, [rip + 0x4056e20]
		{0x01345650, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4056a19]
		{0x013456F0, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4056979]
		{0x013457EF, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x405687a]
		{0x013459E0, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4056689]
		{0x01345DC2, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x40562a7]
		{0x01345F97, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x40560d2]
		{0x013462E1, 4, 8, false, 0x0140}, // cmp r14d, dword ptr [rax + r11 + 0x539c190]
		{0x0134638D, 4, 8, false, 0x0144}, // cmp r11d, dword ptr [rax + r14 + 0x539c194]
		{0x013463D8, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r15d
		{0x013463E0, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
		{0x0134640C, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r15d
		{0x01346414, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
		{0x01346468, 3, 7, false, 0x0148}, // lea rcx, [rax + 0x539c198]
		{0x0134648B, 4, 8, false, 0x0140}, // mov dword ptr [rsi + rax + 0x539c190], r14d
		{0x0134649A, 4, 12, false, 0x193C}, // mov dword ptr [rdi + r14 + 0x539d98c], 1
		{0x013464F5, 4, 8, false, 0x0140}, // cmp r10d, dword ptr [rcx + r14 + 0x539c190]
		{0x01346537, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r11d
		{0x0134653F, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
		{0x01346566, 4, 8, false, 0x0144}, // mov dword ptr [rax + r14 + 0x539c194], r11d
		{0x0134656E, 4, 12, false, 0x193C}, // mov dword ptr [rdx + r14 + 0x539d98c], 1
		{0x0134674A, 3, 7, true , 0x0138}, // lea rcx, [rip + 0x4055a57]
		{0x01346917, 3, 7, false, 0x0148}, // lea rcx, [rsi + 0x539c198]
		{0x01346928, 4, 8, false, 0x0140}, // mov qword ptr [rbx + rsi + 0x539c190], rax
		{0x01346930, 3, 11, false, 0x193C}, // mov dword ptr [rdi + rsi + 0x539d98c], 1
		{0x01346B24, 3, 7, true , 0x0144}, // lea rax, [rip + 0x4055689]
		{0x01346BD2, 3, 7, true , 0x0138}, // lea rax, [rip + 0x40555cf]
		{0x01346C62, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4055407]
		{0x01346D0B, 3, 7, true , 0x0140}, // lea rcx, [rip + 0x405549e]
		{0x01346E37, 3, 7, true , 0x0138}, // lea rax, [rip + 0x405536a]
		{0x01346EE2, 3, 7, true , 0x0140}, // lea r13, [rip + 0x40552c7]
		{0x0134700D, 3, 7, true , 0x0140}, // lea rax, [rip + 0x405519c]
		{0x013470A0, 3, 7, true , 0x0138}, // lea rax, [rip + 0x4055101]
		{0x0134724D, 3, 7, true , 0x0138}, // lea rcx, [rip + 0x4054f54]
		{0x013475E6, 3, 7, true , 0x193C}, // lea rdi, [rip + 0x40563bf]
		{0x013478B4, 3, 7, true , 0x0000}, // lea r15, [rip + 0x40547b5]
		{0x01347993, 3, 7, true , 0x193C}, // lea rcx, [rip + 0x4056012]
		{0x013479FA, 3, 7, true , 0x0148}, // lea rsi, [rip + 0x40547b7]
		{0x01347C68, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4054401]
		{0x01347D9C, 3, 7, true , 0x0000}, // lea r10, [rip + 0x40542cd]
		{0x01347E2F, 3, 7, true , 0x0000}, // lea r12, [rip + 0x405423a]
		{0x01347E3B, 3, 7, true , 0x0148}, // lea rax, [rip + 0x4054376]
		{0x01347E77, 3, 7, true , 0x0148}, // lea rax, [rip + 0x405433a]
		{0x01347F7F, 3, 7, true , 0x0138}, // lea rax, [rip + 0x4054222]
		{0x013481CE, 3, 7, true , 0x0000}, // lea r8, [rip + 0x4053e9b]
		{0x01DDE2EA, 3, 7, true , 0x0000}, // lea rax, [rip + 0x35b12cf]
		// Not a site: 0x0219DA4D is unreachable Arxan filler whose bytes happen
		// to equal playerKeys+0x2994. Never write unproven bytes.
	};
	inline constexpr entcoll_site notetracklerps_sites[] = {
		{0x00248E3F, 3, 7, true , 0x0000}, // lea rdx, [rip + 0x45812ea]
		{0x00249355, 3, 7, true , 0x0034}, // lea rcx, [rip + 0x4580e08]
		{0x0024E039, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x457c0f0]
		{0x0026EE8A, 4, 9, false, 0x0000}, // cmp dword ptr [rdi + r10 + 0x47ca130], 3
		{0x0026EE99, 4, 8, false, 0x002C}, // movsxd r8, dword ptr [rdi + r10 + 0x47ca15c]
		{0x002735C3, 4, 8, false, 0x0000}, // mov eax, dword ptr [rdi + r10 + 0x47ca130]
		{0x002735CF, 4, 8, false, 0x0030}, // mov edx, dword ptr [rdi + r10 + 0x47ca160]
		{0x002735D7, 3, 7, true , 0x0014}, // lea rax, [rip + 0x4556b66]
		{0x00273605, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
		{0x0027361B, 4, 8, false, 0x0014}, // mov eax, dword ptr [rdi + r10 + 0x47ca144]
		{0x0027366C, 4, 12, false, 0x0000}, // mov dword ptr [rdi + r10 + 0x47ca130], 3
		{0x00275949, 4, 9, false, 0x0000}, // cmp dword ptr [rdi + r10 + 0x47ca130], 3
		{0x0027595F, 4, 8, false, 0x002C}, // movsxd r8, dword ptr [rdi + r10 + 0x47ca15c]
		{0x0027A063, 4, 8, false, 0x0024}, // mov edx, dword ptr [rdi + r10 + 0x47ca154]
		{0x0027A06B, 4, 8, false, 0x0028}, // mov r8d, dword ptr [rdi + r10 + 0x47ca158]
		{0x0027A07F, 4, 8, false, 0x0000}, // mov eax, dword ptr [rdi + r10 + 0x47ca130]
		{0x0027A0AA, 6, 10, false, 0x0004}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca134]
		{0x0027A0B4, 6, 10, false, 0x0008}, // movss xmm2, dword ptr [rdi + r10 + 0x47ca138]
		{0x0027A0BE, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
		{0x0027A0C8, 4, 8, false, 0x0030}, // mov edx, dword ptr [rdi + r10 + 0x47ca160]
		{0x0027A0E7, 6, 10, false, 0x0018}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca148]
		{0x0027A0FA, 6, 10, false, 0x000C}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca13c]
		{0x0027A10C, 6, 10, false, 0x001C}, // movss xmm2, dword ptr [rdi + r10 + 0x47ca14c]
		{0x0027A11F, 6, 10, false, 0x0010}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca140]
		{0x0027A131, 6, 10, false, 0x0020}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca150]
		{0x0027A167, 6, 10, false, 0x0004}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca134]
		{0x0027A171, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
		{0x0027A1A8, 6, 10, false, 0x0004}, // movss xmm0, dword ptr [rdi + r10 + 0x47ca134]
		{0x0027A1B2, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
		{0x0027A1D9, 4, 8, false, 0x0030}, // mov edx, dword ptr [rdi + r10 + 0x47ca160]
		{0x0027A1F5, 3, 7, true , 0x0014}, // lea rax, [rip + 0x454ff48]
		{0x0027A216, 6, 10, false, 0x0014}, // movss xmm1, dword ptr [rdi + r10 + 0x47ca144]
		{0x0027A22C, 4, 8, false, 0x0014}, // mov eax, dword ptr [rdi + r10 + 0x47ca144]
		{0x0027A273, 4, 12, false, 0x0000}, // mov dword ptr [rdi + r10 + 0x47ca130], 3
	};
	inline constexpr entcoll_site cg_pmove_sites[] = {
		{0x00926DC4, 2, 6, true , 0x0340}, // mov dword ptr [rip + 0x43f1cb6], esi
		{0x00926DCA, 3, 7, true , 0x0344}, // mov byte ptr [rip + 0x43f1cb3], sil
		{0x00926DD1, 2, 10, true , 0x02D0}, // mov dword ptr [rip + 0x43f1c35], 0x7e967699
		{0x00926DDB, 2, 10, true , 0x02D4}, // mov dword ptr [rip + 0x43f1c2f], 0x7e967699
		{0x00926DE5, 2, 10, true , 0x02D8}, // mov dword ptr [rip + 0x43f1c29], 0x7e967699
		{0x00926DEF, 2, 10, true , 0x02E0}, // mov dword ptr [rip + 0x43f1c27], 0xfe967699
		{0x00926DF9, 2, 10, true , 0x02E4}, // mov dword ptr [rip + 0x43f1c21], 0xfe967699
		{0x00926E03, 2, 10, true , 0x02E8}, // mov dword ptr [rip + 0x43f1c1b], 0xfe967699
		{0x00926E0D, 2, 6, true , 0x19A0}, // mov dword ptr [rip + 0x43f32cd], esi
		{0x00926E14, 2, 6, true , 0x19A4}, // mov byte ptr [rip + 0x43f32ca], dh
		{0x00926E1A, 2, 10, true , 0x1930}, // mov dword ptr [rip + 0x43f324c], 0x7e967699
		{0x00926E24, 2, 10, true , 0x1934}, // mov dword ptr [rip + 0x43f3246], 0x7e967699
		{0x00926E2E, 2, 10, true , 0x1938}, // mov dword ptr [rip + 0x43f3240], 0x7e967699
		{0x00926E38, 2, 10, true , 0x1940}, // mov dword ptr [rip + 0x43f323e], 0xfe967699
		{0x00926E42, 2, 10, true , 0x1944}, // mov dword ptr [rip + 0x43f3238], 0xfe967699
		{0x00926E4C, 2, 10, true , 0x1948}, // mov dword ptr [rip + 0x43f3232], 0xfe967699
		{0x009D18A0, 4, 8, false, 0x02B0}, // mov dword ptr [rsi + r15 + 0x4d189f0], ebx
		{0x009D18A8, 3, 7, false, 0x00A8}, // lea rbx, [r15 + 0x4d187e8]
		{0x009D18AF, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi + 0x4d18740], r13
		{0x009D18BA, 4, 9, false, 0x02AC}, // mov byte ptr [rsi + r15 + 0x4d189ec], 0
		{0x009D1915, 4, 12, false, 0x0294}, // mov dword ptr [rsi + r15 + 0x4d189d4], 0
		{0x009D1927, 4, 8, false, 0x0290}, // mov dword ptr [rsi + r15 + 0x4d189d0], eax
		{0x009D1C16, 3, 7, false, 0x0008}, // lea rbx, [r15 + 0x4d18748]
		{0x009D1C6D, 3, 7, false, 0x0000}, // lea rcx, [r15 + 0x4d18740]
		{0x009D1CD0, 3, 7, false, 0x0008}, // lea r13, [rax + 0x4d18748]
		{0x009D1CF4, 3, 7, false, 0x0008}, // mov eax, dword ptr [rsi + rcx + 0x4d18748]
		{0x009D1D10, 3, 7, false, 0x0058}, // lea r8, [rcx + 0x4d18798]
		{0x009D1E56, 4, 8, false, 0x0000}, // mov rax, qword ptr [rsi + rcx + 0x4d18740]
		{0x009D1E5E, 3, 7, false, 0x0000}, // lea rcx, [rcx + 0x4d18740]
		{0x009D1E8D, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rax + 0x4d18740]
		{0x009D1F36, 4, 8, false, 0x0000}, // mov rax, qword ptr [rsi + rax + 0x4d18740]
		{0x009D1F7A, 3, 7, false, 0x0000}, // lea rcx, [r14 + 0x4d18740]
		{0x009D1FE1, 6, 10, false, 0x0294}, // movss xmm8, dword ptr [rsi + r14 + 0x4d189d4]
		{0x009D2000, 4, 8, false, 0x0290}, // cmp dword ptr [rsi + r14 + 0x4d189d0], eax
		{0x009D2086, 4, 8, false, 0x0290}, // mov eax, dword ptr [rsi + r14 + 0x4d189d0]
		{0x009D2106, 4, 8, false, 0x0290}, // mov eax, dword ptr [rsi + r14 + 0x4d189d0]
		{0x010BBA0C, 4, 9, false, 0x1618}, // cmp dword ptr [r14 + rax + 0x4d19d58], 0
		{0x010BBA17, 3, 7, false, 0x161C}, // lea rbx, [rax + 0x4d19d5c]
		{0x010BBA53, 4, 8, false, 0x1618}, // cmp edi, dword ptr [r14 + r15 + 0x4d19d58]
		{0x010C153D, 4, 9, false, 0x1618}, // cmp dword ptr [r12 + rax + 0x4d19d58], 0
		{0x010C1548, 3, 7, false, 0x161C}, // lea rdi, [rax + 0x4d19d5c]
		{0x010C1583, 4, 8, false, 0x1618}, // cmp r14d, dword ptr [r12 + r13 + 0x4d19d58]
		{0x023A0ACE, 3, 7, true , 0x0000}, // mov rdx, qword ptr [rip + 0x28feafb]
		{0x023AAE5A, 4, 9, false, 0x0000}, // cmp qword ptr [rax + r15 + 0x4d18740], 0
		{0x02621DD8, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x267d7f1]
		{0x02CCE142, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x1fd0ff7]
		{0x02EF9657, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x1da0082]
	};
	inline constexpr entcoll_site camerashake_sites[] = {
		{0x005830A3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42608e6]
		{0x0058491C, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425f06d]
		{0x0058651E, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425d46b]
	};
	inline constexpr entcoll_site moverinfos_sites[] = {
		{0x004CB9D4, 3, 7, true , 0x0000}, // lea rax, [rip + 0x43177d5]
		{0x004F0FDB, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42f21ce]
		{0x02CCD75A, 3, 7, true , 0x0000}, // lea rbx, [rip + 0x1a9c44f]
	};
	inline constexpr entcoll_site moveinfoentnum_sites[] = {
		{0x004F0FCA, 3, 7, false, 0x0000}, // lea rsi, [r11 + 0x47e3140]
	};
	inline constexpr entcoll_site rumble_sites[] = {
		{0x009E6E50, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4336619]
		{0x009E6E99, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x43365d0]
		{0x009E6F03, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4336566]
		{0x009E6F98, 3, 7, true , 0x0000}, // lea rax, [rip + 0x43364d1]
		{0x009E7033, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4336436]
		{0x009E70AB, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x43363be]
		{0x009E7112, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4336357]
		{0x009EF6A1, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x432ddc8]
	};
	inline constexpr entcoll_site atglob_sites[] = {
		{0x000771E3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36085d6]
		{0x0007AA64, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x3604d55]
		{0x0007E103, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36016b6]
		{0x00086724, 4, 8, false, 0x1600}, // mov dword ptr [rax + rdx + 0x3680dc0], r14d
	};
	inline constexpr entcoll_site aimtargetcmd_sites[] = {
		{0x0007A991, 3, 7, true , 0x0000}, // lea r12, [rip + 0x3604de8]
		{0x0008672C, 4, 8, false, 0x0004}, // mov dword ptr [rdx + r11*8 + 0x367f784], r14d
		{0x0008978A, 4, 8, false, 0x0004}, // movsxd rdx, dword ptr [rax + r11*8 + 0x367f784]
		{0x000897BA, 4, 8, false, 0x0004}, // mov dword ptr [rdx + r11*8 + 0x367f784], eax
	};
	inline constexpr entcoll_site arcdata_sites[] = {
		{0x005F11E7, 3, 7, true , 0x0000}, // lea r14, [rip + 0x42270c2]
		{0x005F66A0, 3, 7, true , 0x0000}, // lea r9, [rip + 0x4221c09]
		{0x005F6852, 3, 7, true , 0x0000}, // lea r9, [rip + 0x4221a57]
		{0x005F6981, 3, 7, true , 0x0000}, // lea r9, [rip + 0x4221928]
		{0x005F8562, 3, 7, true , 0x0000}, // lea r9, [rip + 0x421fd47]
		{0x005F86BE, 3, 7, true , 0x0000}, // lea r9, [rip + 0x421fbeb]
		{0x005FBB81, 4, 8, false, 0x0E60}, // mov dword ptr [rsi + r12 + 0x4819110], r15d
		{0x005FBBBD, 4, 8, false, 0x0E60}, // mov dword ptr [rsi + r12 + 0x4819110], r15d
		{0x005FBBC5, 5, 11, false, 0x0EE8}, // mov word ptr [rsi + r12 + 0x4819198], 0x100
		{0x005FF962, 3, 8, false, 0x0EE8}, // mov byte ptr [rsi + rax + 0x4819198], 1
		{0x005FF97B, 3, 8, false, 0x0EE9}, // mov byte ptr [rsi + rax + 0x4819199], 1
		{0x005FF98C, 3, 8, false, 0x0E60}, // cmp dword ptr [rsi + rax + 0x4819110], 1
		{0x005FF996, 3, 8, false, 0x0EE9}, // mov byte ptr [rsi + rax + 0x4819199], 0
	};
	inline constexpr entcoll_site session_member_sites[] = {
		{0x020E4180, 3, 7, true , 0x0000}, // lea rcx, [arr]           clear
		{0x020E6224, 3, 7, false, 0x0000}, // lea rdx, [rax + arr]     rax = image base
		{0x020E6260, 3, 7, true , 0x0021}, // lea rax, [arr + 0x21]
		{0x020E6271, 3, 7, false, 0x0001}, // lea rsi, [rsi + arr + 1] image-base relative
	};
	inline constexpr uint32_t session_member_clear_rva = 0x020E4189;
	inline constexpr entcoll_site aaglob_v2_sites[] = {
		{0x0002D7B6, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3647a13]
		{0x0002DAD3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36476f6]
		{0x0002F70F, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3645aba]
		{0x0002FC45, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3645584]
		{0x0002FFD1, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36451f8]
		{0x00034D16, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36404b3]
		{0x000369C8, 3, 7, true , 0x0000}, // lea rax, [rip + 0x363e801]
		{0x00039BB9, 3, 7, false, 0x0000}, // lea rsi, [rcx + 0x36751d0]
		{0x0003FF57, 3, 7, false, 0x4E18}, // mov ebx, dword ptr [rax + rbx + 0x3679fe8]
		{0x00043773, 3, 7, true , 0x4E20}, // lea rdx, [rip + 0x3636876]
		{0x00043793, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x3631a36]
		{0x00043E7D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x363134c]
		{0x000457A3, 3, 7, true , 0x4E20}, // lea rcx, [rip + 0x3634846]
		{0x0004EDEE, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36263db]
		{0x000520D3, 3, 7, true , 0x2814}, // lea rcx, [rip + 0x362590a]
		{0x00052104, 3, 7, true , 0x0214}, // lea rcx, [rip + 0x36232d9]
		{0x0005219A, 3, 7, true , 0x0000}, // lea rax, [rip + 0x362302f]
		{0x00056D33, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x361e496]
		{0x0005B969, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3619860]
		{0x0005BA60, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3619769]
		{0x0005D383, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3617e46]
		{0x000604DE, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3614ceb]
		{0x000639B3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3611816]
		{0x00065353, 3, 7, true , 0x0000}, // lea rax, [rip + 0x360fe76]
		{0x00066CC7, 3, 7, true , 0x0000}, // lea rax, [rip + 0x360e502]
		{0x00066D50, 3, 7, true , 0x0000}, // lea rax, [rip + 0x360e479]
		{0x0006DC2A, 3, 7, false, 0x00C8}, // lea rsi, [r13 + 0x3675298]
		{0x0006DC47, 6, 10, false, 0x01C0}, // movss dword ptr [rdi + r13 + 0x3675390], xmm6
		{0x0006DC6E, 3, 7, false, 0x00CC}, // lea r12, [r13 + 0x367529c]
		{0x0006DC8E, 6, 10, false, 0x01C4}, // movss dword ptr [rdi + r13 + 0x3675394], xmm6
		{0x0006DC98, 4, 9, false, 0x018D}, // cmp byte ptr [rdi + r13 + 0x367535d], 0
		{0x0006DCA3, 6, 10, false, 0x0194}, // movss xmm6, dword ptr [rdi + r13 + 0x3675364]
		{0x0006DCE4, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
		{0x0006DCEE, 6, 10, false, 0x01C8}, // movss dword ptr [rdi + r13 + 0x3675398], xmm6
		{0x0006DD24, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
		{0x0006DD2E, 6, 10, false, 0x01CC}, // movss dword ptr [rdi + r13 + 0x367539c], xmm6
		{0x0006DD64, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
		{0x0006DD6E, 6, 10, false, 0x01D0}, // movss dword ptr [rdi + r13 + 0x36753a0], xmm6
		{0x0006DDA7, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
		{0x0006DDB1, 6, 10, false, 0x01D4}, // movss dword ptr [rdi + r13 + 0x36753a4], xmm6
		{0x0006DDE7, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
		{0x0006DDF1, 6, 10, false, 0x01D8}, // movss dword ptr [rdi + r13 + 0x36753a8], xmm6
		{0x0006DE64, 6, 10, false, 0x00B4}, // mulss xmm6, dword ptr [rdi + r13 + 0x3675284]
		{0x0006DE6E, 6, 10, false, 0x01DC}, // movss dword ptr [rdi + r13 + 0x36753ac], xmm6
		{0x0006F5EB, 5, 9, false, 0x01E0}, // movss dword ptr [rax + rdx + 0x36753b0], xmm6
		{0x00070E6D, 5, 9, false, 0x01E4}, // movss dword ptr [rax + rcx + 0x36753b4], xmm6
		{0x00071B48, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3603681]
		{0x00073644, 3, 7, true , 0x0000}, // lea rax, [rip + 0x3601b85]
		{0x000753E0, 3, 7, true , 0x0000}, // lea rax, [rip + 0x35ffde9]
		{0x02C6A7A5, 3, 7, true , 0x01B4}, // lea rax, [rip + 0x9915d8]
	};
	inline constexpr perclient_array aaglob_array = {
		"aaGlobArray", 0x035F61D0, 0x4E30, aaglob_v2_sites, std::size(aaglob_v2_sites), 0, {}};
	inline constexpr entcoll_site totalcoverage_sites[] = {
		{0x0125F881, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x3ae2bb8]
		{0x01262A2B, 3, 7, true , 0x0008}, // lea rax, [rip + 0x3adfa16]
		{0x01262B66, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x3adf8d3]
		{0x01262B71, 3, 7, true , 0x0018}, // lea rax, [rip + 0x3adf8e0]
		{0x012633A3, 3, 7, true , 0x0018}, // lea rcx, [rip + ..]  (missed by both scans)
		{0x01264CA6, 3, 7, true , 0x0004}, // lea r10, [rip + 0x3add797]
	};
	inline constexpr entcoll_site gaglobs_sites[] = {
		{0x0133F00B, 3, 7, true , 0x0000}, // lea rax, [rip + 0x405c6de]
		{0x0133F085, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x405c664]
		{0x0133F5AA, 3, 7, true , 0x0000}, // lea rax, [rip + 0x405c13f]
		{0x0133FB06, 3, 7, false, 0x0000}, // lea r14, [r13 + 0x539b6d0]
		{0x0133FD4E, 3, 7, false, 0x0000}, // lea r9, [r10 + 0x539b6d0]
		{0x01340140, 3, 7, true , 0x001C}, // lea r8, [rip + 0x405b5c5]
		{0x0134067B, 3, 7, false, 0x0000}, // lea rdi, [r15 + 0x539b6d0]
	};
	inline constexpr entcoll_site gamepadbuttons_sites[] = {
		{0x013402B6, 4, 8, false, 0x0002}, // lea rdi, [r12 + 0x539b782]           init
		{0x013402C7, 5, 9, false, 0x0000}, // mov word [r14 + r12 + 0x539b780], r13w
		{0x01340323, 5, 9, false, 0x002C}, // mov word [r14 + r12 + 0x539b7ac], ax  KeyPressBits
		{0x0134036D, 3, 7, true , 0x0000}, // lea rcx, [rip -> 0x0539B780]        CL_ModelForButton
		{0x01340383, 3, 7, true , 0x002C}, // lea rcx, [rip -> 0x0539B7AC]        KeyPressBits getter
		{0x013403D2, 3, 7, true , 0x0002}, // lea rax, [rip -> 0x0539B782]        per-controller reset
	};
	inline constexpr entcoll_site rightstick_sites[] = {
		{0x01340235, 5, 9, false, 0x0000}, // mov word ptr [r12 + rbx*2 + 0x539b760], ax
		{0x01340243, 5, 9, false, 0x0000}, // movzx ecx, word ptr [r12 + rbx*2 + 0x539b760]
		{0x01340253, 5, 9, false, 0x0002}, // mov word ptr [r12 + rbx*2 + 0x539b762], ax
		{0x01340261, 5, 9, false, 0x0000}, // movzx ecx, word ptr [r12 + rbx*2 + 0x539b760]
		{0x01340271, 5, 9, false, 0x0004}, // mov word ptr [r12 + rbx*2 + 0x539b764], ax
		{0x0134027F, 5, 9, false, 0x0000}, // movzx ecx, word ptr [r12 + rbx*2 + 0x539b760]
		{0x0134028F, 5, 9, false, 0x0006}, // mov word ptr [r12 + rbx*2 + 0x539b766], ax
		{0x013402A8, 5, 9, false, 0x0008}, // mov word ptr [r12 + rbx*2 + 0x539b768], ax
		{0x013403FB, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x405b37e]
		{0x013404F8, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x405b281]
	};
	inline constexpr entcoll_site exploder_trig_sites[] = {
		{0x001FD4B8, 3, 7, true , 0x0010}, // lea rax,[rip+..]  +0x10   CG_ExploderUpdate walk, imul lc,0xBB80
		{0x001FD776, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          CG_ExplodersInit memset (length widened below)
		{0x002008EA, 3, 7, true , 0x0018}, // lea rdi,[rip+..]  +0x18   CG_FindTrigger(lc, ...)
		{0x00205679, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          (lc*1000 + i) * 0x30
		{0x002070EA, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          (lc*1000 + i) * 0x30
		{0x00208B38, 3, 7, true , 0x0000}, // lea rcx,[rip+..]          (lc*1000 + i) * 0x30
	};
	inline constexpr entcoll_site exploder_count_sites[] = {
		{0x001FD466, 3, 7, false, 0x0000}, // lea r12, [r10 + 0x43806c8]
		{0x001FD78F, 3, 7, true , 0x0000}, // mov qword ptr [rip + 0x4182f32], rax
		{0x0020090A, 3, 7, true , 0x0000}, // lea rdi, [rip + 0x417fdb7]
		{0x00205612, 3, 7, false, 0x0000}, // lea rax, [rax + 0x43806c8]
		{0x0020706C, 4, 8, false, 0x0000}, // lea rcx, [rax*4 + 0x43806c8]
		{0x00208AAC, 4, 8, false, 0x0000}, // lea rcx, [rax*4 + 0x43806c8]
	};
	inline constexpr uint32_t exploder_trig_base = 0x046946E0;
	inline constexpr uint32_t exploder_count_base = 0x043016C8;
	inline constexpr entcoll_site screenblur_sites[] = {
		{0x0060136E, 3, 7, true , 0x0000}, // lea rax, [rip + 0x421c09b]
		{0x0060C333, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42110d6]
		{0x00641853, 3, 7, true , 0x0018}, // lea rcx, [rip + 0x41dbbce]
		{0x006467B2, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41d6c57]
		{0x00652BA0, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41ca869]
	};
	inline constexpr entcoll_site screenelec_sites[] = {
		{0x00602C3B, 4, 8, false, 0x0000}, // lea rdx, [rcx*4 + 0x481d448]
		{0x0060C363, 3, 7, true , 0x0000}, // lea r8, [rip + 0x42110de]
		{0x0061136D, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d44c], r12d
		{0x0061137F, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d44c], eax
		{0x00611394, 4, 8, false, 0x0008}, // mov dword ptr [r13 + rdi*4 + 0x481d450], eax
		{0x0061139C, 4, 8, false, 0x0000}, // mov qword ptr [r13 + rdi*4 + 0x481d448], r12
	};
	inline constexpr entcoll_site screenburn_sites[] = {
		{0x0060C393, 3, 7, true , 0x0000}, // lea r8, [rip + 0x42110c6]
		{0x006113A9, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d464], r12d
		{0x006113BB, 4, 8, false, 0x0004}, // cmp dword ptr [r13 + rdi*4 + 0x481d464], eax
		{0x006113D0, 4, 8, false, 0x0008}, // mov dword ptr [r13 + rdi*4 + 0x481d468], eax
		{0x006113D8, 4, 8, false, 0x0000}, // mov qword ptr [r13 + rdi*4 + 0x481d460], r12
		{0x0063FECB, 4, 8, false, 0x0000}, // lea rdx, [rcx*4 + 0x481d460]
	};
	inline constexpr entcoll_site compass_actors_sites[] = {
		{0x00598884, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426bcf5]   CG_ClearCompassPingData
		{0x005A1EC3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42626b6]
		{0x005A1FA3, 3, 7, true , 0x0000}, // lea rcx,[array] / imul rax,rax,0x2C00
		{0x005A3B0D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4260a6c]
		{0x005A5343, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425f236]
		{0x005A6CFF, 3, 7, true , 0x0000}, // lea rax, [rip + 0x425d87a]
		{0x005B3452, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4251127]
		{0x005B4DB4, 3, 7, true , 0x0000}, // lea r11, [rip + 0x424f7c5]
		{0x005B7FDB, 3, 7, true , 0x0000}, // lea r11, [rip + 0x424c59e]
		{0x005CC464, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4238115]
		{0x005D2B8D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42319ec]
		{0x005D2BBE, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42319bb]
		{0x005D445E, 3, 7, true , 0x0000}, // lea rax, [rip + 0x423011b]
	};
	inline constexpr entcoll_site compass_vehicles_sites[] = {
		{0x005988AC, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42735cd]
		{0x005A20A3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4269dd6]
		{0x005AB961, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4260518]
		{0x005D95D5, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x42328a4]
	};
	inline constexpr entcoll_site compass_artillery_sites[] = {
		{0x00593D7F, 3, 7, true , 0x0010}, // lea rax, [rip + 0x427950a]
		{0x00593DD2, 4, 8, false, 0x0010}, // mov dword ptr [rsi + rbx*4 + 0x480d290], r9d
		{0x00593DDE, 3, 7, false, 0x0000}, // mov dword ptr [rsi + rbx*4 + 0x480d280], eax
		{0x00593DE8, 3, 7, false, 0x0004}, // mov dword ptr [rsi + rbx*4 + 0x480d284], eax
		{0x00593DF1, 3, 7, false, 0x0008}, // mov dword ptr [rsi + rbx*4 + 0x480d288], eax
		{0x00593DFB, 3, 7, false, 0x000C}, // mov dword ptr [rsi + rbx*4 + 0x480d28c], eax
		{0x00593E18, 5, 9, false, 0x0000}, // addss xmm0, dword ptr [rsi + rbx*4 + 0x480d280]
		{0x00593E21, 5, 9, false, 0x0000}, // movss dword ptr [rsi + rbx*4 + 0x480d280], xmm0
		{0x00593E36, 5, 9, false, 0x0004}, // addss xmm0, dword ptr [rsi + rbx*4 + 0x480d284]
		{0x00593E3F, 5, 9, false, 0x0004}, // movss dword ptr [rsi + rbx*4 + 0x480d284], xmm0
		{0x005988E8, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4274991]
		{0x005A1FC3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b2b6]
	};
	inline constexpr entcoll_site compass_heli_sites[] = {
		{0x005988FC, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4274a6d]
		{0x005A2043, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b326]
		{0x005D9285, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x42340e4]
	};
	inline constexpr entcoll_site compass_0240_sites[] = {
		{0x00598910, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4274c19]
		{0x005A2023, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b506]
		{0x005D9145, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x42343e4]
	};
	inline constexpr entcoll_site compass_0120_sites[] = {
		{0x00598924, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4275085]
		{0x005A2063, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426b946]
		{0x005D8DC8, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4234be1]
	};
	inline constexpr entcoll_site compass_0500_sites[] = {
		{0x00598938, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42762b1]
		{0x005A1FE3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426cc06]
		{0x005CC5B0, 3, 7, true , 0x0004}, // lea rax, [rip + 0x424263d]
	};
	inline constexpr entcoll_site compass_0400_sites[] = {
		{0x0059894C, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x4276c9d]
		{0x005A2083, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x426d566]
		{0x005D94A5, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x4236144]
	};
	inline constexpr entcoll_site cg_weaponsarray_sites[] = {
		{0x0044D1AA, 4, 8, false, 0x0000}, // add r13, qword ptr [rax + rcx*8 + 0x49d9410]
		{0x00843ACF, 4, 8, false, 0x0000}, // mov qword ptr [rsi + r13 + 0x49d9410], rax
		{0x00853DE9, 4, 8, false, 0x0000}, // mov rdx, qword ptr [r14 + rdi*8 + 0x49d9410]
		{0x00856EE2, 3, 7, true , 0x0000}, // mov qword ptr [rip + 0x4182527], rax
		{0x00856EE9, 3, 7, true , 0x0008}, // mov qword ptr [rip + 0x4182528], rax
		{0x008F258B, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rcx + r14*8 + 0x49d9410]
		{0x0119A273, 4, 8, false, 0x0000}, // add r15, qword ptr [rdx + r14*8 + 0x49d9410]
		{0x011CA1FD, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x380f22c]
		{0x0122C649, 4, 8, false, 0x0000}, // add rdi, qword ptr [rbp + r14*8 + 0x49d9410]
		{0x0126671A, 4, 8, false, 0x0000}, // add rdi, qword ptr [r10 + rsi*8 + 0x49d9410]
		{0x026D13F4, 4, 8, false, 0x0000}, // add r13, qword ptr [rcx + rax*8 + 0x49d9410]
		{0x0271F6BE, 4, 8, false, 0x0000}, // add rbx, qword ptr [r14 + rax*8 + 0x49d9410]
	};
	inline constexpr entcoll_site cg_ikbuf_sites[] = {
		{0x00843B03, 4, 8, false, 0x0000}, // mov qword ptr [rsi + r13 + 0x4a315c0], rax
		{0x00853DC0, 4, 8, false, 0x0000}, // mov rdx, qword ptr [r14 + rdi*8 + 0x4a315c0]
	};
	inline constexpr entcoll_site cg_destructibles_sites[] = {
		{0x00843AF1, 4, 8, false, 0x0000}, // mov qword ptr [rsi + r13 + 0x17f00ff0], rax
		{0x00853DD9, 4, 8, false, 0x0000}, // mov rdx, qword ptr [r14 + rdi*8 + 0x17f00ff0]
		{0x00856F0C, 3, 7, true , 0x0000}, // mov qword ptr [rip + 0x176aa0dd], rax
		{0x00856F13, 3, 7, true , 0x0008}, // mov qword ptr [rip + 0x176aa0de], rax
		{0x022F28C0, 4, 8, false, 0x0000}, // mov rax, qword ptr [r13 + rdi*8 + 0x17f00ff0]
		{0x022F5C96, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b921e3]
		{0x022F5CB3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b921c6]
		{0x022F5CF3, 4, 8, false, 0x0000}, // mov r10, qword ptr [rdi + r11*8 + 0x17f00ff0]
		{0x022F5D7D, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
		{0x022F5D8D, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
		{0x022F5D9D, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
		{0x022F5DAA, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
		{0x022F5DB7, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
		{0x022F5DDB, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r11*8 + 0x17f00ff0]
		{0x022F5EF1, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rsi*8 + 0x17f00ff0]
		{0x022F5F10, 4, 8, false, 0x0000}, // mov rdi, qword ptr [r14 + rsi*8 + 0x17f00ff0]
		{0x022F6067, 4, 8, false, 0x0000}, // add r8, qword ptr [rdi + r12*8 + 0x17f00ff0]
		{0x022F60B4, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r12*8 + 0x17f00ff0]
		{0x022F60C8, 4, 8, false, 0x0000}, // mov r8, qword ptr [rdx + r12*8 + 0x17f00ff0]
		{0x022F60EA, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r12*8 + 0x17f00ff0]
		{0x022F6110, 4, 8, false, 0x0000}, // mov rax, qword ptr [r14 + r12*8 + 0x17f00ff0]
		{0x022F6154, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdi + r12*8 + 0x17f00ff0]
		{0x022F6161, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdi + r12*8 + 0x17f00ff0]
		{0x022FAB99, 4, 8, false, 0x0000}, // add rdx, qword ptr [r12 + r15*8 + 0x17f00ff0]
		{0x022FAC3A, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b8d23f]
		{0x022FC8DF, 4, 8, false, 0x0000}, // lea rsi, [rdi*8 + 0x17f00ff0]
		{0x022FD47C, 4, 8, false, 0x0000}, // add rdi, qword ptr [rsi + r15*8 + 0x17f00ff0]
		{0x0230064F, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15b8782a]
	};
	inline constexpr entcoll_site numdestructibles_sites[] = {
		// Three leas that point at this base are deliberately not listed: they are end markers
		// of loops over s_destructibles (0x80 x 0x108), which ends exactly where this array
		// begins. Moving them made the destructible walk run off the end (tools/sentinel_check.py).
		{0x022F5A58, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15bd1511]
		{0x022F5D62, 4, 8, false, 0x0000}, // inc dword ptr [rdi + r11*4 + 0x17f400e0]
		{0x022F5F55, 4, 12, false, 0x0000}, // mov dword ptr [r14 + rsi*4 + 0x17f400e0], 0
		{0x022F601F, 4, 8, false, 0x0000}, // mov dword ptr [rdi + r14*4 + 0x17f400e0], eax
		{0x022F91CB, 4, 8, false, 0x0000}, // cmp r13d, dword ptr [rdi + r12*4 + 0x17f400e0]
		{0x022FAB84, 4, 8, false, 0x0000}, // cmp dword ptr [r12 + r15*4 + 0x17f400e0], ebx
		{0x022FABC2, 4, 8, false, 0x0000}, // cmp ebx, dword ptr [r12 + r15*4 + 0x17f400e0]
		{0x022FC94D, 4, 8, false, 0x0000}, // cmp dword ptr [r14 + r15 + 0x17f400e0], ebx
		{0x022FC98F, 4, 8, false, 0x0000}, // cmp ebx, dword ptr [r14 + r15 + 0x17f400e0]
	};
	inline constexpr entcoll_site cg_updatetime_sites[] = {
		{0x022F5F61, 4, 12, false, 0x0000}, // mov dword ptr [r14 + rsi*4 + 0x17f400e8], 0
		{0x022FC920, 4, 8, false, 0x0000}, // mov eax, dword ptr [r14 + r15 + 0x17f400e8]
		{0x022FC92A, 4, 8, false, 0x0000}, // mov dword ptr [r14 + r15 + 0x17f400e8], eax
		{0x022FC945, 4, 8, false, 0x0000}, // mov dword ptr [r14 + r15 + 0x17f400e8], eax
	};
	inline constexpr entcoll_site destr_gamestates_sites[] = {
		{0x0230200C, 3, 7, true , 0x0002}, // lea rax, [rip + 0x15b85e7f]
		{0x02302043, 3, 7, true , 0x0000}, // lea rax, [rip + 0x15b85e46]
		{0x02302B07, 3, 7, true , 0x0000}, // lea r15, [rip + 0x15b85382]
		{0x02302BB1, 3, 7, true , 0x0002}, // lea rax, [rip + 0x15b852da]
		{0x02302BE3, 3, 7, false, 0x0000}, // lea rdx, [r11 + 0x17f01000]
		{0x02302BEA, 3, 7, false, 0x0000}, // lea r9, [r11 + 0x17f01000]
	};
	inline constexpr entcoll_site destr_numgamestates_sites[] = {
		{0x02301FF3, 3, 7, true , 0x0000}, // lea rax, [rip + 0x15b87f96]
		{0x02302ACD, 3, 7, true , 0x0000}, // lea rax, [rip + 0x15b874bc]
		{0x02302BA1, 4, 8, false, 0x0000}, // mov r9d, dword ptr [r11 + r10*4 + 0x17f03100]
		{0x02302BF1, 4, 8, false, 0x0000}, // mov dword ptr [r11 + r10*4 + 0x17f03100], eax
	};
	inline constexpr entcoll_site ikstates_sites[] = {
		{0x023F7B43, 3, 7, true , 0x0008}, // lea rdx, [ikStates+8]   IK_AllocateLocalClientMemory
		{0x023F7D0D, 3, 7, true , 0x0000}, // lea rdx, [ikStates]
		{0x023F7D3F, 3, 7, true , 0x0000}, // lea rdx, [ikStates]
		{0x023F7E15, 3, 7, true , 0x0008}, // lea rax, [ikStates+8]
		{0x023F8200, 3, 7, true , 0x0000}, // lea rsi, [ikStates]
		{0x023F84AC, 3, 7, true , 0x0000}, // lea rsi, [ikStates]      reset loop start
		{0x023F8594, 3, 7, true , 0x0000}, // lea rsi, [ikStates]
		{0x023F9260, 3, 7, true , 0x0008}, // lea rcx, [ikStates+8]
		{0x0245A539, 4, 9, false, 0x0008}, // cmp qword [rbx+rcx*8+ikStates+8], 0
	};
	inline constexpr entcoll_site cg_clientents30_sites[] = {
		{0x0019891D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c2bdc]
		{0x001989B0, 3, 7, true , 0x0080}, // lea rcx, [rip + 0x40c2bc9]
		{0x001989EB, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c2b0e]
		{0x00198A89, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c2a70]
		{0x0019A3BC, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c113d]
		{0x0019A462, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c1097]
		{0x0019A63D, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0ebc]
		{0x0019A754, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0da5]
		{0x0019A8A4, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0c55]
		{0x0019A9D6, 3, 7, true , 0x0000}, // lea rax, [rip + 0x40c0b23]
		{0x0019B831, 3, 7, true , 0x0000}, // lea rdx, [rip + 0x40bfcc8]
		{0x02CA73F8, 3, 7, true , 0x00F0}, // lea rax, [rip + 0x153abf1]
	};
	inline constexpr entcoll_site cg_perclient_3c0_sites[] = {
		{0x0060F6B5, 3, 7, true , 0x0000}, // lea rax, [rip + 0x420d5c4]
		{0x0060F844, 3, 7, true , 0x0000}, // lea rax, [rip + 0x420d435]
		{0x00641878, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41db401]
		{0x00643596, 3, 7, true , 0x0000}, // lea rax, [rip + 0x41d96e3]
		{0x0065785F, 3, 7, false, 0x0000}, // lea rcx, [rcx + 0x481cc80]
		{0x00662085, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x41babf4]
	};
	inline constexpr entcoll_site tnotify_list_sites[] = {
		{0x00A18777, 3, 7, true , 0x0044}, // lea rcx, [rip + 0x430c0e6]
		{0x00A2179C, 3, 7, true , 0x0000}, // lea rax, [rip + 0x430307d]
		{0x00A217DC, 3, 7, true , 0x0000}, // lea rax, [rip + 0x430303d]
		{0x00A2181C, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302ffd]
		{0x00A21865, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302fb4]
		{0x00A218BC, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302f5d]
		{0x00A21A0B, 3, 7, true , 0x0000}, // lea rax, [rip + 0x4302e0e]
		{0x00A21B09, 3, 7, true , 0x0000}, // lea r11, [rip + 0x4302d10]
		{0x00A21B10, 3, 7, true , 0x0048}, // lea rdx, [rip + 0x4302d51]
		{0x00A21B69, 3, 7, true , 0x0044}, // lea rsi, [rip + 0x4302cf4]
		{0x02D2C855, 3, 7, true , 0x0040}, // lea rax, [rip + 0x1f7ea04]
	};
	inline constexpr entcoll_site tnotify_head_sites[] = {
		{0x00A219F1, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286c0], rbx
		{0x00A21B22, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi*8 + 0x4d286c0], r10
		{0x00A21CC4, 4, 8, false, 0x0000}, // mov r15, qword ptr [r9 + rbx*8 + 0x4d286c0]
		{0x00A21CDB, 4, 12, false, 0x0000}, // mov qword ptr [r9 + rbx*8 + 0x4d286c0], 0
		{0x00A21FA2, 4, 8, false, 0x0000}, // mov rax, qword ptr [r12 + rdi*8 + 0x4d286c0]
		{0x00A21FBB, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286c0], rbx
	};
	inline constexpr entcoll_site tnotify_tail_sites[] = {
		{0x00A219DE, 4, 8, false, 0x0000}, // mov rax, qword ptr [r12 + rdi*8 + 0x4d286d0]
		{0x00A219F9, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286d0], rbx
		{0x00A21B31, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi*8 + 0x4d286d0], r10
		{0x00A21CE7, 4, 12, false, 0x0000}, // mov qword ptr [r9 + rbx*8 + 0x4d286d0], 0
		{0x00A21FB2, 4, 9, false, 0x0000}, // cmp qword ptr [r12 + rdi*8 + 0x4d286d0], 0
		{0x00A21FC5, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286d0], r13
	};
	inline constexpr entcoll_site tnotify_free_sites[] = {
		{0x00A21915, 4, 9, false, 0x0000}, // cmp qword ptr [r12 + rdi*8 + 0x4d286e0], 0
		{0x00A219A9, 4, 8, false, 0x0000}, // mov rbx, qword ptr [r12 + rdi*8 + 0x4d286e0]
		{0x00A219D2, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286e0], rax
		{0x00A21B5E, 4, 8, false, 0x0000}, // mov qword ptr [r15 + rsi*8 + 0x4d286e0], rax
		{0x00A21EEE, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + rax*8 + 0x4d286e0]
		{0x00A21F02, 4, 8, false, 0x0000}, // mov qword ptr [r12 + rdi*8 + 0x4d286e0], rsi
	};
	inline constexpr uint8_t tnotify_static_init[] = {
		0xB9, 0xC7, 0x00, 0x00, 0x00,                         // mov ecx, 0xC7
		0x48, 0x8D, 0x05,                                     // lea rax, [rip+..]
	};
	inline constexpr uint8_t tnotify_static_init_body[] = {
		0x89, 0x50, 0xC0,                                     // mov [rax-0x40], edx
		0xC7, 0x40, 0xC4, 0xFF, 0x03, 0x00, 0x00,             // mov dword [rax-0x3C], 0x3FF
	};
	inline constexpr entcoll_site fxgpu_client_sites[] = {
		{0x01CBD588, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd841ea1]
		{0x02E89DB4, 3, 7, true , 0x00C0}, // lea rbx, [rip + 0xc608505]
	};
	inline constexpr entcoll_site scene_pc480_sites[] = {
		{0x01C84F2F, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9201136]
		{0x01C85077, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9200fee]
		{0x01C850AE, 3, 7, true , 0x001C}, // lea r8, [rip + 0x9200fb7]
		{0x01C854D8, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9200b8d]
		{0x01C855E7, 3, 7, true , 0x0138}, // lea rdi, [rip + 0x9200b9a]
		{0x01C856A2, 3, 7, true , 0x001C}, // lea rax, [rip + 0x92009c3]
		{0x01C858A7, 3, 7, true , 0x001C}, // lea rax, [rip + 0x92007be]
		{0x01C8593E, 3, 7, true , 0x001C}, // lea rax, [rip + 0x9200727]
		{0x01C87AF0, 4, 8, false, 0x001C}, // mov ecx, dword ptr [r8 + rbx + 0xae9243c]
		{0x01C87B9A, 4, 8, false, 0x001C}, // mov ecx, dword ptr [r8 + rbx + 0xae9243c]
		{0x01C87C2E, 3, 7, true , 0x0020}, // lea rax, [rip + 0x91fe43b]
		{0x01C87C6A, 3, 7, true , 0x0034}, // lea rax, [rip + 0x91fe413]
		{0x01C87CAA, 3, 7, true , 0x0048}, // lea rax, [rip + 0x91fe3e7]
		{0x01C87CEA, 3, 7, true , 0x005C}, // lea rax, [rip + 0x91fe3bb]
		{0x01C87D80, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fe2e5]
		{0x01C8A9D8, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fb68d]
		{0x01C8AF48, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fb11d]
		{0x01C8B094, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91fafd1]
		{0x01CE1DA1, 3, 7, true , 0x037C}, // lea rax, [rip + 0x91a4624]
		{0x01D0DB6F, 3, 7, true , 0x001C}, // lea rax, [rip + 0x91784f6]
	};
	inline constexpr entcoll_site scene_c_sites[] = {
		{0x01C85A0C, 4, 8, false, 0x0000}, // mov rax, qword ptr [r9 + r10 + 0x10615a60]
		{0x01C85A4E, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r8 + rcx*8 + 0x10615a60]
		{0x01C8745D, 4, 8, false, 0x0000}, // mov qword ptr [rbx + rsi + 0x10615a60], rax
		{0x01C8752F, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rbx + rdi + 0x10615a60]
	};
	inline constexpr entcoll_site rview_a24_sites[] = {
		{0x01C9C189, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd83b9ec]
		{0x01C9C3C6, 3, 7, true , 0x0A20}, // lea rdi, [rip + 0xd83c1cf]
		{0x01C9C3CD, 3, 7, true , 0x0000}, // lea rbp, [rip + 0xd83b7a8]
		{0x01C9C440, 4, 8, true , 0x01C0}, // movss xmm0, dword ptr [rip + 0xd83b8f4]
		{0x01C9C457, 4, 8, true , 0x01C4}, // divss xmm0, dword ptr [rip + 0xd83b8e1]
		{0x01C9C464, 4, 8, true , 0x01C8}, // movss xmm0, dword ptr [rip + 0xd83b8d8]
		{0x01C9C471, 4, 8, true , 0x01CC}, // divss xmm1, dword ptr [rip + 0xd83b8cf]
		{0x01C9C47E, 4, 8, true , 0x01B0}, // movss xmm0, dword ptr [rip + 0xd83b8a6]
		{0x01C9C48B, 4, 8, true , 0x01B4}, // movss xmm1, dword ptr [rip + 0xd83b89d]
		{0x01C9C498, 4, 8, true , 0x01B8}, // movss xmm0, dword ptr [rip + 0xd83b894]
		{0x01C9C4A5, 4, 8, true , 0x01BC}, // movss xmm1, dword ptr [rip + 0xd83b88b]
		{0x01C9C4B2, 4, 8, true , 0x01D8}, // movss xmm0, dword ptr [rip + 0xd83b89a]
		{0x01C9C4BF, 4, 8, true , 0x01DC}, // movss xmm1, dword ptr [rip + 0xd83b891]
		{0x01C9C4CC, 4, 8, true , 0x01E0}, // movss xmm0, dword ptr [rip + 0xd83b888]
		{0x01C9C4D9, 4, 8, true , 0x01E8}, // movss xmm1, dword ptr [rip + 0xd83b883]
		{0x01C9C4E6, 4, 8, true , 0x01EC}, // movss xmm0, dword ptr [rip + 0xd83b87a]
		{0x01C9C4F3, 4, 8, true , 0x01F0}, // movss xmm1, dword ptr [rip + 0xd83b871]
		{0x01C9C500, 4, 8, true , 0x01F4}, // movss xmm0, dword ptr [rip + 0xd83b868]
		{0x01C9C50D, 4, 8, true , 0x01F8}, // movss xmm1, dword ptr [rip + 0xd83b85f]
		{0x01C9C51A, 4, 8, true , 0x01E4}, // movss xmm0, dword ptr [rip + 0xd83b83e]
		{0x01C9C85F, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd83b316]
		{0x01C9CA22, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd83b153]
		{0x01C9CB0F, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd83b066]
		{0x02E89C1A, 3, 7, true , 0x0000}, // lea rbx, [rip + 0xc5e0d2b]
	};
	inline constexpr entcoll_site rview_org30_sites[] = {
		{0x01C73DFD, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd8651dc]
		{0x01C747DB, 3, 7, true , 0x0000}, // lea rcx, [rip + 0xd8647fe]
		{0x01C74EDC, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd8640fd]
		{0x01C75012, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd863fc7]
		{0x01C7AB76, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85e463]
		{0x01C7AFB0, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85e029]
		{0x01C7C8CC, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85c70d]
		{0x01C7D057, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85bf82]
		{0x01C7E2E4, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85acf5]
		{0x01C7E4E5, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85aaf4]
		{0x01C7E8BA, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd85a71f]
		{0x01CB2BC5, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd826414]
		{0x01CDD3EC, 3, 7, true , 0x0000}, // lea rax, [rip + 0xd7fbbed]
	};
	inline constexpr entcoll_site aimactors_sites[] = {
		{0x0007A998, 3, 7, true , 0x0000}, // lea rax, [rip + 0x36049e1]
		{0x000897A8, 4, 8, false, 0x0000}, // mov qword ptr [rax + rcx*8 + 0x367f380], r8
	};

	inline constexpr uint32_t entword_base = 0x16D545D0;
	inline constexpr uint32_t model_pool_base = 0x16293160;
	inline constexpr uint32_t cbuf_old_records_rva = 0x1681EFB8;
	inline constexpr uint32_t joinclient_base = 0x156CB4B0;
	inline constexpr uint32_t netchan_old_base = 0x16DEAEB0;
	inline constexpr uint32_t sst_base = 0x10B21260;
	inline constexpr uint32_t session_member_base = 0x1684FAA0;
	inline constexpr uint32_t ikstates_base = 0x17F297C0;
	inline constexpr uint32_t players_kb_base_rva = 0x052739F0;          // complete_players_kb
	inline constexpr uint32_t playerkeys_end_marker_rva = 0x01347A03;    // binding-clear loop end (lea)
	// cg_zbarriers / numcgZBarriers re-implemented (roadmap item 8): the only code that
	// addresses cg_zbarriers (0x0474B8F0, [2][128] x 0x188) or numcgZBarriers (0x04764100,
	// int[2]) - every reference scanned 2026-09-30; both functions are replaced whole.
	inline constexpr uint32_t zbarrier_alloc_rva = 0x00461030;      // called only by CG_InitZBarrier 0x004613A0 (at 0x004613C8)
	inline constexpr uint8_t zbarrier_alloc_expected[] = {         // 0x00461030..0x004610CB, 156 bytes
		0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x33, 0xDB, 0x4C, 0x8D, 0x0D, 0xC1, 0x30, 0x30, 0x04, 0x4C,
		0x63, 0xC1, 0x4B, 0x63, 0x0C, 0x81, 0x8B, 0xD3, 0x4C, 0x8D, 0x15, 0xA1, 0xA8, 0x2E, 0x04, 0x85,
		0xC9, 0x7E, 0x38, 0x49, 0x8B, 0xC0, 0x48, 0x69, 0xC0, 0x00, 0xC4, 0x00, 0x00, 0x49, 0x03, 0xC2,
		0x38, 0x18, 0x74, 0x0E, 0xFF, 0xC2, 0x48, 0x05, 0x88, 0x01, 0x00, 0x00, 0x3B, 0xD1, 0x7C, 0xF0,
		0xEB, 0x19, 0x49, 0x8B, 0xD8, 0x48, 0x63, 0xC2, 0x48, 0xC1, 0xE3, 0x07, 0x48, 0x03, 0xD8, 0x48,
		0x69, 0xDB, 0x88, 0x01, 0x00, 0x00, 0x49, 0x03, 0xDA, 0x75, 0x25, 0x81, 0xF9, 0x80, 0x00, 0x00,
		0x00, 0x7D, 0x30, 0x49, 0x8B, 0xD8, 0x48, 0xC1, 0xE3, 0x07, 0x48, 0x03, 0xD9, 0x8D, 0x49, 0x01,
		0x43, 0x89, 0x0C, 0x81, 0x48, 0x69, 0xDB, 0x88, 0x01, 0x00, 0x00, 0x49, 0x03, 0xDA, 0x74, 0x13,
		0x33, 0xD2, 0x41, 0xB8, 0x88, 0x01, 0x00, 0x00, 0x48, 0x8B, 0xCB, 0xE8, 0x30, 0x3C, 0x76, 0x02,
		0xC6, 0x03, 0x01, 0x48, 0x8B, 0xC3, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3,
	};
	inline constexpr uint32_t cg_init_zbarriers_rva = 0x004616C0;   // CG_InitZBarriers; called from CG_Init 0x008F2DC9, CG_MapRestart 0x00F83685
	inline constexpr uint8_t cg_init_zbarriers_expected[] = {      // 0x004616C0..0x004616E7, 40 bytes (stock, unwidened)
		0x48, 0x83, 0xEC, 0x28, 0x48, 0x8D, 0x0D, 0x25, 0xA2, 0x2E, 0x04, 0x33, 0xD2, 0x41, 0xB8, 0x00,
		0x88, 0x01, 0x00, 0xE8, 0x18, 0x36, 0x76, 0x02, 0x48, 0xC7, 0x05, 0x1D, 0x2A, 0x30, 0x04, 0x00,
		0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3,
	};
	inline constexpr size_t zbarrier_size = 0x188;        // cgZBarrier_t on PC: add rax,0x188 (0x00461066), memset len (0x004610B2)
	inline constexpr size_t zbarriers_per_client = 128;   // cmp ecx,0x80 (0x0046108B); shl 7 (0x00461078/0x00461096); row 0xC400 (0x00461056)
	// relocate_gaglobs: GpadAxesGlob[2] and its loop end marker (lea r10, [rip + d32], 7 bytes)
	inline constexpr uint32_t gaglobs_base_rva = 0x0531C6D0;
	inline constexpr uint32_t gaglobs_stride = 0x48;
	inline constexpr uint32_t gaglobs_end_marker_rva = 0x0134014A;
	// relocate_exploder_triggers: memset length mov r8d, 0x17700 -> 0x2EE10
	inline constexpr uint32_t exploder_trig_len_rva = 0x001FD77F;
	inline constexpr uint8_t exploder_trig_len_bytes[] = {0x41, 0xB8, 0x00, 0x77, 0x01, 0x00};
	inline constexpr uint8_t exploder_trig_len_new[] = {0x41, 0xB8, 0x10, 0xEE, 0x02, 0x00};
	inline constexpr uint32_t ikstates_end_marker_rva = 0x023F84B3;      // IK reset loop end (lea r14)
	inline constexpr uint32_t tnotify_static_init_rva = 0x02D2C850;
	inline constexpr uint32_t tnotify_static_init_body_rva = 0x02D2C862;
	// ======== splitscreen/08_guest_copy.inl ========

	inline constexpr uint32_t save_read_callsite = 0x0221806E;
	inline constexpr uint32_t save_read_rva = 0x01C144B0;
	inline constexpr uint32_t dvar_get_string_rva = 0x02262A70;

	inline constexpr uint32_t save_base_dvar_rva = 0x179E63E0;
	// ======== splitscreen/09_script_bounds.inl ========

	inline constexpr uint32_t csc_lc_check_imms[] = {
		0x0028A308, 0x00293969, 0x00293A1A, 0x00293A7C, 0x002B41CB, 0x002DDF33,
		0x002E5B63, 0x002E7413, 0x002E8D03, 0x002EC2A6, 0x002EC2F3, 0x002EF4FA,
		0x002EF59C, 0x002F0E6A, 0x002F0EE3, 0x003284FA, 0x0032855A, 0x00368C1A,
		0x00368FE8, 0x003694DC, 0x0036C73C, 0x0036FB2A, 0x0036FBA1, 0x00372E1A,
		0x0037AAC4, 0x0037AB6A, 0x0039294A, 0x003929AA, 0x00392A0A, 0x00395CAF,
		0x00395D2A, 0x00395D91, 0x003AC01A, 0x003AC0DA, 0x003AC13A, 0x003B24BA,
		0x003B3E16, 0x003B3F6F, 0x003B592F, 0x003B8B8A, 0x003B8C16, 0x003B8FAF,
		0x003BA90A, 0x003BC21A, 0x003BC27A, 0x003BC308, 0x003BC463, 0x003D8293,
		0x003DE4EF, 0x003DE564, 0x003DE5AA, 0x003DE60A, 0x003DE66A, 0x003DE6CA,
		0x003DE75A, 0x003DE82F, 0x003DE8E4, 0x003DE9AF, 0x003DEA54, 0x003E984C,
		0x003ED234, 0x003EEDD9, 0x003EF00A, 0x003FF0D2, 0x00412A33, 0x004209C2,
		0x00422265, 0x00423CA5, 0x004255B4, 0x004257FA, 0x00425925, 0x00425A7D,
		0x00425B61, 0x004274EA, 0x004275D1, 0x00428FE4, 0x0042924C, 0x0042AAE4,
		0x0042AB66, 0x00435E84, 0x00A3630C, 0x00A3F43C, 0x00A8444B, 0x00A8DA73,
		0x00A9A624, 0x00AA0A94, 0x00AAD334, 0x00AB686A, 0x00AB9E71, 0x00ACCA84,
		0x00ACFBB4, 0x00B582E8, 0x00B583B9, 0x00B61869, 0x00B7A497, 0x00B7EF11,
		0x00B824E1, 0x00BC53D4, 0x00BC542C, 0x00BC6CDC, 0x00BE8296, 0x00BF095B,
		0x00C07E54, 0x00C224DC, 0x00C30536, 0x00C383D1, 0x00C4041F, 0x00C6728A,
		0x00C7BC62, 0x00C7EDE2, 0x00C82289, 0x00C8252A, 0x00C86DFE, 0x00C8D28C,
		0x00C8EB2C, 0x00C9FBB8, 0x00C9FCAC, 0x00CA5F19, 0x00CA909A, 0x00CBA12E,
		0x00CBA24D, 0x00CBA304, 0x00CBA3E4, 0x00CBA4B4, 0x00CBA58E, 0x00CBA6B4,
		0x00CBA76A, 0x00CBA84F, 0x00CBA8CF, 0x00CBA93F, 0x00CBA9E3, 0x00CBCC90,
		0x00CC1873, 0x00CC3123, 0x00CCADCB, 0x00CCB38C, 0x00CE56B4, 0x00CE5723,
		0x00CE57E4, 0x00CE5834, 0x00CE588C, 0x00CE715C, 0x00CF86AB, 0x00CFD1CB,
		0x00D254CA, 0x00D2E913, 0x00D301E1, 0x00D31AB3, 0x00D333B8, 0x00D46288,
		0x00D46318, 0x00D463A8, 0x00D46488, 0x00D46518, 0x00D4DF98, 0x00D4E07C,
		0x00D4F938, 0x00D4F9F8, 0x00D4FA78, 0x00D4FAEB, 0x00D5136D, 0x00D53289,
		0x00D57C31, 0x00D5C6A8, 0x00D5C714, 0x00D5C78B, 0x00D5E1EA, 0x00D6475B,
		0x00D647DC, 0x00D6C1CC, 0x00D6DA64, 0x00D73DBC, 0x00D97681, 0x00D9F424,
		0x00D9F484, 0x00D9F504, 0x00DF9DA4, 0x00DF9E0C, 0x00DFE8AC, 0x00E00144,
		0x00E001A4, 0x00E001F4, 0x00E0025C, 0x00E04C2F, 0x00E2183E, 0x00E2946F,
		0x00E2AEA1, 0x00E2C83A, 0x00E2E186, 0x00E2FB6E, 0x00E314CD, 0x00E32DFD,
		0x00E34711, 0x00E360AA, 0x00E36226, 0x00E363D7, 0x00E36469, 0x00E44376,
		0x00E45F85, 0x00E63981, 0x00E65289, 0x00E69CFC, 0x00E6B5DC, 0x00E6CECC,
		0x00E6E7BC, 0x00E93371, 0x00EA12F3, 0x00EE3D51, 0x00EE560A, 0x00F0175C,
		0x00F0C47E, 0x00F0DD86, 0x00F29D51, 0x00F2B7D0, 0x00F2B8D5, 0x00F41B44,
		0x00F5101B,
	};
	inline constexpr odd_lc_check odd_lc_checks[] = {
		// movsxd r9,eax / cmp r9d,edi / ja  ->  movsxd r9,eax / cmp eax,1 / ja
		{"SetFilterPassEnabled", 0x0039DB48, 8, 5,
		 {0x4C, 0x63, 0xC8, 0x44, 0x3B, 0xCF, 0x0F, 0x87},
		 {0x4C, 0x63, 0xC8, 0x83, 0xF8, 0x01, 0x0F, 0x87}, false, false},
		// mov ebx,eax / cmp eax,edi / jbe / lea rcx,msg / mov edx,eax
		//   ->  cmp eax,1 / xchg ebx,eax / jbe / lea rcx,msg / mov edx,ebx
		{"LUIDisable", 0x004259CD, 15, 2,
		 {0x8B, 0xD8, 0x3B, 0xC7, 0x76, 0x1B, 0x48, 0x8D, 0x0D, 0x56, 0xFD, 0xAF, 0x02, 0x8B, 0xD0},
		 {0x83, 0xF8, 0x01, 0x93, 0x76, 0x1B, 0x48, 0x8D, 0x0D, 0x56, 0xFD, 0xAF, 0x02, 0x8B, 0xD3},
		 true, true},
		// mov r14d,eax / cmp eax,edi  ->  cmp eax,1 / xchg r14d,eax. A rejected lc's error text
		// then shows r14's old value (no room for mov edx,r14d); only a bad script sees it.
		{"GetDStat", 0x00A187BF, 7, 2,
		 {0x44, 0x8B, 0xF0, 0x3B, 0xC7, 0x76, 0x1B},
		 {0x83, 0xF8, 0x01, 0x41, 0x96, 0x76, 0x1B}, true, false},
		// cmp eax,2 / jl  ->  cmp eax,1 / jbe (unsigned: a negative lc is rejected now)
		{"GetPerks", 0x00A22072, 5, 2,
		 {0x83, 0xF8, 0x02, 0x7C, 0x1B}, {0x83, 0xF8, 0x01, 0x76, 0x1B}, false, false},
		{"HasPerk", 0x00A22137, 5, 2,
		 {0x83, 0xF8, 0x02, 0x7C, 0x1B}, {0x83, 0xF8, 0x01, 0x76, 0x1B}, false, false},
		// mov r9d,eax / mov [rbp+48h],eax / cmp eax,ebx / jbe
		//   ->  mov [rbp+48h],eax / cmp eax,1 / nop / jbe to the stock `mov r9d,[rbp+48h]`
		{"IsInHelicopter", 0x00D8B0B9, 10, 5,
		 {0x44, 0x8B, 0xC8, 0x89, 0x45, 0x48, 0x3B, 0xC3, 0x76, 0x1F},
		 {0x89, 0x45, 0x48, 0x83, 0xF8, 0x01, 0x66, 0x90, 0x76, 0x1B}, false, false},
	};

	// install_lc_bound_hooks: AllocatePerLocalClientMemory `mov [rsp+8],rbx; mov [rsp+10h],rbp`,
	// CL_FreePerLocalClientMemory `push rbx; sub rsp,20h; call`
	inline constexpr uint32_t alloc_per_lc_rva = 0x0135D330;
	inline constexpr uint8_t alloc_per_lc_head[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10};
	inline constexpr uint32_t free_per_lc_rva = 0x0135DC20;
	inline constexpr uint8_t free_per_lc_head[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0xE8};
	// ======== splitscreen/10_relocations_b.inl ========

	inline constexpr entcoll_site hudpl_score_sites[] = {
		{0x026A4375, 4, 8, false, 0x0000}, // cmp dword ptr [r14 + rsi + 0x1a8759d0], eax
		{0x026A4388, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a8759d0], eax
		{0x026A74F2, 3, 7, false, 0x0000}, // mov dword ptr [rcx + rsi + 0x1a8759d0], eax
		{0x026C6E0B, 3, 7, false, 0x0000}, // lea rcx, [r10 + 0x1a8759d0]
		{0x026CC141, 4, 8, false, 0x0000}, // mov edx, dword ptr [r14 + rax + 0x1a8759d0]
	};
	inline constexpr entcoll_site hudpl_gap_sites[] = {
		{0x026A4396, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a875a10], eax
	};
	inline constexpr entcoll_site hudpl_flags_sites[] = {
		{0x026A4349, 4, 8, false, 0x0000}, // cmp dword ptr [r14 + rsi + 0x1a875a50], eax
		{0x026A4362, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a875a50], eax
		{0x026A74D7, 3, 8, false, 0x0000}, // cmp dword ptr [rcx + rsi + 0x1a875a50], 1
		{0x026A74F9, 3, 11, false, 0x0000}, // mov dword ptr [rcx + rsi + 0x1a875a50], 0
		{0x026C6DDC, 4, 8, false, 0x0000}, // mov qword ptr [rax + r10 + 0x1a875a50], r9
		{0x026C6DE4, 4, 8, false, 0x0008}, // mov qword ptr [rax + r10 + 0x1a875a58], r9
		{0x026C6DEC, 4, 8, false, 0x0010}, // mov qword ptr [rax + r10 + 0x1a875a60], r9
		{0x026C6DFB, 4, 8, false, 0x0018}, // mov qword ptr [rax + r10 + 0x1a875a68], r9
		{0x026CC0D5, 4, 9, false, 0x0000}, // cmp dword ptr [r14 + rdx + 0x1a875a50], 0
	};
	inline constexpr entcoll_site hudpl_ids_sites[] = {
		{0x026A7434, 4, 8, false, 0x0000}, // cmp ebx, dword ptr [r14 + rax + 0x1a875a90]
		{0x026A74A6, 4, 8, false, 0x0000}, // mov dword ptr [r14 + rsi + 0x1a875a90], ebx
		{0x026C6D58, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x18135bc1]
		{0x026C6DD2, 3, 7, false, 0x0000}, // lea rdx, [r10 + 0x1a875a90]
	};
	inline constexpr entcoll_site hudpl_icons_sites[] = {
		{0x026A43A2, 3, 8, false, 0x0000}, // cmp qword ptr [rsi + 0x1a875ad0], 0
		{0x026A43AA, 3, 7, false, 0x0000}, // lea rsi, [rsi + 0x1a875ad0]
		{0x026A74E1, 4, 8, false, 0x0000}, // mov qword ptr [rsi + rax*8 + 0x1a875ad0], rbx
		{0x026C6DC0, 3, 7, true , 0x0000}, // lea rdx, [rip + 0x18135b99]
		{0x026CC0A9, 3, 7, true , 0x0000}, // lea rax, [rip + 0x181308b0]
		{0x026C6D43, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x18135c16]  (reset leaf; the generator misses it)
	};
	inline constexpr entcoll_site hudpl_self_sites[] = {
		{0x026A42DB, 4, 8, false, 0x0000}, // mov dword ptr [rsi + r11*4 + 0x1a875b50], eax
		{0x026CC08D, 3, 7, false, 0x0000}, // lea rax, [rdx + 0x1a875b50]
	};
	inline constexpr entcoll_site prevview_sites[] = {
		{0x01CDF3D5, 3, 7, true , 0x0000}, // lea rax, [rip + 0xe15ffd4]
	};
	// Batch 19: the per-client statics of cg_draw_names.cpp (PS4 [4], PC [2], packed from
	// 0x04945090 to drawNameEntities 0x049482C0). Generated and cross-checked against every raw
	// candidate field (every Arxan-section candidate is data or junk).
	// playerdetails (PS4 playerDetails PlayerDetails[4][18] x 0x68): 3 sites
	inline constexpr entcoll_site playerdetails_sites[] = {
		{0x00677B94, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42cd4f5]
		{0x0069D795, 3, 7, true , 0x0000}, // lea rax, [rip + 0x42a78f4]
		{0x006A7AEB, 3, 7, true , 0x0000}, // lea rax, [rip + 0x429d59e]
	};
	// actoroverheadfade (PS4 actorOverheadFade OverheadFade[4][64]): 24 sites
	inline constexpr entcoll_site actoroverheadfade_sites[] = {
		{0x00677B38, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42ce3f1]
		{0x00679461, 4, 9, false, 0x000C}, // mov byte ptr [r13 + rax*8 + 0x4945f3c], 0
		{0x00679479, 5, 9, false, 0x000C}, // movzx ecx, byte ptr [r13 + rbx*8 + 0x4945f3c]
		{0x00679486, 4, 8, false, 0x0000}, // mov eax, dword ptr [r13 + rbx*8 + 0x4945f30]
		{0x0067949B, 4, 9, false, 0x000C}, // mov byte ptr [r13 + rbx*8 + 0x4945f3c], 0
		{0x006794BB, 4, 8, false, 0x0000}, // mov dword ptr [r13 + rbx*8 + 0x4945f30], eax
		{0x006794C3, 4, 9, false, 0x000C}, // cmp byte ptr [r13 + rbx*8 + 0x4945f3c], 0
		{0x006794CE, 4, 8, false, 0x0004}, // mov dword ptr [r13 + rbx*8 + 0x4945f34], eax
		{0x0067E054, 4, 9, false, 0x000C}, // mov byte ptr [rdx + r8*8 + 0x4945f3c], 0
		{0x00692A20, 4, 9, false, 0x000C}, // mov byte ptr [r8 + rbx*8 + 0x4945f3c], 0
		{0x00692A51, 4, 9, false, 0x000C}, // cmp byte ptr [r8 + rbx*8 + 0x4945f3c], 0
		{0x00692A60, 4, 9, false, 0x000C}, // mov byte ptr [r8 + rbx*8 + 0x4945f3c], 1
		{0x00692A69, 4, 8, false, 0x0008}, // mov dword ptr [r8 + rbx*8 + 0x4945f38], esi
		{0x00699CAA, 4, 9, false, 0x000C}, // cmp byte ptr [r8 + rax*8 + 0x4945f3c], 0
		{0x00699CBD, 4, 8, false, 0x0004}, // mov dword ptr [r8 + rax*8 + 0x4945f34], ecx
		{0x00699CCD, 4, 8, false, 0x0004}, // mov r8d, dword ptr [r8 + rax*8 + 0x4945f34]
		{0x00699CE4, 4, 8, false, 0x0008}, // mov edx, dword ptr [r10 + rax*8 + 0x4945f38]
		{0x006A79A5, 4, 8, false, 0x0008}, // mov eax, dword ptr [r15 + rcx*8 + 0x4945f38]
		{0x006A79B0, 4, 8, false, 0x0004}, // mov eax, dword ptr [r15 + rcx*8 + 0x4945f34]
		{0x006A7A53, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x429e4d6]
		{0x006B0524, 4, 8, false, 0x0004}, // sub eax, dword ptr [rcx + r8 + 0x4945f34]
		{0x006B054D, 4, 8, false, 0x0008}, // mov dword ptr [rcx + r8 + 0x4945f38], eax
		{0x006B055B, 4, 8, false, 0x0004}, // mov dword ptr [rcx + r8 + 0x4945f34], eax
		{0x006EF430, 4, 8, false, 0x000C}, // cmp byte ptr [rax + rcx*8 + 0x4945f3c], r14b
	};
	// centoverheadfade (PS4 centOverheadFade centity_overheadName_t[4][32] (PC 0x50 each)): 2 sites
	inline constexpr entcoll_site centoverheadfade_sites[] = {
		{0x00677B4C, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42cebdd]
		{0x006A7A73, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x429ecb6]
	};
	// overheadfade (PS4 overheadFade OverheadFade[4][18]): 32 sites
	inline constexpr entcoll_site overheadfade_sites[] = {
		{0x00677B24, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42d0035]
		{0x00682AEC, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42c506d]
		{0x00682B48, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42c5011]
		{0x00682B7E, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42c4fdb]
		{0x00682DF6, 4, 9, false, 0x000C}, // mov byte ptr [rdx + r8*8 + 0x4947b6c], 0
		{0x0068822C, 4, 9, false, 0x000C}, // mov byte ptr [r8 + rdx*8 + 0x4947b6c], 0
		{0x00688279, 4, 9, false, 0x000C}, // cmp byte ptr [r8 + rdx*8 + 0x4947b6c], 0
		{0x0068829E, 4, 9, false, 0x000C}, // mov byte ptr [r8 + rdx*8 + 0x4947b6c], 1
		{0x006882A7, 4, 8, false, 0x0008}, // mov dword ptr [r8 + rdx*8 + 0x4947b68], eax
		{0x0068F145, 4, 9, false, 0x000C}, // cmp byte ptr [rbx + r8 + 0x4947b6c], 0
		{0x0068F154, 4, 9, false, 0x000C}, // mov byte ptr [rbx + r8 + 0x4947b6c], 1
		{0x0068F15D, 4, 8, false, 0x0008}, // mov dword ptr [rbx + r8 + 0x4947b68], esi
		{0x0068F16E, 4, 8, false, 0x000D}, // mov byte ptr [rbx + r8 + 0x4947b6d], al
		{0x0068F186, 4, 9, false, 0x000C}, // mov byte ptr [rbx + r8 + 0x4947b6c], 0
		{0x00696323, 4, 9, false, 0x000D}, // cmp byte ptr [r8 + rdx*8 + 0x4947b6d], 0
		{0x00696332, 4, 9, false, 0x000C}, // cmp byte ptr [r8 + rdx*8 + 0x4947b6c], 0
		{0x00696345, 4, 8, false, 0x0004}, // mov dword ptr [r8 + rdx*8 + 0x4947b64], eax
		{0x00696351, 4, 8, false, 0x0004}, // mov r8d, dword ptr [r8 + rdx*8 + 0x4947b64]
		{0x0069636A, 4, 8, false, 0x0008}, // mov edx, dword ptr [r10 + rdx*8 + 0x4947b68]
		{0x0069639C, 4, 9, false, 0x000D}, // cmp byte ptr [r8 + rdx*8 + 0x4947b6d], 0
		{0x006963AB, 4, 9, false, 0x000C}, // cmp byte ptr [r8 + rdx*8 + 0x4947b6c], 0
		{0x006963BE, 4, 8, false, 0x0004}, // mov dword ptr [r8 + rdx*8 + 0x4947b64], eax
		{0x006963CA, 4, 8, false, 0x0004}, // mov r8d, dword ptr [r8 + rdx*8 + 0x4947b64]
		{0x006963E3, 4, 8, false, 0x0008}, // mov edx, dword ptr [r10 + rdx*8 + 0x4947b68]
		{0x006A47A8, 4, 8, false, 0x000C}, // cmp byte ptr [rdx + rcx + 0x4947b6c], r15b
		{0x006A47B2, 3, 8, false, 0x000C}, // mov byte ptr [rdx + rcx + 0x4947b6c], 1
		{0x006A47BA, 4, 8, false, 0x0008}, // mov dword ptr [rdx + rcx + 0x4947b68], r14d
		{0x006A47C2, 4, 8, false, 0x0004}, // mov dword ptr [rdx + rcx + 0x4947b64], r14d
		{0x006A47E8, 3, 8, false, 0x000C}, // mov byte ptr [rdx + rcx + 0x4947b6c], 0
		{0x006A47F0, 4, 8, false, 0x0004}, // mov r8d, dword ptr [rdx + rcx + 0x4947b64]
		{0x006A47FB, 3, 7, false, 0x0008}, // mov edx, dword ptr [rdx + rcx + 0x4947b68]
		{0x006A7AB3, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42a00a6]
	};
	// friendlyheadtrace (PS4 s_friendlyHeadTrace FriendlyHeadTrace[4][18]): 8 sites
	inline constexpr entcoll_site friendlyheadtrace_sites[] = {
		{0x00677B60, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42d0239]
		{0x00682DDE, 3, 7, false, 0x0004}, // mov byte ptr [rcx + rdx + 0x4947da4], al
		{0x00682DE5, 3, 8, false, 0x0005}, // mov byte ptr [rcx + rdx + 0x4947da5], 1
		{0x00682E03, 3, 7, false, 0x0000}, // mov eax, dword ptr [rcx + rdx + 0x4947da0]
		{0x00687F6F, 3, 7, false, 0x0000}, // mov dword ptr [rdi + rdx + 0x4947da0], esi
		{0x0068B8FD, 4, 8, false, 0x0005}, // mov byte ptr [r8 + rdx*8 + 0x4947da5], dil
		{0x0068F132, 4, 9, false, 0x0004}, // cmp byte ptr [rdi + r8 + 0x4947da4], 0
		{0x0068F165, 5, 9, false, 0x0005}, // movzx eax, byte ptr [rdi + r8 + 0x4947da5]
	};
	// friendlyactorheadtrace (PS4 s_friendlyActorHeadTrace FriendlyHeadTrace[4][64]): 5 sites
	inline constexpr entcoll_site friendlyactorheadtrace_sites[] = {
		{0x00677B74, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x42d0345]
		{0x0067E044, 3, 7, false, 0x0004}, // mov byte ptr [rcx + rdx + 0x4947ec4], al
		{0x0067E061, 3, 7, false, 0x0000}, // mov eax, dword ptr [rcx + rdx + 0x4947ec0]
		{0x006929D2, 4, 8, false, 0x0000}, // mov dword ptr [r8 + rbx*8 + 0x4947ec0], esi
		{0x00692A0E, 4, 9, false, 0x0004}, // cmp byte ptr [r8 + rbx*8 + 0x4947ec4], 0
	};
	inline constexpr entcoll_site visbits_sites[] = {
		{0x0061D911, 3, 7, false, 0x0000}, // mov ebx, dword ptr [rbx + rsi*8 + 0x179dbdc8]
		{0x006D3CAD, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r9 + rbx*8 + 0x179dbdc8]
		{0x008635A1, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rsi*8 + 0x179dbdc8]
		{0x00A141F4, 4, 8, false, 0x0000}, // mov eax, dword ptr [r13 + r15*8 + 0x179dbdc8]
		{0x00E477F9, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r15 + rsi*8 + 0x179dbdc8]
		{0x00FB5B24, 3, 7, false, 0x0000}, // mov ecx, dword ptr [rdx + rsi*8 + 0x179dbdc8]
		{0x0135FAF1, 4, 8, false, 0x0000}, // mov rax, qword ptr [r14 + rdi*8 + 0x179dbdc8]
		{0x013D2030, 4, 8, false, 0x0000}, // mov rax, qword ptr [r14 + rdx + 0x179dbdc8]
		{0x013D2047, 4, 12, false, 0x0000}, // mov qword ptr [r14 + rdx + 0x179dbdc8], 0
		{0x013D38B2, 4, 8, false, 0x0000}, // or qword ptr [r14 + rdx + 0x179dbdc8], rax
		{0x013D38CB, 4, 8, false, 0x0000}, // or rcx, qword ptr [r14 + rdx + 0x179dbdc8]
		{0x013D38D3, 4, 8, false, 0x0000}, // mov qword ptr [r14 + rdx + 0x179dbdc8], rcx
		{0x013D38FA, 4, 8, false, 0x0000}, // mov qword ptr [rax + rdx + 0x179dbdc8], rcx
		{0x013D3919, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x40000000
		{0x013D3946, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
		{0x013D3972, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
		{0x013D3991, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x40000000
		{0x013D39A6, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x800000
		{0x013D39CD, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x1000000
		{0x013D3A12, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x2000000
		{0x013D3A54, 4, 8, false, 0x0000}, // or qword ptr [rcx + rdx + 0x179dbdc8], rax
		{0x013D3A81, 4, 8, false, 0x0000}, // or qword ptr [rcx + rdx + 0x179dbdc8], rax
		{0x013D3AAA, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
		{0x013D3ADA, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
		{0x013D3B03, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
		{0x013D3B22, 4, 12, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], 0x4000000
		{0x013D3B3B, 4, 12, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], 0x8000000
		{0x013D3B5E, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
		{0x013D3B79, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
		{0x013D3B94, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
		{0x013D3BBD, 4, 8, false, 0x0000}, // or qword ptr [rax + rdx + 0x179dbdc8], rcx
		{0x013D3BF1, 4, 12, false, 0x0000}, // or qword ptr [rax + rcx + 0x179dbdc8], 0x10000000
		{0x013D6D42, 4, 12, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], 0x20000000
		{0x013D6DDF, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D6E04, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D6E26, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D6E44, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D6E7E, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D6EDC, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D6EFD, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D6F78, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D6F80, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r15 + r13 + 0x179dbdc8]
		{0x013D6FE1, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r15 + r13 + 0x179dbdc8]
		{0x013D7033, 4, 8, false, 0x0000}, // mov qword ptr [r15 + r13 + 0x179dbdc8], rcx
		{0x013D7055, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D709D, 4, 8, false, 0x0000}, // and qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D70BF, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D70DD, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D7103, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D711E, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D7139, 4, 8, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D714A, 4, 12, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], 0x200000
		{0x013D715F, 4, 12, false, 0x0000}, // or qword ptr [r15 + r13 + 0x179dbdc8], 0x400000
		{0x013D7173, 4, 8, false, 0x0000}, // mov rax, qword ptr [r15 + r13 + 0x179dbdc8]
		{0x013D71D5, 4, 8, false, 0x0000}, // mov qword ptr [r15 + r13 + 0x179dbdc8], rax
		{0x013D72CC, 4, 8, false, 0x0000}, // mov r8, qword ptr [r15 + r13 + 0x179dbdc8]
		{0x01F23CB7, 4, 8, false, 0x0000}, // mov esi, dword ptr [rax + r12*8 + 0x179dbdc8]
		{0x01F24013, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rbx + r12*8 + 0x179dbdc8]
		{0x01F26735, 3, 7, true , 0x0000}, // lea rsi, [rip + 0x15aa8f0c]
		{0x01FDAA77, 3, 7, true , 0x0000}, // lea r8, [rip + 0x159f4bca]
		{0x01FF7347, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rdx*8 + 0x179dbdc8]
		{0x01FF7537, 4, 8, false, 0x0000}, // mov r9, qword ptr [r14 + rdi*8 + 0x179dbdc8]
		{0x01FF770F, 4, 8, false, 0x0000}, // mov r9, qword ptr [rdx + rdi*8 + 0x179dbdc8]
		{0x01FF785E, 4, 8, false, 0x0000}, // mov r9, qword ptr [rcx + rdi*8 + 0x179dbdc8]
		{0x01FF79B7, 4, 8, false, 0x0000}, // mov r9, qword ptr [r14 + rdi*8 + 0x179dbdc8]
		{0x0200C283, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rbx*8 + 0x179dbdc8]
		{0x0201127F, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x159be3c2]
		{0x0201153E, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x159be103]
		{0x0201C4B7, 5, 9, false, 0x0000}, // movzx eax, byte ptr [r13 + rdi*8 + 0x179dbdc8]
		{0x02035F70, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r11 + rsi*8 + 0x179dbdc8]
		{0x02037FE1, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r14*8 + 0x179dbdc8]
		{0x02038163, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r14*8 + 0x179dbdc8]
		{0x0203838E, 4, 8, false, 0x0000}, // mov rax, qword ptr [rcx + r14*8 + 0x179dbdc8]
		{0x0203A820, 4, 8, false, 0x0000}, // mov rax, qword ptr [rdx + r12*8 + 0x179dbdc8]
		{0x020462F0, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rcx + r14*8 + 0x179dbdc8]
		{0x02054DD8, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r15*8 + 0x179dbdc8]
		{0x0205BF00, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r8 + r13*8 + 0x179dbdc8]
		{0x020668E6, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + rsi*8 + 0x179dbdc8]
		{0x0206F333, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rdi*8 + 0x179dbdc8]
		{0x0206F711, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rdi*8 + 0x179dbdc8]
		{0x02074E6E, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r8 + rbx*8 + 0x179dbdc8]
		{0x0207824F, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r14*8 + 0x179dbdc8]
		{0x02079CBC, 4, 8, false, 0x0000}, // mov r9, qword ptr [r12 + r14*8 + 0x179dbdc8]
		{0x0207B8ED, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r12*8 + 0x179dbdc8]
		{0x02080A84, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r14*8 + 0x179dbdc8]
		{0x02084436, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + rsi*8 + 0x179dbdc8]
		{0x02087672, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r10 + rdi*8 + 0x179dbdc8]
		{0x020877EE, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rbx + rdi*8 + 0x179dbdc8]
		{0x02092456, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rax + rbx*8 + 0x179dbdc8]
		{0x02098F85, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r12 + rsi*8 + 0x179dbdc8]
		{0x0209920F, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rdx + r13*8 + 0x179dbdc8]
		{0x020A0DDF, 3, 7, true , 0x0000}, // lea r9, [rip + 0x1592e862]
		{0x020A0ED7, 3, 7, true , 0x0000}, // lea r9, [rip + 0x1592e76a]
		{0x025B1223, 4, 8, true , 0x0000}, // movdqu xmm0, xmmword ptr [rip + 0x153b1a2d]
		{0x025B126B, 4, 8, true , 0x0000}, // movdqu xmmword ptr [rip + 0x153b19e5], xmm0
		{0x025D17BE, 3, 7, true , 0x0000}, // lea rcx, [rip + 0x15391493]
		{0x025D40C8, 4, 8, false, 0x0000}, // mov rcx, qword ptr [r14 + rbx*8 + 0x179dbdc8]
		{0x026B72CE, 4, 8, false, 0x0000}, // mov rcx, qword ptr [rsi + rdi*8 + 0x179dbdc8]
		{0x026EBC6A, 4, 8, false, 0x0000}, // movzx ecx, byte ptr [rdx + rsi*8 + 0x179dbdc8]
		{0x026EF0C3, 4, 8, false, 0x0000}, // mov eax, dword ptr [rax + r13*8 + 0x179dbdc8]
		{0x026EF10A, 4, 8, false, 0x0000}, // mov eax, dword ptr [rax + r13*8 + 0x179dbdc8]
	};
	inline constexpr uint32_t conmsgbuf_base = 0x052F87F8;
	inline constexpr uint32_t conmsgbuf_stride = 0x2BC0;
	inline constexpr entcoll_site conmsgbuf_sites[] = {
		{0x0133930F, 4, 8, false, 0x2030}, // mov qword ptr [rax + rdi + 0x5379828], rcx
		{0x01339317, 4, 8, false, 0x2038}, // mov qword ptr [rax + rdi + 0x5379830], rcx
		{0x0133931F, 4, 8, false, 0x2070}, // mov qword ptr [rax + rdi + 0x5379868], rcx
		{0x01339327, 4, 8, false, 0x2078}, // mov qword ptr [rax + rdi + 0x5379870], rcx
		{0x0133932F, 4, 8, false, 0x20B0}, // mov qword ptr [rax + rdi + 0x53798a8], rcx
		{0x01339337, 4, 8, false, 0x20B8}, // mov qword ptr [rax + rdi + 0x53798b0], rcx
		{0x0133933F, 4, 8, false, 0x20F0}, // mov qword ptr [rax + rdi + 0x53798e8], rcx
		{0x01339347, 4, 8, false, 0x20F8}, // mov qword ptr [rax + rdi + 0x53798f0], rcx
		{0x0133934F, 4, 8, false, 0x2B30}, // mov qword ptr [rbx + rdi + 0x537a328], rcx
		{0x01339357, 4, 8, false, 0x2B38}, // mov qword ptr [rbx + rdi + 0x537a330], rcx
		{0x0133A666, 3, 7, true , 0x201C}, // lea rcx, [rip + 0x403f1c7]
		{0x0133A7E0, 3, 7, true , 0x2000}, // lea rcx, [rip + 0x403f031]  Con_GetGameMsgWindow
		{0x0133AA39, 3, 7, true , 0x2B10}, // lea rdi, [rip + 0x403f8e8]  Con_InitMessageBuffer
		{0x0133D8E4, 3, 7, true , 0x2000}, // lea r13, [rip + 0x403bf2d]
		{0x0133DA8E, 3, 7, true , 0x2000}, // lea rax, [rip + 0x403bd83]
	};
	inline constexpr con_rel_site conmsgbuf_con_rel[] = {
		{0x0133D107, 3, 0x13094}, // lea rcx, [rsi + 0x13094]            rsi = con (0x0133D0D8)
		{0x0133D172, 4, 0x13BB0}, // cmp dword ptr [rdi + rsi + 0x13bb0], r11d
		{0x0133D180, 3, 0x13BAC}, // mov eax, dword ptr [rdi + rsi + 0x13bac]
		{0x0133D18E, 3, 0x13B94}, // idiv dword ptr [rdi + rsi + 0x13b94]
		{0x0133D19C, 4, 0x13B78}, // mov rax, qword ptr [rdi + rsi + 0x13b78]
		{0x0133D1A8, 4, 0x13B80}, // mov rax, qword ptr [rdi + rsi + 0x13b80]
		{0x0133D1C3, 4, 0x13BB0}, // cmp r11d, dword ptr [rdi + rsi + 0x13bb0]
		{0x0133D22C, 3, 0x13078}, // lea rbx, [r15 + 0x13078]            r15 = con (0x0133D1FE)
		{0x0133D256, 3, 0x13B78}, // lea rcx, [r15 + 0x13b78]
		{0x0133D930, 4, 0x13B78}, // lea rdx, [r12 + 0x13b78]            r12 = con (0x0133D8CB)
		{0x0133D987, 4, 0x13B78}, // lea rdx, [r12 + 0x13b78]
		{0x0133DABC, 3, 0x13B78}, // add r8, 0x13b78                     r8 = con (0x0133DA95)
		{0x0133DB8B, 4, 0x13BB4}, // mov eax, dword ptr [rcx + r8 + 0x13bb4]   r8 = con (0x0133DB56)
		{0x0133DB93, 4, 0x13B80}, // mov rdi, qword ptr [rcx + r8 + 0x13b80]
		{0x0133DB9E, 4, 0x13B94}, // idiv dword ptr [rcx + r8 + 0x13b94]
		{0x0133DBA6, 4, 0x13BB4}, // mov dword ptr [rcx + r8 + 0x13bb4], edx
		// Con_ClearNotify (PS4 0x3F1FE0), a leaf without .pdata: clears the four game-message
		// windows, rcx = con. Missing, it cleared parts of the print queue for lc 2/3.
		{0x01339223, 4, 0x130A8}, // mov qword ptr [rax + rcx + 0x130a8], rdx
		{0x0133922B, 4, 0x130B0},
		{0x01339233, 4, 0x130E8},
		{0x0133923B, 4, 0x130F0},
		{0x01339243, 4, 0x13128},
		{0x0133924B, 4, 0x13130},
		{0x01339253, 4, 0x13168},
		{0x0133925B, 4, 0x13170},
	};
	inline constexpr uint32_t conmsgbuf_end_marker_rva = 0x0133AB7E; // lea rcx, [&messageBuffer[2]+0x2B10]
	inline constexpr entcoll_site uiinfo_sites[] = {
		{0x022304A1, 3, 7, true , 0x0000},
		{0x0223085C, 3, 7, true , 0x184C},
		{0x0223088C, 3, 7, true , 0x002C},
		{0x02230BC9, 3, 7, true , 0x0000}, // UI_UIContext_GetInfo
		{0x02231087, 3, 7, true , 0x001C}, // UI_InitUIInfos
		{0x022312C9, 3, 7, true , 0x184C},
		{0x0223157F, 3, 7, true , 0x0000},
		{0x0223268B, 3, 7, true , 0x0030},
		{0x022328F5, 3, 7, true , 0x0000},
		{0x022329A0, 3, 7, true , 0x0000},
	};
	inline constexpr uint32_t uiinfo_init_bound_rva = 0x02231110;
	inline constexpr uint8_t uiinfo_init_bound_old[] = {0x83, 0xFD, 0x02};   // cmp ebp, 2
	inline constexpr entcoll_site ui3d_windows_sites[] = {
		{0x01D0FD15, 3, 7, true , 0x0000}, // lea rbx, [saved]         init, slot 0
		{0x01D0FDB0, 3, 7, true , 0x0438}, // lea rcx, [saved + 0x438] init, slot 1
		{0x01D0FF2F, 3, 7, true , 0x0000}, // lea rdx, [saved]         R_UI3D_PerframeInit
		{0x01D10373, 3, 7, true , 0x0000}, // lea rcx, [saved]         R_UI3D_SetupBackendData
	};
	inline constexpr entcoll_site lightq_sites[] = {
		{0x000B15FE, 3, 7, true , 0x0000}, // restore: memset
		{0x000B1665, 3, 7, true , 0x0000}, // restore
		{0x000B1E7B, 3, 7, false, 0x0000}, // save
		{0x000B1EDE, 4, 9, false, 0x0020},
		{0x000B1EF4, 4, 8, false, 0x0000},
		{0x000B1F73, 4, 8, false, 0x0008},
		{0x000B1F8B, 4, 8, false, 0x0008},
		{0x000B1F93, 4, 8, false, 0x0010},
		{0x000B2008, 4, 8, false, 0x001C},
		{0x00436EF7, 4, 8, false, 0x0008}, // consumer
		{0x00436EFF, 4, 8, false, 0x0020},
		{0x00436F07, 4, 8, false, 0x0010},
		{0x00436F0F, 4, 8, false, 0x0000},
		{0x00436F1C, 4, 8, false, 0x0018},
		{0x01CEDEFE, 4, 8, false, 0x0010}, // producer
		{0x01CEDF09, 4, 8, false, 0x0008},
		{0x01CEDF1F, 5, 9, false, 0x0020},
		{0x01CEDF2E, 4, 8, false, 0x001C},
		{0x01CEDF3A, 4, 8, false, 0x0018},
		{0x01CEDF52, 5, 9, false, 0x0000},
		{0x01CEDF5B, 5, 10, false, 0x0020},
		{0x01CEDF6B, 4, 8, false, 0x0000},
		{0x01CEE6EE, 3, 7, false, 0x0000},
	};
	inline constexpr entcoll_site lightq_a_sites[] = {
		{0x000B174B, 4, 8, false, 0},
		{0x000B1DB0, 4, 8, false, 0},
		{0x00436E47, 4, 8, false, 0},
		{0x0043A80D, 4, 8, false, 0},
		{0x01CEDECE, 4, 8, false, 0},
		{0x01CEE158, 3, 7, true , 0}, // reset
		{0x01CEE6C5, 4, 8, false, 0},
	};
	inline constexpr entcoll_site lightq_b_sites[] = {
		{0x000B1753, 4, 8, false, 0},
		{0x000B1DB8, 3, 7, false, 0},
		{0x00436E3C, 4, 8, false, 0},
		{0x0043A7DE, 4, 8, false, 0},
		{0x0043A805, 4, 8, false, 0},
		{0x01CEDEE5, 4, 8, false, 0},
		{0x01CEE15F, 3, 7, true , 0}, // reset
		{0x01CEE6DA, 3, 7, false, 0},
	};
	inline constexpr uint32_t umbra_params_new = 0x470220;
	inline constexpr uint32_t umbra_trig_new = 0x470270;
	inline constexpr uint32_t umbra_ptrig_new = 0x470280;
	inline constexpr uint32_t umbra_params_delta = umbra_params_new - 0x12DC0C;
	inline constexpr umbra_disp umbra_disps[] = {
		// params (field offsets 0x00..0x10 of each 0x14 entry)
		{0x01C8D417, 5, 0x12DC0C, 0x12DC0C + umbra_params_delta},
		{0x01C8D420, 5, 0x12DC10, 0x12DC10 + umbra_params_delta},
		{0x01C8D42F, 5, 0x12DC14, 0x12DC14 + umbra_params_delta},
		{0x01C8D43E, 5, 0x12DC18, 0x12DC18 + umbra_params_delta},
		{0x01C8D44D, 5, 0x12DC1C, 0x12DC1C + umbra_params_delta},
		{0x01C8DDDE, 5, 0x12DC10, 0x12DC10 + umbra_params_delta},   // SetAccurateOcclusionThreshold
		{0x01C8E045, 5, 0x12DC14, 0x12DC14 + umbra_params_delta},   // SetMinimumContributionThreshold
		{0x01C8EC55, 4, 0x12DC10, 0x12DC10 + umbra_params_delta},
		{0x01C8EFF6, 3, 0x12DC0C, umbra_params_new},                // defaults init
		{0x01C8F084, 3, 0x12DC0C, umbra_params_new},                // UmbraLevel settings x5
		{0x01C8F0D0, 3, 0x12DC0C, umbra_params_new},
		{0x01C8F11C, 3, 0x12DC0C, umbra_params_new},
		{0x01C8F16C, 3, 0x12DC0C, umbra_params_new},
		{0x01C8F1B3, 3, 0x12DC0C, umbra_params_new},
		{0x01C8E013, 2, 0xF167, umbra_params_new / 0x14},           // SetDistanceScale index bias
		// tome trigger
		{0x01C8C81D, 2, 0x12DC48, umbra_trig_new},
		{0x01C8CAFA, 3, 0x12DC48, umbra_trig_new},
		{0x01C8CBFE, 3, 0x12DC48, umbra_trig_new},
		{0x01C8CC72, 3, 0x12DC48, umbra_trig_new},
		{0x01C8DB20, 4, 0x12DC48, umbra_trig_new},
		{0x01C8DB68, 4, 0x12DC48, umbra_trig_new},
		{0x01C8F3FC, 3, 0x12DC48, umbra_trig_new},
		// persistent tome trigger
		{0x01C8CB3E, 3, 0x12DC50, umbra_ptrig_new},
		{0x01C8CBB2, 3, 0x12DC50, umbra_ptrig_new},
		{0x01C8DA64, 4, 0x12DC50, umbra_ptrig_new},
		{0x01C8F41F, 3, 0x12DC50, umbra_ptrig_new},
		// the allocation and its memset
		{0x01C8D345, 1, 0x470210, 0x470300},
		{0x01C8D35E, 2, 0x470210, 0x470300},
	};
	inline constexpr umbra_bound umbra_bounds[] = {
		{0x01C8EFFD, 0x28, 0x50},   // defaults: 2 x 0x14 -> 4 x 0x14
		{0x01C8F08B, 0x28, 0x50},
		{0x01C8F0D7, 0x28, 0x50},
		{0x01C8F123, 0x28, 0x50},
		{0x01C8F173, 0x28, 0x50},
		{0x01C8F1BA, 0x28, 0x50},
		{0x01C8F403, 0x08, 0x10},   // tome triggers: 2 x 4 -> 4 x 4
		{0x01C8F426, 0x08, 0x10},
	};
	inline constexpr lc_gate lensflare_gates[] = {
		{0x014BA810, {0x89, 0x54, 0x24, 0x10, 0x48, 0x89, 0x4C, 0x24, 0x08}, 9}, // per-client pool setup
		{0x014BAC40, {0x40, 0x55, 0x56, 0x57, 0x41, 0x54}, 6},  // SetPersistentData
		{0x014BB9B0, {0x48, 0x8B, 0xC4, 0x57, 0x41, 0x54}, 6},  // per-client update
		{0x014BBD40, {0x48, 0x89, 0x4C, 0x24, 0x08}, 5},        // SpawnInstance
		{0x014BC9E0, {0x48, 0x8B, 0xC4, 0x55, 0x53}, 5},        // per-view render
	};
	// route_lensflares_for_extra_clients: a second FxLensFlaresManager for lc 2/3.
	inline constexpr uint32_t lensflare_manager_rva = 0x032AEC10;
	inline constexpr size_t lensflare_manager_size = 0xB100;       // methods use +0xA000..+0xB090
	inline constexpr uint32_t lensflare_persistent_off = 0xA058;   // persistentData*[2]
	struct lensflare_site
	{
		uint32_t rva;
		uint8_t stock[9];
		uint8_t len;
	};
	// `this` in rcx and lc in edx at each (the entry, or just after an early exit)
	inline constexpr lensflare_site lensflare_lc_entries[] = {
		{0x014BA810, {0x89, 0x54, 0x24, 0x10, 0x48, 0x89, 0x4C, 0x24, 0x08}, 9}, // FixupOnRestore (FX_Restore)
		{0x014BAC40, {0x40, 0x55, 0x56, 0x57, 0x41, 0x54}, 6},  // Init(lc, memory, size) (FX_InitSystem)
		{0x014BB9B0, {0x48, 0x8B, 0xC4, 0x57, 0x41, 0x54}, 6},  // Shutdown(lc) (FX_ShutdownSystem)
		{0x014BBD40, {0x48, 0x89, 0x4C, 0x24, 0x08}, 5},        // SpawnInstance (3 callers)
		{0x014BC9E0, {0x48, 0x8B, 0xC4, 0x55, 0x53}, 5},        // UpdateVisibleLensFlares (per view)
		{0x014BA46A, {0x48, 0x89, 0x5C, 0x24, 0x18}, 5},        // DeleteInstance, after handle == -1 -> ret
		{0x014BB520, {0x48, 0x89, 0x74, 0x24, 0x20}, 5},        // MarkVisibleLensFlare (FX draw)
	};
	// 0x014BC5B0 (this, view, ...): lc = view+0x398, loaded into r15 before this site
	inline constexpr uint32_t lensflare_view_lc_rva = 0x014BC5BC;
	inline constexpr uint8_t lensflare_view_lc_bytes[] = {0x4C, 0x63, 0xBA, 0x98, 0x03, 0x00, 0x00}; // movsxd r15,[rdx+0x398]
	inline constexpr uint32_t lensflare_view_route_rva = 0x014BC5C3;
	inline constexpr uint8_t lensflare_view_route_stock[] = {0x33, 0xDB, 0x48, 0x8B, 0xFA};           // xor ebx,ebx; mov rdi,rdx
	// AllocateLensFlareSource 0x014B97E0: accumulation index = lc * 0x300 + handle
	inline constexpr uint32_t lensflare_accum_premise_rva = 0x014B9835;
	inline constexpr uint8_t lensflare_accum_premise[] = {0x43, 0x8D, 0x2C, 0x76, 0xC1, 0xE5, 0x08};   // lea ebp,[r14+r14*2]; shl ebp,8
	inline constexpr uint32_t lensflare_accum_rva = 0x014B983C;
	inline constexpr uint8_t lensflare_accum_stock[] = {0x03, 0xE8, 0x48, 0x8D, 0x97, 0x40, 0x34, 0x00, 0x00}; // add ebp,eax; lea rdx,[rdi+0x3440]
	// InitSharedResources: element counts of both accumulation buffers, 2 x 0x300 -> 4 x 0x300
	inline constexpr uint32_t lensflare_accum_count_rvas[] = {0x014BB2C7, 0x014BB2FE};
	inline constexpr uint8_t lensflare_accum_count_stock[] = {0xC7, 0x44, 0x24, 0x20, 0x00, 0x06, 0x00, 0x00};
	inline constexpr uint8_t lensflare_accum_count_new[] = {0xC7, 0x44, 0x24, 0x20, 0x00, 0x0C, 0x00, 0x00};
	// the buffers' GfxBuffer records: three resource pointers first (+0x20 = byte size, 0x1800)
	inline constexpr uint32_t lensflare_accum_buffer_rvas[] = {0x09E97C20, 0x09E97BF0};
	inline constexpr size_t lensflare_accum_buffer_ptrs = 0x18;
	inline constexpr uint32_t lensflare_exit_thunk_rva = 0x02EF9840;
	inline constexpr uint8_t lensflare_exit_thunk_expected[] = {
		0x48, 0x8D, 0x0D, 0xC9, 0x53, 0x3B, 0x00,   // lea rcx, [FxLensFlaresManager]
		0xE9, 0x24, 0x24, 0x5C, 0xFE,               // jmp Shutdown
	};
	inline constexpr uint32_t lastinput_init_rva = 0x020E32E0;       // Com_LocalClient_LastInput_Init
	inline constexpr uint8_t lastinput_init_expected[] = {
		0x48, 0x89, 0x5C, 0x24, 0x10,                         // mov [rsp+0x10], rbx
	};
	inline constexpr uint32_t ui_controller_model_getter_rva = 0x0200CEE0;
	inline constexpr uint8_t ui_controller_model_getter_expected[] = {
		0x48, 0x63, 0xC1,                                     // movsxd rax, ecx
		0x48, 0x8D, 0x0D, 0x52, 0xF1, 0x25, 0x14,             // lea rcx, [s_controllerModel]
		0x0F, 0xB7, 0x04, 0x41,                               // movzx eax, word [rcx+rax*2]
		0xC3,
	};
	inline constexpr uint32_t ui_global_model_getter_rva = 0x0200CD10;
	inline constexpr uint8_t ui_global_model_getter_expected[] = {
		0x0F, 0xB7, 0x05, 0x21, 0xF3, 0x25, 0x14,             // movzx eax, word [global model]
		0xC3,
	};
	inline constexpr uint32_t ui_create_persistent_rva = 0x0200C900;
	inline constexpr uint8_t ui_create_persistent_expected[] = {
		0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, // prologue
		0x57, 0x48, 0x83, 0xEC, 0x70,
	};
	inline constexpr uint32_t ui_create_persistent_alloc_rva = 0x0200C965;
	inline constexpr uint8_t ui_create_persistent_alloc_expected[] = {
		0x48, 0x8D, 0x54, 0x24, 0x20,                         // lea rdx, [rsp+0x20]  (key)
		0x41, 0xB0, 0x01,                                     // mov r8b, 1           (persistent)
		0x0F, 0xB7, 0xCF,                                     // movzx ecx, di        (parent)
		0xE8, 0xCB, 0xFC, 0xFF, 0xFF,                         // call UI_Model_AllocateNode
	};
	inline constexpr uint32_t gamepad_models_bound_rva = 0x01340339;
	inline constexpr uint8_t gamepad_models_bound_expected[] = {
		0x83, 0xFE, 0x02,                                     // cmp esi, 2
		0x0F, 0x8C, 0xBE, 0xFE, 0xFF, 0xFF,                   // jl loop head
	};
	inline constexpr uint32_t lobby_max_local_reg_rva = 0x01EDBF02;
	inline constexpr uint8_t lobby_max_local_reg_expected[] = {
		0xB9, 0x69, 0xF0, 0xD2, 0x44,                         // mov ecx, hash
		0x89, 0x5C, 0x24, 0x28,                               // mov [rsp+0x28], ebx  flags
		0x48, 0x89, 0x05, 0x2E, 0x25, 0x7F, 0x13,             // mov [rip+..], rax
		0xC7, 0x44, 0x24, 0x20, 0x02, 0x00, 0x00, 0x00,       // mov dword [rsp+0x20], 2  max
	};

	inline constexpr uint32_t lightq_base = 0x10598670;
	inline constexpr uint32_t lightq_a = 0x105AC670;
	inline constexpr uint32_t lightq_b = 0x105AC678;
	inline constexpr uint32_t ui_global_model_rva = 0x1626C038;
	inline constexpr uint32_t ui_controller_model_rva = 0x1626C03C;  // uint16[2] on the PC
	// widen_visbits_reset: cmp edi, 2
	inline constexpr uint32_t visbits_reset_bound_rva = 0x01F26750;
	inline constexpr uint8_t visbits_reset_bound_bytes[] = {0x83, 0xFF, 0x02};
	// relocate_lightq: instructions rewritten together with the queue
	struct lightq_fixed_site { uint32_t rva; uint8_t len; uint8_t old_bytes[6]; uint8_t new_bytes[6]; };
	inline constexpr lightq_fixed_site lightq_fixed_sites[] = {
		{0x01CEE155, 3, {0x0F, 0x57, 0xC0}, {0x0F, 0x57, 0xC0}},   // xorps xmm0,xmm0 - must be there
		{0x01CEE158, 3, {0x48, 0x89, 0x05}, {0x0F, 0x11, 0x05}},   // mov qword -> movups (A)
		{0x01CEE15F, 3, {0x48, 0x89, 0x05}, {0x0F, 0x11, 0x05}},   // mov qword -> movups (B)
		{0x000B15F8, 6, {0x41, 0xB8, 0x00, 0x40, 0x01, 0x00}, {0x41, 0xB8, 0x00, 0x80, 0x02, 0x00}}, // restore memset
		{0x000B176E, 4, {0x41, 0x83, 0xFF, 0x02}, {0x41, 0x83, 0xFF, 0x03}},   // restore loop
		{0x000B2060, 4, {0x41, 0x83, 0xFD, 0x02}, {0x41, 0x83, 0xFD, 0x03}},   // save loop
	};
	inline constexpr uint32_t umbra_object_rva = 0x0AE15BF8;   // grow_umbra_client_arrays: the culler object pointer
	// perclient_rows descriptors (the driver table stays in 10_relocations_b.inl)
	inline constexpr perclient_array cgdc_array = {"cgdc", 0x049B2CD0, 0x1838, cgdc_sites, std::size(cgdc_sites), 0, {}};
	inline constexpr perclient_array notetracklerps_array = {"g_notetrackLerps", 0x0474B130, 0x340, notetracklerps_sites, std::size(notetracklerps_sites), 0, {}};
	inline constexpr perclient_array cg_pmove_array = {"cg_pmove", 0x04C99740, 0x1660, cg_pmove_sites, std::size(cg_pmove_sites), 0x009B4720, {0x33, 0xD2, 0x0F, 0x57, 0xC0, 0x48, 0x8D, 0x05}};
	inline constexpr perclient_array camerashake_array = {"camerashake", 0x04764990, 0x104, camerashake_sites, std::size(camerashake_sites), 0, {}};
	inline constexpr perclient_array moverinfos_array = {"moverinfos", 0x047641B0, 0x390, moverinfos_sites, std::size(moverinfos_sites), 0, {}};
	inline constexpr perclient_array moveinfoentnum_array = {"moveinfoentnum", 0x04764140, 0x4, moveinfoentnum_sites, std::size(moveinfoentnum_sites), 0, {}};
	inline constexpr perclient_array rumble_array = {"rumble", 0x04C9E470, 0x410, rumble_sites, std::size(rumble_sites), 0, {}};
	inline constexpr perclient_array atglob_array = {"atglob", 0x036007C0, 0x1604, atglob_sites, std::size(atglob_sites), 0, {}};
	inline constexpr perclient_array aimtargetcmd_array = {"aimtargetcmd", 0x03600780, 0x10, aimtargetcmd_sites, std::size(aimtargetcmd_sites), 0, {}};
	inline constexpr perclient_array arcdata_array = {"arcdata", 0x047992B0, 0xEEC, arcdata_sites, std::size(arcdata_sites), 0, {}};
	inline constexpr perclient_array totalcoverage_array = {"totalcoverage", 0x04CC3420, 0x360, totalcoverage_sites, std::size(totalcoverage_sites), 0, {}};
	inline constexpr perclient_array rightstick_array = {"rightstick", 0x0531C760, 0xA, rightstick_sites, std::size(rightstick_sites), 0, {}};
	inline constexpr perclient_array gamepadbuttons_array = {"gamepadbuttons", 0x0531C780, 0x2E, gamepadbuttons_sites, std::size(gamepadbuttons_sites), 0, {}};
	inline constexpr perclient_array screenblur_array = {"screenblur", 0x0479E410, 0x1C, screenblur_sites, std::size(screenblur_sites), 0, {}};
	inline constexpr perclient_array screenelec_array = {"screenelec", 0x0479E448, 0xC, screenelec_sites, std::size(screenelec_sites), 0, {}};
	inline constexpr perclient_array screenburn_array = {"screenburn", 0x0479E460, 0xC, screenburn_sites, std::size(screenburn_sites), 0, {}};
	inline constexpr perclient_array compass_actors_array = {"compass_actors", 0x04785580, 0x2C00, compass_actors_sites, std::size(compass_actors_sites), 0, {}};
	inline constexpr perclient_array compass_vehicles_array = {"compass_vehicles", 0x0478CE80, 0x900, compass_vehicles_sites, std::size(compass_vehicles_sites), 0, {}};
	inline constexpr perclient_array compass_artillery_array = {"compass_artillery", 0x0478E280, 0x78, compass_artillery_sites, std::size(compass_artillery_sites), 0, {}};
	inline constexpr perclient_array compass_heli_array = {"compass_heli", 0x0478E370, 0xE0, compass_heli_sites, std::size(compass_heli_sites), 0, {}};
	inline constexpr perclient_array compass_0240_array = {"compass_0240", 0x0478E530, 0x240, compass_0240_sites, std::size(compass_0240_sites), 0, {}};
	inline constexpr perclient_array compass_0120_array = {"compass_0120", 0x0478E9B0, 0x120, compass_0120_sites, std::size(compass_0120_sites), 0, {}};
	inline constexpr perclient_array compass_0500_array = {"compass_0500", 0x0478FBF0, 0x500, compass_0500_sites, std::size(compass_0500_sites), 0, {}};
	inline constexpr perclient_array compass_0400_array = {"compass_0400", 0x047905F0, 0x400, compass_0400_sites, std::size(compass_0400_sites), 0, {}};
	inline constexpr perclient_array cg_weaponsarray_array = {"cg_weaponsarray", 0x0495A410, 0x8, cg_weaponsarray_sites, std::size(cg_weaponsarray_sites), 0, {}};
	inline constexpr perclient_array cg_ikbuf_array = {"cg_ikbuf", 0x049B25C0, 0x8, cg_ikbuf_sites, std::size(cg_ikbuf_sites), 0, {}};
	inline constexpr perclient_array cg_destructibles_array = {"cg_destructibles", 0x17E820C0, 0x8, cg_destructibles_sites, std::size(cg_destructibles_sites), 0, {}};
	inline constexpr perclient_array numdestructibles_array = {"numdestructibles", 0x17EC11B0, 0x4, numdestructibles_sites, std::size(numdestructibles_sites), 0, {}};
	inline constexpr perclient_array cg_updatetime_array = {"cg_updatetime", 0x17EC11B8, 0x4, cg_updatetime_sites, std::size(cg_updatetime_sites), 0, {}};
	inline constexpr perclient_array destr_gamestates_array = {"destr_gamestates", 0x17E820D0, 0x1080, destr_gamestates_sites, std::size(destr_gamestates_sites), 0, {}};
	inline constexpr perclient_array destr_numgamestates_array = {"destr_numgamestates", 0x17E841D0, 0x4, destr_numgamestates_sites, std::size(destr_numgamestates_sites), 0, {}};
	inline constexpr perclient_array cg_clientents30_array = {"cg_clientents30", 0x041DC500, 0x21840, cg_clientents30_sites, std::size(cg_clientents30_sites), 0, {}};
	inline constexpr perclient_array cg_perclient_3c0_array = {"cg_perclient_3c0", 0x0479DC80, 0x3C0, cg_perclient_3c0_sites, std::size(cg_perclient_3c0_sites), 0, {}};
	inline constexpr perclient_array tnotify_list_array = {"tnotify_list", 0x04CA5820, 0x1F40, tnotify_list_sites, std::size(tnotify_list_sites), 0, {}};
	inline constexpr perclient_array tnotify_head_array = {"tnotify_head", 0x04CA96C0, 0x8, tnotify_head_sites, std::size(tnotify_head_sites), 0, {}};
	inline constexpr perclient_array tnotify_tail_array = {"tnotify_tail", 0x04CA96D0, 0x8, tnotify_tail_sites, std::size(tnotify_tail_sites), 0, {}};
	inline constexpr perclient_array tnotify_free_array = {"tnotify_free", 0x04CA96E0, 0x8, tnotify_free_sites, std::size(tnotify_free_sites), 0, {}};
	inline constexpr perclient_array fxgpu_client_array = {"view_idsets_240", 0x0F48C880, 0x240, fxgpu_client_sites, std::size(fxgpu_client_sites), 0, {}};
	inline constexpr perclient_array scene_pc480_array = {"scene_pc480", 0x0AE134A0, 0x480, scene_pc480_sites, std::size(scene_pc480_sites), 0, {}};
	inline constexpr perclient_array scene_c_array = {"scene_c", scene_c_rva, 0x8, scene_c_sites, std::size(scene_c_sites), 0, {}};
	inline constexpr perclient_array rview_a24_array = {"rview_a24", 0x0F464FCC, 0xA24, rview_a24_sites, std::size(rview_a24_sites), 0, {}};
	inline constexpr perclient_array rview_org30_array = {"rview_org30", 0x0F466430, 0x30, rview_org30_sites, std::size(rview_org30_sites), 0, {}};
	inline constexpr perclient_array aimactors_array = {"aimactors", 0x03600380, 0x200, aimactors_sites, std::size(aimactors_sites), 0, {}};
	inline constexpr perclient_array hudpl_score_array = {"hudpl_score", 0x1A7F6A50, 0x20, hudpl_score_sites, std::size(hudpl_score_sites), 0, {}};
	inline constexpr perclient_array hudpl_gap_array = {"hudpl_gap", 0x1A7F6A90, 0x20, hudpl_gap_sites, std::size(hudpl_gap_sites), 0, {}};
	inline constexpr perclient_array hudpl_flags_array = {"hudpl_flags", 0x1A7F6AD0, 0x20, hudpl_flags_sites, std::size(hudpl_flags_sites), 0, {}};
	inline constexpr perclient_array hudpl_ids_array = {"hudpl_ids", 0x1A7F6B10, 0x20, hudpl_ids_sites, std::size(hudpl_ids_sites), 0, {}};
	inline constexpr perclient_array hudpl_icons_array = {"hudpl_icons", 0x1A7F6B50, 0x40, hudpl_icons_sites, std::size(hudpl_icons_sites), 0, {}};
	inline constexpr perclient_array hudpl_self_array = {"hudpl_self", 0x1A7F6BD0, 0x4, hudpl_self_sites, std::size(hudpl_self_sites), 0, {}};
	inline constexpr perclient_array prevview_array = {"prevview", 0x0FDCC800, 0x290, prevview_sites, std::size(prevview_sites), 0, {}};
	inline constexpr perclient_array visbits_array = {"visbits", 0x1795CEC8, 0x8, visbits_sites, std::size(visbits_sites), 0, {}};
	inline constexpr perclient_array conmsgbuf_array = {"conmsgbuf", conmsgbuf_base, conmsgbuf_stride, conmsgbuf_sites, std::size(conmsgbuf_sites), 0, {}};
	inline constexpr perclient_array uiinfo_array = {"uiinfo", 0x1795D270, 0x1B68, uiinfo_sites, std::size(uiinfo_sites), 0, {}};
	inline constexpr perclient_array ui3d_windows_array = {"ui3d_windows", 0x10B2F2F0, 0x438, ui3d_windows_sites, std::size(ui3d_windows_sites), 0, {}};
	inline constexpr perclient_array playerdetails_array = {"playerdetails", 0x04945090, 0x750, playerdetails_sites, std::size(playerdetails_sites), 0, {}};
	inline constexpr perclient_array actoroverheadfade_array = {"actoroverheadfade", 0x04945F30, 0x400, actoroverheadfade_sites, std::size(actoroverheadfade_sites), 0, {}};
	inline constexpr perclient_array centoverheadfade_array = {"centoverheadfade", 0x04946730, 0xA00, centoverheadfade_sites, std::size(centoverheadfade_sites), 0, {}};
	inline constexpr perclient_array overheadfade_array = {"overheadfade", 0x04947B60, 0x120, overheadfade_sites, std::size(overheadfade_sites), 0, {}};
	inline constexpr perclient_array friendlyheadtrace_array = {"friendlyheadtrace", 0x04947DA0, 0x90, friendlyheadtrace_sites, std::size(friendlyheadtrace_sites), 0, {}};
	inline constexpr perclient_array friendlyactorheadtrace_array = {"friendlyactorheadtrace", 0x04947EC0, 0x200, friendlyactorheadtrace_sites, std::size(friendlyactorheadtrace_sites), 0, {}};
	// widen_name_reset: each Batch 19 array's memset in its reset (0x00677B20, CG_ClearPlayerDetails
	// 0x00677B90): `lea rcx, [array]` (a site of its row) and `mov r8d, 2 * stride`
	struct name_reset
	{
		uint32_t array_base;
		uint32_t lea_rva;
		uint32_t len_rva;
		uint8_t len_bytes[6];
	};
	inline constexpr name_reset name_resets[] = {
		{0x04945090, 0x00677B94, 0x00677B9D, {0x41, 0xB8, 0xA0, 0x0E, 0x00, 0x00}},
		{0x04945F30, 0x00677B38, 0x00677B41, {0x41, 0xB8, 0x00, 0x08, 0x00, 0x00}},
		{0x04946730, 0x00677B4C, 0x00677B55, {0x41, 0xB8, 0x00, 0x14, 0x00, 0x00}},
		{0x04947B60, 0x00677B24, 0x00677B2D, {0x41, 0xB8, 0x40, 0x02, 0x00, 0x00}},
		{0x04947DA0, 0x00677B60, 0x00677B69, {0x41, 0xB8, 0x20, 0x01, 0x00, 0x00}},
		{0x04947EC0, 0x00677B74, 0x00677B7D, {0x41, 0xB8, 0x00, 0x04, 0x00, 0x00}},
	};
	inline constexpr uint32_t compass_actors_clear_rva = 0x0059888D;   // CG_ClearCompassPingData memset length `mov r8d, imm32` (compass_clear_*)
	inline constexpr uint32_t compass_vehicles_clear_rva = 0x005988B5;   // CG_ClearCompassPingData memset length `mov r8d, imm32` (compass_clear_*)
	inline constexpr uint32_t compass_artillery_clear_rva = 0x005988F1;   // CG_ClearCompassPingData memset length `mov r8d, imm32` (compass_clear_*)
	inline constexpr uint32_t compass_heli_clear_rva = 0x00598905;   // CG_ClearCompassPingData memset length `mov r8d, imm32` (compass_clear_*)
	inline constexpr uint32_t compass_0240_clear_rva = 0x00598919;   // CG_ClearCompassPingData memset length `mov r8d, imm32` (compass_clear_*)
	inline constexpr uint32_t compass_0120_clear_rva = 0x0059892D;   // CG_ClearCompassPingData memset length `mov r8d, imm32` (compass_clear_*)
	inline constexpr uint32_t compass_0500_clear_rva = 0x00598941;   // CG_ClearCompassPingData memset length `mov r8d, imm32` (compass_clear_*)
	inline constexpr uint32_t compass_0400_clear_rva = 0x00598955;   // CG_ClearCompassPingData memset length `mov r8d, imm32` (compass_clear_*)

	// ======== splitscreen/11_sun_shadow.inl ========

	// Sun-shadow view setup 0x01D0D790: slot = min(max(CL_SplitscreenPlayerCount(), 1) - 1,
	// viewInfo lc [r13+0x398]) (1 - 1 = 0 when dvar 0x0AE166A0 is set). The whole stretch
	// from the count's call to the slot store is the premise of clamp_sun_shadow_slot().
	inline constexpr uint32_t sun_slot_count_call_rva = 0x01D0D8DA;   // call CL_SplitscreenPlayerCount
	inline constexpr uint8_t sun_slot_premise[] = {
		0xE8, 0xD1, 0x41, 0xAB, 0x00,                     // call 0x027C1AB0
		0x48, 0x8B, 0x0D, 0xBA, 0x8D, 0x10, 0x09,         // mov rcx, [dvar 0x0AE166A0]
		0x41, 0xBF, 0x01, 0x00, 0x00, 0x00,               // mov r15d, 1
		0x8B, 0xD8,                                       // mov ebx, eax
		0x41, 0x3B, 0xC7,                                 // cmp eax, r15d
		0x41, 0x0F, 0x4E, 0xDF,                           // cmovle ebx, r15d
		0xE8, 0xB6, 0x2A, 0x55, 0x00,                     // call Dvar_GetBool 0x022603B0
		0x41, 0x8D, 0x4F, 0x04,                           // lea ecx, [r15+4]
		0x84, 0xC0,                                       // test al, al
		0x41, 0xC7, 0x85, 0x18, 0x1F, 0x00, 0x00, 0x05, 0x00, 0x09, 0x00,   // mov [r13+0x1F18], 0x90005
		0x41, 0x0F, 0x45, 0xDF,                           // cmovne ebx, r15d
		0xE8, 0xEC, 0x76, 0xFC, 0xFF,                     // call 0x01CD5000
		0x8D, 0x4B, 0xFF,                                 // lea ecx, [rbx-1]
		0x0F, 0xB7, 0xC0,                                 // movzx eax, ax
		0xC1, 0xE8, 0x05,                                 // shr eax, 5
		0x89, 0x45, 0x10,                                 // mov [rbp+0x10], eax
		0x41, 0x8B, 0x85, 0x98, 0x03, 0x00, 0x00,         // mov eax, [r13+0x398]
		0x3B, 0xC8,                                       // cmp ecx, eax
		0x0F, 0x4D, 0xC8,                                 // cmovge ecx, eax
		0x89, 0x4D, 0x14,                                 // mov [rbp+0x14], ecx   the slot
	};
	inline constexpr uint32_t sun_slices_rva = 0x01CD12AD;       // mov r8d, <slices>
	inline constexpr uint32_t sun_partitions_rva = 0x01C6FFFD;   // cmp ebx, <partitions>
	inline constexpr uint32_t sun_desc_rt5_rva = 0x01CD1357;
	inline constexpr uint8_t sun_desc_rt5_stock[] = {0x4C, 0x89, 0x85, 0x04, 0x0D, 0x00, 0x00};
	inline constexpr uint32_t sun_desc_rt9_rva = 0x01CD140A;
	inline constexpr uint8_t sun_desc_rt9_stock[] = {0x44, 0x89, 0x85, 0x5C, 0x0D, 0x00, 0x00};
	inline constexpr uint32_t sun_view_check_rva = 0x01CD5449;                           // cmp ax,5
	inline constexpr uint8_t sun_view_check_stock[] = {0x66, 0x83, 0xF8, 0x05};
	inline constexpr uint32_t sun_view_loop_rva = 0x01CD5494;                            // movzx eax,[rsi+0xA86]
	inline constexpr uint8_t sun_view_loop_stock[] = {0x0F, 0xB7, 0x86, 0x86, 0x0A, 0x00, 0x00};
	// R_InitRenderTargets' one use of its descriptor table (rcx = &table, edx = 6 entries).
	inline constexpr uint32_t init_rt_platform_call_rva = 0x01CD47B9;
	inline constexpr uint8_t init_rt_platform_call_stock[] = {0xE8, 0x12, 0x14, 0x00, 0x00};
	inline constexpr uint32_t init_rt_platform_rva = 0x01CD5BD0;   // R_InitRenderTargetsPlatform (PS4 0x9F8D90)
	// R_SetRenderTargetSlice 0x01CF54E0: the view-set lookup, then the colour view
	// inline[clamp(slice, 0, 7)] and the depth pointer at +0x40. Premise of the setter hook.
	inline constexpr uint32_t rt_view_set_rva = 0x01CD5250;       // leaf: records + rt*0xAE0 [+ set*0x150] + 8
	inline constexpr uint32_t sun_setter_call_rva = 0x01CF5508;   // call rt_view_set
	inline constexpr uint32_t sun_setter_premise_rva = 0x01CF54F8;
	inline constexpr uint8_t sun_setter_premise[] = {
		0x48, 0x8B, 0xE9,                                 // mov rbp, rcx
		0x48, 0x81, 0xC1, 0x00, 0x04, 0x00, 0x00,         // add rcx, 0x400
		0x41, 0x8B, 0xD8,                                 // mov ebx, r8d          slice (r8 unchanged)
		0x0F, 0xB7, 0xF2,                                 // movzx esi, dx         rt
		0xE8, 0x43, 0xFD, 0xFD, 0xFF,                     // call rt_view_set
		0xB9, 0x07, 0x00, 0x00, 0x00,                     // mov ecx, 7
		0x3B, 0xD9,                                       // cmp ebx, ecx
		0x44, 0x8B, 0xCB,                                 // mov r9d, ebx
		0x0F, 0xB7, 0xD6,                                 // movzx edx, si
		0x44, 0x0F, 0x4D, 0xC9,                           // cmovge r9d, ecx
		0x33, 0xC9,                                       // xor ecx, ecx
		0x45, 0x85, 0xC9,                                 // test r9d, r9d
		0x44, 0x0F, 0x4E, 0xC9,                           // cmovle r9d, ecx
		0x4E, 0x8B, 0x04, 0xC8,                           // mov r8, [rax+r9*8]    colour view
		0x4C, 0x89, 0x44, 0x24, 0x60,                     // mov [rsp+0x60], r8
		0x48, 0x8B, 0x40, 0x40,                           // mov rax, [rax+0x40]   depth views
		0x4C, 0x8B, 0x34, 0xD8,                           // mov r14, [rax+rbx*8]
	};
	inline constexpr uint32_t sun_clear_rva = 0x01CF367D;         // colour clear of the current slice
	inline constexpr uint8_t sun_clear_stock[] = {
		0x8B, 0x93, 0xC8, 0x80, 0x00, 0x00,   // mov edx,[rbx+0x80C8]   current slice
		0x48, 0x8B, 0x07,                     // mov rax,[rdi]
		0x4C, 0x8B, 0xC6,                     // mov r8,rsi
		0x48, 0x8B, 0x54, 0xD5, 0x00,         // mov rdx,[rbp+rdx*8]    inline[slice]
		0x48, 0x8B, 0xCF,                     // mov rcx,rdi
	};                                        // then call [rax+0x190] (ClearRenderTargetView)
	inline constexpr uint32_t rt_records_ptr_rva = 0x0FBBE758;   // GfxRenderTarget records, stride 0xAE0

	// ======== splitscreen/12_relocations_ui.inl ========

	inline constexpr uint32_t percg_base_rva = 0x04CADB40;
	inline constexpr uint32_t percg_walker_count_rva = 0x02D2CC1A;
	inline constexpr uint8_t percg_walker_expected[] = {0xBD, 0x01, 0x00, 0x00, 0x00};
	inline constexpr percg_ref percg_refs[] = {
		{0x010EB4F0, 7, 3, 0x0, true, {0x48, 0x8D, 0x15, 0x49, 0x26, 0xBC, 0x03}},
		{0x010F30C2, 7, 3, 0x0, true, {0x48, 0x8D, 0x05, 0x77, 0xAA, 0xBB, 0x03}},
		{0x02D2CC1F, 7, 3, 0x830, true, {0x48, 0x8D, 0x1D, 0x4A, 0x17, 0xF8, 0x01}},
		{0x010CD118, 12, 4, 0x2BB0, false, {0x42, 0xC7, 0x84, 0x30, 0xF0, 0x06, 0xCB, 0x04, 0xFF, 0xFF, 0xFF, 0xFF}},
		{0x010CD124, 8, 4, 0x2BA0, false, {0x4A, 0x89, 0x8C, 0x30, 0xE0, 0x06, 0xCB, 0x04}},
		{0x010CD12C, 8, 4, 0x2BA8, false, {0x4A, 0x89, 0x8C, 0x30, 0xE8, 0x06, 0xCB, 0x04}},
	};
	inline constexpr uint32_t uiroot_bound_rva = 0x01F1CC37;
	inline constexpr uint8_t uiroot_bound_expected[] = {0x83, 0xFB, 0x02};
	// UI_CoD_Init's s_rootData clear (PS4 0xD046B1: memset 0x2C0, all four roots): the PC
	// clears 0x160 = two roots, so roots 2/3 kept "in use" (+0xAC) from an earlier UI init.
	// A later UI init without player 3 builds no Lua UIRoot2, and Live_RaiseLUIEvent for
	// controller 2 (a pad plugged in mid-match) indexed nil -> Lua panic, int3 at 0x01D3C84B
	// (2026-10-02). `mov r8d, 0x160` -> 0x2C0, the relocated block's size.
	inline constexpr uint32_t uiroot_clear_size_rva = 0x01F1C9E2;
	inline constexpr uint8_t uiroot_clear_size_expected[] = {0x41, 0xB8, 0x60, 0x01, 0x00, 0x00};
	inline constexpr uint32_t uiroot_bound2_rva = 0x01F1CCDB;
	inline constexpr uint8_t uiroot_bound2_expected[] = {0x83, 0xFB, 0x02};
	inline constexpr uiroot_ref uiroot_refs[] = {
		{0x01F1C1CC, 7, 3, 0x00, true, {0x48, 0x8D, 0x0D, 0xDD, 0x6F, 0x34, 0x14}},
		{0x01F1C3D5, 8, 3, 0xAC, false, {0x80, 0xBC, 0x38, 0x5C, 0x32, 0x26, 0x16, 0x00}},
		{0x01F1C3DF, 7, 3, 0x8C, false, {0x4C, 0x8D, 0x87, 0x3C, 0x32, 0x26, 0x16}},
		{0x01F1C46C, 8, 3, 0xAC, false, {0x80, 0xBC, 0x38, 0x5C, 0x32, 0x26, 0x16, 0x00}},
		{0x01F1C476, 7, 3, 0x8C, false, {0x4C, 0x8D, 0x87, 0x3C, 0x32, 0x26, 0x16}},
		{0x01F1C5E6, 8, 3, 0xAC, false, {0x80, 0xBC, 0x28, 0x5C, 0x32, 0x26, 0x16, 0x00}},
		{0x01F1C5F0, 7, 3, 0x8C, false, {0x48, 0x8D, 0x9D, 0x3C, 0x32, 0x26, 0x16}},
		{0x01F1C79E, 7, 3, 0x00, true, {0x48, 0x8D, 0x05, 0x0B, 0x6A, 0x34, 0x14}},
		{0x01F1C9E8, 7, 3, 0x00, true, {0x48, 0x8D, 0x2D, 0xC1, 0x67, 0x34, 0x14}},
		{0x01F1CB8E, 7, 3, 0x80, true, {0x48, 0x8D, 0x35, 0x9B, 0x66, 0x34, 0x14}},
		{0x01F1CD35, 7, 3, 0xAC, true, {0x48, 0x8D, 0x1D, 0x20, 0x65, 0x34, 0x14}},
		{0x01F1CD3C, 7, 3, 0x8C, true, {0x48, 0x8D, 0x35, 0xF9, 0x64, 0x34, 0x14}},
		{0x01F1D832, 7, 3, 0x00, true, {0x48, 0x8D, 0x05, 0x77, 0x59, 0x34, 0x14}},
		{0x01F21A19, 7, 3, 0x00, true, {0x48, 0x8D, 0x05, 0x90, 0x17, 0x34, 0x14}},
		{0x01F25915, 7, 3, 0x00, true, {0x48, 0x8D, 0x15, 0x94, 0xD8, 0x33, 0x14}},
		{0x01F25D13, 9, 4, 0xAC, false, {0x42, 0x80, 0xBC, 0x39, 0x5C, 0x32, 0x26, 0x16, 0x00}},
		{0x01F25D1E, 7, 3, 0x8C, false, {0x4D, 0x8D, 0x87, 0x3C, 0x32, 0x26, 0x16}},
		{0x01F26428, 7, 3, 0x00, true, {0x4C, 0x8D, 0x2D, 0x81, 0xCD, 0x33, 0x14}},
		{0x01F26AB4, 7, 3, 0xAC, true, {0x48, 0x8D, 0x3D, 0xA1, 0xC7, 0x33, 0x14}},
		{0x01F26ABB, 7, 3, 0x8C, true, {0x48, 0x8D, 0x35, 0x7A, 0xC7, 0x33, 0x14}},
	};
	inline constexpr entcoll_site perctrl_sites[] = {
		{0x01F14F0D, 3, 7, true , 0x8},    // lea rcx,[+8]    subscribers++
		{0x01F14F47, 3, 7, true , 0x4},    // lea rax,[+4]    UI_CoD_BlurWorld (setter; was missing:
		                                   // controllers 2/3 wrote 0x1626333C / 0x16263350, the glyph buffer)
		{0x01F1A550, 3, 7, true , 0x4},    // lea rcx,[+4]    float getter
		{0x01F1C68C, 5, 9, false, 0xC},    // mulss xmm1,[rbp+rbx*4+RVA+0xC]
		{0x01F1C6B8, 5, 9, false, 0x10},   // mulss xmm1,[rbp+rbx*4+RVA+0x10]
		{0x01F1CA11, 3, 7, true , 0x0},    // lea r14,[base]  init clear
		{0x01F1D09E, 3, 8, false, 0x0},    // cmp byte [rcx+rax*4+RVA],0
		{0x01F1D0E0, 3, 7, true , 0x1},    // lea rcx,[+1]    UI_CoD_IsUIActive
		{0x01F1F66E, 3, 7, true , 0x0},    // lea rax,[base]  flag setter
		{0x01F21363, 6, 10, false, 0xC},   // movss [r14+rax*4+RVA+0xC],xmm6
		{0x01F2136D, 6, 10, false, 0x10},  // movss [r14+rax*4+RVA+0x10],xmm7
		{0x01F2197D, 3, 7, true , 0x8},    // lea rcx,[+8]    subscribers--
		{0x01F265E7, 3, 7, true , 0x1},    // lea rax,[+1]
	};
	inline constexpr uint32_t perctrl_clear_rva = 0x01F1CA0D;                    // lea r8d,[rdx+0x28]
	inline constexpr uint8_t perctrl_clear_stock[] = {0x44, 0x8D, 0x42, 0x28};
	inline constexpr entcoll_site lui_armblade_sites[] = {
		{0x01FF9FF3, 3, 7, true, 0x10}, {0x01FFA053, 3, 7, true, 0x0}, {0x01FFA0D7, 3, 7, true, 0x0},
		{0x01FFA157, 3, 7, true, 0x0}, {0x01FFBA12, 4, 8, false, 0x4}, {0x01FFEB9D, 5, 9, false, 0x8},
		{0x01FFEBAB, 5, 9, false, 0xC}, {0x01FFEBB4, 5, 9, false, 0x10}, {0x01FFEBC5, 4, 8, false, 0x0},
		{0x01FFEBE7, 3, 7, false, 0x14}, {0x01FFEBEE, 3, 7, false, 0x14}, {0x01FFEC0A, 3, 11, false, 0x4},
		{0x01FFEC15, 3, 11, false, 0x14}, {0x01FFEC20, 4, 8, false, 0x8}, {0x0200C192, 3, 7, true, 0x0},
		{0x0200C3E6, 3, 7, true, 0x0},
	};
	inline constexpr entcoll_site lui_rocket_sites[] = {
		{0x01FFA1A7, 3, 7, true, 0x0}, {0x0200BFDE, 4, 8, false, 0x0}, {0x0200BFFF, 4, 8, false, 0x2},
		{0x0200C25E, 5, 9, false, 0x0}, {0x0200C343, 3, 7, true, 0x0}, {0x0200C5C9, 3, 7, true, 0x0},
	};
	inline constexpr entcoll_site lui_reticle_sites[] = {
		{0x01FFA187, 3, 7, true, 0x0}, {0x02000639, 4, 9, false, 0x0}, {0x02000689, 6, 10, false, 0x4},
		{0x020006B7, 4, 8, false, 0x0}, {0x020006C1, 6, 10, false, 0x4}, {0x020006DA, 6, 10, false, 0x4},
		{0x020006E6, 4, 8, false, 0x0}, {0x020006F0, 6, 10, false, 0x4}, {0x0200071B, 6, 10, false, 0x4},
		{0x02000760, 4, 8, false, 0x4}, {0x0200078E, 6, 10, false, 0x4}, {0x020007C1, 6, 10, false, 0x4},
	};
	inline constexpr entcoll_site lui_weakpoint_sites[] = {
		{0x01FDF700, 3, 7, true, 0x0}, {0x01FED34C, 5, 9, false, 0x4}, {0x01FED355, 4, 8, false, 0x0},
		{0x01FEEDC5, 4, 8, false, 0x0}, {0x01FEEE37, 4, 8, false, 0x0}, {0x01FEEEAD, 6, 10, false, 0x8},
		{0x01FEEEC9, 6, 10, false, 0xC}, {0x01FEEF28, 5, 9, false, 0x4}, {0x01FEEF50, 5, 9, false, 0x4},
		{0x01FEEF65, 4, 8, false, 0x0}, {0x01FF2847, 5, 9, false, 0x10}, {0x01FF2868, 5, 9, false, 0x10},
		{0x01FF96E5, 3, 7, true, 0x6}, {0x01FF9734, 3, 7, true, 0x0},
	};
	inline constexpr lui_bound lui_bounds[] = {
		{0x01FFA01E, {0x48, 0x83, 0xF8, 0x08}, 0x10},   // arm-blade clear: cmp rax,8
		{0x0200C35C, {0x48, 0x83, 0xF9, 0x02}, 0x04},   // rocket-launcher clear: cmp rcx,2
	};
	inline constexpr uint32_t cg_marks_base = 0x04748220;
	inline constexpr entcoll_site cg_marks_sites[] = {
		{0x00220447, 3, 7, true, 0x38},   // lea rax,[+0x38]   per-client init
		{0x002287DD, 3, 7, true, 0x8},    // lea rax,[+0x8]
		{0x00228863, 3, 7, true, 0x0},    // lea rcx,[base]    block getter
		{0x00228883, 3, 7, true, 0x0},    // lea rcx,[base]    find by entity
		{0x00233BA4, 3, 7, false, 0x0},   // lea rbx,[rax+RVA]
	};
	inline constexpr uint32_t le_active_rva = 0x0494A900;
	inline constexpr uint32_t le_free_rva = 0x0494AAF0;
	inline constexpr uint32_t le_pool_rva = 0x0494AB10;
	inline constexpr le_ref le_refs[] = {
		{0x0083B986, 7, 3, le_active, 0x0, true, {0x48, 0x8D, 0x05, 0x73, 0xEF, 0x10, 0x04}},
		{0x0083D20B, 8, 4, le_free, 0x0, false, {0x4B, 0x8B, 0x84, 0xE5, 0xF0, 0xAA, 0x94, 0x04}},
		{0x0083D217, 8, 4, le_free, 0x0, false, {0x4B, 0x89, 0x9C, 0xE5, 0xF0, 0xAA, 0x94, 0x04}},
		{0x0083D519, 9, 4, le_free, 0x0, false, {0x48, 0x83, 0xBC, 0xFE, 0xF0, 0xAA, 0x94, 0x04, 0x00}},
		{0x0083D52E, 8, 4, le_active, 0x0, false, {0x48, 0x8B, 0x94, 0x32, 0x00, 0xA9, 0x94, 0x04}},
		{0x0083D53B, 8, 4, le_free, 0x0, false, {0x48, 0x8B, 0x9C, 0xFE, 0xF0, 0xAA, 0x94, 0x04}},
		{0x0083D552, 8, 4, le_free, 0x0, false, {0x48, 0x89, 0x84, 0xFE, 0xF0, 0xAA, 0x94, 0x04}},
		{0x0083D569, 8, 4, le_active, 0x8, false, {0x48, 0x8B, 0x8C, 0x37, 0x08, 0xA9, 0x94, 0x04}},
		{0x0083D575, 7, 3, le_active, 0x0, false, {0x48, 0x8D, 0x8E, 0x00, 0xA9, 0x94, 0x04}},
		{0x0083D582, 8, 4, le_active, 0x8, false, {0x48, 0x8B, 0x8C, 0x37, 0x08, 0xA9, 0x94, 0x04}},
		{0x0083D58D, 8, 4, le_active, 0x8, false, {0x48, 0x89, 0x9C, 0x37, 0x08, 0xA9, 0x94, 0x04}},
		{0x0083D5F9, 7, 3, le_free, 0x0, true, {0x48, 0x8D, 0x0D, 0xF0, 0xD4, 0x10, 0x04}},
		{0x0083D632, 7, 3, le_pool, 0x0, true, {0x48, 0x8D, 0x35, 0xD7, 0xD4, 0x10, 0x04}},
		{0x0083D667, 7, 3, le_active, 0x0, false, {0x48, 0x8D, 0x82, 0x00, 0xA9, 0x94, 0x04}},
		{0x0083D66E, 8, 4, le_free, 0x0, false, {0x48, 0x89, 0x9C, 0xFA, 0xF0, 0xAA, 0x94, 0x04}},
		{0x0083D67D, 8, 4, le_active, 0x8, false, {0x48, 0x89, 0x84, 0x11, 0x08, 0xA9, 0x94, 0x04}},
		{0x0083D68B, 7, 3, le_pool, 0x8, true, {0x48, 0x8D, 0x0D, 0x86, 0xD4, 0x10, 0x04}},
	};
	inline constexpr uint32_t exploder_count_rva = 0x043016C4;
	// relocate_radiant_exploders: the instruction each exploder_expect_* array verifies
	inline constexpr uint32_t exploder_site_A49 = 0x00200A49;
	inline constexpr uint32_t exploder_site_A5C = 0x00200A5C;
	inline constexpr uint32_t exploder_site_703 = 0x00205703;
	inline constexpr uint32_t exploder_site_7A4 = 0x001FD7A4;
	inline constexpr uint32_t exploder_site_AF7 = 0x00200AF7;
	inline constexpr uint32_t exploder_site_AC9 = 0x00200AC9;
	inline constexpr uint32_t exploder_site_589 = 0x00205589;
	inline constexpr uint32_t exploder_site_5D7 = 0x002055D7;
	inline constexpr uint32_t exploder_site_6E3 = 0x002056E3;
	inline constexpr uint32_t exploder_site_5CF = 0x002055CF;
	inline constexpr uint32_t exploder_site_79B = 0x001FD79B;
	inline constexpr uint32_t exploder_site_A38 = 0x00200A38;
	inline constexpr uint8_t exploder_expect_A49[] = {0x48, 0x69, 0xC9, 0x30, 0x39, 0x00, 0x00};
	inline constexpr uint8_t exploder_expect_A5C[] = {0x48, 0x81, 0xC6, 0x30, 0x39, 0x00, 0x00};
	inline constexpr uint8_t exploder_expect_703[] = {0x49, 0x81, 0xC0, 0x30, 0x39, 0x00, 0x00};
	inline constexpr uint8_t exploder_expect_7A4[] = {0x41, 0xB8, 0x00, 0x30, 0x39, 0x00};
	inline constexpr uint8_t exploder_expect_AF7[] = {0x48, 0x89, 0xAC, 0xC6, 0xF0, 0x19, 0x00, 0x00};
	inline constexpr uint8_t exploder_expect_AC9[] = {0x48, 0x8D, 0x48, 0x08};
	inline constexpr uint8_t exploder_expect_589[] = {0x48, 0x8D, 0x3D, 0x40, 0xDB, 0x0F, 0x04};
	inline constexpr uint8_t exploder_expect_5D7[] = {0x48, 0x8D, 0x3D, 0xF2, 0xDA, 0x0F, 0x04};
	inline constexpr uint8_t exploder_expect_6E3[] = {0x48, 0x8D, 0x3D, 0xE6, 0xD9, 0x0F, 0x04};
	inline constexpr uint8_t exploder_expect_5CF[] = {0x48, 0x63, 0x84, 0x87, 0xC8, 0x30, 0x30, 0x04};
	inline constexpr uint8_t exploder_expect_79B[] = {0x48, 0x8D, 0x0D, 0x3E, 0x3F, 0x10, 0x04};
	inline constexpr uint8_t exploder_expect_A38[] = {0x48, 0x8D, 0x35, 0xA1, 0x0C, 0x10, 0x04};

	inline constexpr uint32_t uiroot_base_rva = 0x162631B0;
	inline constexpr uint32_t perctrl_base = 0x16263310;
	inline constexpr uint32_t lui_armblade_base_rva = 0x1626BF60;   // free records carry entity 0x3FF
	inline constexpr lui_table lui_tables[] = {
		{"weakpoints", 0x1626BDB0, 0x14, 20, 40, lui_weakpoint_sites, std::size(lui_weakpoint_sites)},
		{"reticle", 0x1626BF40, 0x8, 2, 4, lui_reticle_sites, std::size(lui_reticle_sites)},
		{"rocket launcher", 0x1626BF58, 0x4, 2, 4, lui_rocket_sites, std::size(lui_rocket_sites)},
		{"arm blade", lui_armblade_base_rva, 0x18, 8, 16, lui_armblade_sites, std::size(lui_armblade_sites)},
	};
	// relocate_lui_roots: the two root loops that stop at two (cmp reg,2)
	struct lui_root_bound { uint32_t rva; uint8_t modrm; };
	inline constexpr lui_root_bound lui_root_bounds[] = {
		{0x01F26B4D, 0xFB},   // cmp ebx,2
		{0x01F1CD94, 0xFF},   // cmp edi,2  HUD menus
	};
	// ======== splitscreen/13_lobby_join.inl ========

	inline constexpr uint32_t lobby_host_add_local_rva = 0x01ECAAF0;
	inline constexpr uint32_t lobby_get_session_rva = 0x01ED03E0;
	inline constexpr uint32_t lobby_get_client_by_xuid_rva = 0x01EF3920;
	inline constexpr uint32_t live_user_get_xuid_rva = 0x01EBA880;
	inline constexpr uint32_t mutable_client_info_rva = 0x01EBEB00;
	inline constexpr uint32_t lobby_update_client_rva = 0x01EF5590;
	inline constexpr uint8_t lobby_host_add_local_bytes[] = {
		0xE9, 0xDB, 0xC7, 0x00, 0x00,
	};
	inline constexpr uint8_t lobby_get_session_bytes[] = {
		0x83, 0xF9, 0x01, 0x77, 0x16, 0x48, 0x63, 0xC1,
	};
	inline constexpr uint8_t lobby_get_client_bytes[] = {
		0x4C, 0x8D, 0x89, 0xF8, 0x00, 0x00, 0x00, 0x33, 0xC0,
	};
	inline constexpr uint8_t live_user_get_xuid_bytes[] = {
		0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0xD9,
	};
	inline constexpr uint8_t mutable_client_info_bytes[] = {
		0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x55, 0x41, 0x56,
	};
	inline constexpr uint8_t lobby_update_client_bytes[] = {
		0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
		0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57,
	};
	inline constexpr uint32_t lobby_get_network_mode_rva = 0x01EDB7F0;
	inline constexpr uint8_t lobby_get_network_mode_bytes[] = {
		0x8B, 0x05, 0x26, 0x2B, 0x7F, 0x13, 0xC3,
	};
	inline constexpr uint32_t lobby_add_all_rva = 0x01ECAB00;
	inline constexpr uint8_t lobby_add_all_prologue[] = {
		0x48, 0x8B, 0xC4, 0x57, 0x41, 0x54, 0x41, 0x55,
		0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0xB0,
	};
	inline constexpr uint32_t lobbyvm_local_leave_rva = 0x01EE3B40;
	inline constexpr uint8_t lobbyvm_local_leave_prologue[] = {
		0x40, 0x57,                                  // push rdi
		0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00,    // sub rsp, 0xB0
		0x48, 0xC7, 0x44, 0x24, 0x38, 0xFE, 0xFF, 0xFF, 0xFF,
	};
	inline constexpr uint8_t guest_signin_prologue[] = {
		0x48, 0x89, 0x5C, 0x24, 0x08,                // mov [rsp+8], rbx
		0x48, 0x89, 0x6C, 0x24, 0x18,                // mov [rsp+18h], rbp
		0x48, 0x89, 0x74, 0x24, 0x20,                // mov [rsp+20h], rsi
		0x57, 0x41, 0x56, 0x41, 0x57,                // push rdi / r14 / r15
	};
	inline constexpr uint32_t lobby_host_is_host_rva = 0x01ECC700;
	inline constexpr uint8_t lobby_host_is_host_bytes[] = {
		0x48, 0x83, 0xEC, 0x28, 0xE8, 0xD7, 0x3C, 0x00, 0x00,   // call 0x01EDCDD0
	};
	inline constexpr uint32_t lobby_host_remove_client_rva = 0x01ECD250;
	inline constexpr uint8_t lobby_host_remove_client_bytes[] = {
		0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
		0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20,
	};
	inline constexpr uint32_t local_client_left_reason_rva = 0x02FB1758;
	inline constexpr uint32_t swap_clients_rva = 0x020E39D0;
	inline constexpr uint8_t swap_clients_prologue[] = {
		0x89, 0x54, 0x24, 0x10,                               // mov [rsp+0x10], edx
		0x89, 0x4C, 0x24, 0x08,                               // mov [rsp+8], ecx
		0x53, 0x55, 0x56, 0x57, 0x41, 0x54,                   // push rbx/rbp/rsi/rdi/r12
	};
	inline constexpr uint32_t game_button_a = 0x00000100;
	inline constexpr uint32_t game_button_b = 0x00000200;
	inline constexpr uint32_t set_active_rva = 0x027C19C0;
	inline constexpr uint8_t set_active_prologue[] = {
		0x48, 0x89, 0x5C, 0x24, 0x08, // mov [rsp+8], rbx
		0x48, 0x89, 0x74, 0x24, 0x10, // mov [rsp+0x10], rsi
		0x57,                         // push rdi
		0x48, 0x83, 0xEC, 0x20,       // sub rsp, 0x20
	};

	// ======== splitscreen/14_player4_fixes.inl ========

	// DynEntCl_CleanUpOldModels 0x0146DBE0 (PS4 0x595790): `vec3_t viewOrigins[4]` on PS4,
	// room for 2 on the PC ([rbp-0x49], stride 0xC, stack cookie at [rbp-0x29]); the loop
	// runs `lc < cl_maxLocalClients`. Loop condition `cmp ebx, r8d` -> `cmp ebx, 2`.
	inline constexpr uint32_t dynent_cleanup_viewer_bound_rva = 0x0146F3F8;
	inline constexpr uint8_t dynent_cleanup_viewer_bound_stock[] = {0x41, 0x3B, 0xD8};
	inline constexpr uint8_t dynent_cleanup_viewer_bound_fixed[] = {0x83, 0xFB, 0x02};

	// Streamer view positions. PS4 streamFrontendGlob +0x1CE4BC savedClientPrevViewPos[4],
	// +0x1CE4EC savedClientViewPos[4], +0x1CE51C numClientsLastFrame. PC glob 0x10698100:
	// prev[2] 0x10AB2768, cur[2] 0x10AB2780, a PC-only bool[2] 0x10AB2798 (set: no
	// prev->cur extrapolation), numClientsLastFrame 0x10AB279C. A third view's position
	// lands on numClientsLastFrame, so `queryClient == numClientsLastFrame` never holds and
	// the combine/sort jobs never run: no streamed mesh loads (Nuk3town cars, 2026-10-01).
	// Every access (rip-relative and glob+disp32 scans): the three sites below.
	inline constexpr uint32_t stream_glob_rva = 0x10698100;
	// R_Stream_BeginUpdateFrame 0x01D07480: prev = cur; cur = 0; bool[0..1] = 0.
	inline constexpr uint32_t stream_begin_views_rva = 0x01D075B8;
	inline constexpr uint8_t stream_begin_views_bytes[] = {
		0x0F, 0x28, 0x05, 0xC1, 0xB1, 0xDA, 0x0E,       // movaps xmm0, [cur]
		0x0F, 0x11, 0x05, 0xA2, 0xB1, 0xDA, 0x0E,       // movups [prev], xmm0
		0xF2, 0x0F, 0x10, 0x0D, 0xC2, 0xB1, 0xDA, 0x0E, // movsd xmm1, [cur+0x10]
		0xF2, 0x0F, 0x11, 0x0D, 0xA2, 0xB1, 0xDA, 0x0E, // movsd [prev+0x10], xmm1
		0x33, 0xC0,                                     // xor eax, eax
		0x48, 0x89, 0x05, 0xA1, 0xB1, 0xDA, 0x0E,       // mov [cur], rax
		0x48, 0x89, 0x05, 0xA2, 0xB1, 0xDA, 0x0E,       // mov [cur+8], rax
		0x48, 0x89, 0x05, 0xA3, 0xB1, 0xDA, 0x0E,       // mov [cur+0x10], rax
		0x66, 0x89, 0x05, 0xA4, 0xB1, 0xDA, 0x0E,       // mov word [bool], ax
	};
	// R_Stream_UpdateForClient (inlined, PS4 0xA68A60), r12 = glob: cur[qc].x/y/z and bool[qc].
	struct stream_disp_site
	{
		uint32_t rva;      // instruction start; the disp32 is at +4
		uint8_t bytes[8];
	};
	inline constexpr stream_disp_site stream_view_stores[] = {
		{0x01D09856, {0x41, 0x89, 0x84, 0x94, 0x80, 0xA6, 0x41, 0x00}}, // [r12+rdx*4+0x41A680], eax
		{0x01D09862, {0x41, 0x89, 0x84, 0x94, 0x84, 0xA6, 0x41, 0x00}}, // [r12+rdx*4+0x41A684], eax
		{0x01D0986E, {0x41, 0x89, 0x84, 0x94, 0x88, 0xA6, 0x41, 0x00}}, // [r12+rdx*4+0x41A688], eax
		{0x01D0987D, {0x46, 0x88, 0xBC, 0x20, 0x98, 0xA6, 0x41, 0x00}}, // [rax+r12+0x41A698], r15b
	};
	// R_Stream_UpdateStaticAllClients_Internal 0x01D09A70: per view i < numClientsLastFrame,
	// r14 = &bool[i], rbx = &cur[i].y, prev[i] read as rbx-0x1C/-0x18/-0x14.
	inline constexpr uint32_t stream_static_bool_lea_rva = 0x01D09CF3;
	inline constexpr uint8_t stream_static_bool_lea_bytes[] = {0x4C, 0x8D, 0x35, 0x9E, 0x8A, 0xDA, 0x0E};
	inline constexpr uint32_t stream_static_cur_lea_rva = 0x01D09CFF;
	inline constexpr uint8_t stream_static_cur_lea_bytes[] = {0x48, 0x8D, 0x1D, 0x7E, 0x8A, 0xDA, 0x0E};
	inline constexpr uint32_t stream_static_prev_subs_rva = 0x01D09D1C;
	inline constexpr uint8_t stream_static_prev_subs_bytes[] = {
		0xF3, 0x0F, 0x5C, 0x43, 0xE4,  // subss xmm0, [rbx-0x1C]
		0xF3, 0x0F, 0x10, 0x0B,        // movss xmm1, [rbx]
		0xF3, 0x0F, 0x5C, 0x4B, 0xE8,  // subss xmm1, [rbx-0x18]
		0xF3, 0x0F, 0x10, 0x53, 0x04,  // movss xmm2, [rbx+4]
		0xF3, 0x0F, 0x5C, 0x53, 0xEC,  // subss xmm2, [rbx-0x14]
	};
	inline constexpr size_t stream_static_prev_disp8_offs[] = {4, 13, 23};
	// Then the 8 streamer hints are appended to the same stack StreamUpdateCmd. Its
	// streamView array holds 10 (cmd at rsp+0x50, count at rsp+0x58, views from rsp+0x5C,
	// stride 0x1C, stack cookie at rsp+0x180): 2 views + 8 hints on the PC, unchecked.
	// Hint loop body; the midhook takes the first 8 bytes, the jbe stays.
	inline constexpr uint32_t stream_hint_body_rva = 0x01D09DA0;
	inline constexpr uint8_t stream_hint_body_bytes[] = {
		0xF3, 0x0F, 0x10, 0x4B, 0x10,       // movss xmm1, [rbx+0x10]
		0x0F, 0x2F, 0xCE,                   // comiss xmm1, xmm6
		0x0F, 0x86, 0x87, 0x00, 0x00, 0x00, // jbe 0x01D09E35 (skip this hint)
	};
	inline constexpr size_t stream_hint_hook_len = 8;
	inline constexpr uint32_t stream_hint_skip_rva = 0x01D09E35;
	static_assert(stream_hint_body_rva + 14 + 0x87 == stream_hint_skip_rva);
	inline constexpr int32_t stream_update_cmd_views = 10;

	// Pad Start/Back for players 2-4. The PC binds BUTTON_START "togglemenu" and
	// BUTTON_BACK "togglescores" only in default_bindings_<language>.cfg, which runs for
	// player 1; the per-controller pad layouts (gamedata/configs/common/buttons/<layout>
	// [_fl], exec'd by Settings_UpdateButtonConfig 0x016501B0, PS4 0x6F5350) bind
	// neither. Measured 2026-10-01: playerKeys keys[14] (K_BUTTON_START) and [15]
	// (K_BUTTON_BACK) bound for lc0 only. The layout exec call:
	inline constexpr uint32_t button_config_exec_call_rva = 0x01650206;
	inline constexpr uint8_t button_config_exec_call_bytes[] = {0xE8, 0xF5, 0x09, 0xA9, 0x00};
	inline constexpr uint32_t cmd_execute_single_command_rva = 0x020E0C00;   // (lc, controller, text, r9)
	// Pad update 0x02286030: settings 16/17 (stick-click limits) read via Settings_GetFloat
	// before L3/R3 are dropped for a deflected stick; no-profile fallback 1.0 at 0x0228608E.
	// install_guest_stick_click_thresholds (patch by rdevathu, public PR 1) redirects both reads.
	inline constexpr uint32_t stick_click_get_float_rva = 0x0164E800;
	inline constexpr uint32_t stick_click_left_call_rva = 0x022860AB;
	inline constexpr uint32_t stick_click_right_call_rva = 0x022860BA;
	inline constexpr uint8_t stick_click_left_call_bytes[] = {0xE8, 0x50, 0x87, 0x3C, 0xFF};
	inline constexpr uint8_t stick_click_right_call_bytes[] = {0xE8, 0x41, 0x87, 0x3C, 0xFF};
	static_assert(stick_click_left_call_rva + 5 - 0x00C378B0 == stick_click_get_float_rva);
	static_assert(stick_click_right_call_rva + 5 - 0x00C378BF == stick_click_get_float_rva);
	// `sub rsp,38h; mov byte [rsp+20h],0` - checked before the update notice calls it directly.
	inline constexpr uint8_t cmd_execute_single_command_prologue[] = {0x48, 0x83, 0xEC, 0x38, 0xC6, 0x44, 0x24, 0x20, 0x00};
	// Live_RaiseLUIEvent(controller, name) (PS4 0xC13EA0; CL_ControllerInserted raises
	// "controller_inserted" with it): `push rdi; sub rsp,0A0h`.
	inline constexpr uint32_t live_raise_lui_event_rva = 0x01E00EE0;
	inline constexpr uint8_t live_raise_lui_event_prologue[] = {0x40, 0x57, 0x48, 0x81, 0xEC, 0xA0, 0x00, 0x00, 0x00};
	static_assert(button_config_exec_call_rva + 5 + 0x00A909F5 == cmd_execute_single_command_rva);

	// CG_CanPauseGame 0x00843BD0 (PS4 0x224FB0). The PC adds "Zombies/Campaign with more
	// than one player: no pause" (0x00843C12..0x00843C33); Multiplayer pauses whenever
	// every client is local, and UI_SetActiveMenu then opens the pause menu for every
	// other local client (PC loop 0x0223371D, PS4 0xFA21B8). Not a detour: the function
	// tail-jumps into CG_AllClientsAreLocal 0x008C1AA0, which has an Arxan caller guard
	// (hangs for a caller outside the image). Midhook on the instruction every
	// "may pause" path passes; its false exit:
	inline constexpr uint32_t cg_can_pause_mp_rva = 0x00843C35;
	inline constexpr uint8_t cg_can_pause_mp_bytes[] = {0xB9, 0x01, 0x00, 0x00, 0x00};   // mov ecx, 1
	inline constexpr uint32_t cg_can_pause_false_rva = 0x00843BF2;
	inline constexpr uint8_t cg_can_pause_false_bytes[] = {0x32, 0xC0, 0x48, 0x83, 0xC4, 0x28, 0xC3};

	// Gamepad device assignment 0x022849F0: `call CL_SplitscreenPlayerCount` in its final
	// test (new device && count > 1 && slot 1 has no device -> give it to controller 1).
	inline constexpr uint32_t assign_player_count_call_rva = 0x02284AE8;
	inline constexpr uint8_t assign_player_count_call_bytes[] = {0xE8, 0xC3, 0xCF, 0x53, 0x00};
	static_assert(assign_player_count_call_rva + 5 + 0x0053CFC3 == splitscreen_player_count_rva);

	inline constexpr uint32_t per_controller_update_rva = 0x01E19AE0;
	inline constexpr uint8_t per_controller_update_prologue[] = {0x48, 0x8B, 0xC4, 0x55, 0x41, 0x54};
	inline constexpr end_bound_fix client_ui_end_bounds[] = {
		{0x0134B907, {0x48, 0x8D, 0x15, 0xA2, 0x03, 0x01, 0x04}},
		{0x0135950F, {0x48, 0x8D, 0x0D, 0x9A, 0x27, 0x00, 0x04}},
		{0x01359B92, {0x48, 0x8D, 0x0D, 0x17, 0x21, 0x00, 0x04}},
		{0x0135A27B, {0x48, 0x8D, 0x15, 0x2E, 0x1A, 0x00, 0x04}},
		{0x0135D1BB, {0x48, 0x8D, 0x15, 0xEE, 0xEA, 0xFF, 0x03}},
		{0x027C171E, {0x4C, 0x8D, 0x0D, 0x8B, 0xA5, 0xB9, 0x02}},
		{0x027C1789, {0x48, 0x8D, 0x15, 0x20, 0xA5, 0xB9, 0x02}},
		{0x027C1890, {0x48, 0x8D, 0x0D, 0x19, 0xA4, 0xB9, 0x02}},
		{0x020ECA0F, {0x48, 0x8D, 0x15, 0xA2, 0xF2, 0x26, 0x03}},
		{0x027C1617, {0x48, 0x8D, 0x0D, 0x9A, 0xA6, 0xB9, 0x02}},
		{0x027C1650, {0x48, 0x8D, 0x0D, 0x61, 0xA6, 0xB9, 0x02}},
		{0x027C1690, {0x48, 0x8D, 0x0D, 0x21, 0xA6, 0xB9, 0x02}},
	};
	inline constexpr size_t active_count_rva = 0x027C1A0D;
	inline constexpr uint8_t active_count_bytes[] = {
		0x40, 0x84, 0x35, 0xAC, 0x81, 0xB9, 0x02, // test byte [rip+..], sil
		0xB8, 0x00, 0x00, 0x00, 0x00,             // mov eax, 0
		0x0F, 0x45, 0xC6,                         // cmovne eax, esi
		0x40, 0x84, 0x35, 0x15, 0x92, 0xB9, 0x02, // test byte [rip+..], sil
		0x74, 0x02,                               // je +2
		0xFF, 0xC0,                               // inc eax
	};
	inline constexpr size_t status_rva = 0x1A828D00;
	// widen_client_shutdown_loops: CL_Disconnect loop bound (cmp edi, 2)
	inline constexpr uint32_t disconnect_loop_bound_rva = 0x020F0797;
	inline constexpr uint8_t disconnect_loop_bound_bytes[] = {0x83, 0xFF, 0x02};
	// Com_ShutdownInternal loops that stop at two (cmp reg,2); the UI close loops index uiInfoArray
	struct shutdown_site { uint32_t rva; uint8_t modrm; const char* what; bool needs_uiinfo; };
	inline constexpr shutdown_site com_shutdown_sites[] = {
		{0x020F11CE, 0xFF, "Com_ShutdownInternal disconnect loop", false},
		{0x020F188B, 0xFB, "inlined Com_ShutdownInternal disconnect loop", false},
		{0x020F1219, 0xFB, "Com_ShutdownInternal UI close loop", true},
		{0x020F18DC, 0xFB, "inlined Com_ShutdownInternal UI close loop", true},
	};
	// the cgame shutdown walk: mov edi,1 (start client) / lea rbx,[clientUIActives[1]]
	inline constexpr uint32_t cgame_shutdown_start_rva = 0x0132E31A;
	inline constexpr uint8_t cgame_shutdown_start_bytes[] = {0xBF, 0x01, 0x00, 0x00, 0x00};
	inline constexpr uint8_t cgame_shutdown_start_new[] = {0xBF, 0x03, 0x00, 0x00, 0x00};
	inline constexpr uint32_t cgame_shutdown_cursor_rva = 0x0132E31F;
	inline constexpr uint8_t cgame_shutdown_cursor_bytes[] = {0x48, 0x8D, 0x1D, 0x12, 0xC9, 0x02, 0x04};

	// ======== splitscreen/15_stats_cache.inl ========

	// "statresponse": the server sends the missing stats-transfer packets as two unsigned
	// masks (SV_ReceiveTransferData 0x021EB62C: va("statresponse %Iu %Iu"), string 0x02FD1740);
	// CL_DispatchConnectionlessPacket reads them with I_atoi64 (0x0227C180, the signed CRT
	// _atoi64) at these two calls. PS4: "statresponse %zu", one mask.
	inline constexpr uint32_t statresponse_parse_rvas[] = {0x0134D01D, 0x0134D032};
	inline constexpr uint32_t i_atoi64_rva = 0x0227C180;

	// SV_AddModifiedStats(clientNum) (PS4 0xF5D780), called first by SV_BeginClientSnapshot
	// 0x021FA650. svs.clients is the pointer at svs+0xC18 (PS4 svs+0xB98); PC client_t
	// stride 0xE5170 (PS4 0xD69F0). Offsets from the PC SV_ReceiveTransferData 0x021EB330:
	// statsDDLCtx +0xE0B80 (DDLContext {buff, len, def +0x10, ...}), statsModified +0xE5038.
	inline constexpr uint32_t sv_add_modified_stats_rva = 0x02206180;
	inline constexpr uint8_t sv_add_modified_stats_prologue[] = {0x4C, 0x8B, 0xDC, 0x55, 0x41, 0x54, 0x41, 0x57};
	inline constexpr uint32_t svs_clients_ptr_rva = 0x1767A398;
	inline constexpr size_t sv_client_stride = 0xE5170;
	inline constexpr size_t sv_client_stats_def = 0xE0B80 + 0x10;
	inline constexpr size_t sv_client_stats_modified = 0xE5038;

	// PS4 s_cachedStatsChanges cachedStats_t[4] x 0x1984 (0x0E3BF670); PC [2] x 0x4404.
	inline constexpr uint32_t statscache_rva = 0x1139B860;            // engine slots 0/1, count at +0x4400
	inline constexpr uint32_t statscache_stride = 0x4404;             // 0x100 x {u8[0x40]; int} + int count
	inline constexpr uint32_t set_stat_changed_rva = 0x01E994E0;      // LiveStats_SetStatChanged (PS4 0xC63D00)
	inline constexpr uint8_t set_stat_changed_prologue[] = {
		0x40, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, // push rbp, rsi, rdi, r12-r15
		0x48, 0x8D, 0x6C, 0x24, 0xD9,                                           // lea rbp, [rsp-0x27]
	};
	inline constexpr uint32_t set_stat_changed_internal_rva = 0x01E99850; // LiveStats_SetStatChangedInternal (PS4 0xC639B0)
	inline constexpr uint32_t com_decode_yenc_rva = 0x02245B90;       // Com_DecodeYEnc (PS4 0xFC1540)
	inline constexpr uint32_t com_error_rva = 0x020EB9F0;             // Com_Error (PS4 0xE47C30)
	inline constexpr uint32_t bb_register_hwm_rva = 0x02241CC0;       // BB_RegisterHighWaterMark (PS4 0xFBB850)
	inline constexpr uint32_t bb_set_hwm_rva = 0x02242070;            // BB_SetHighWaterMark (PS4 0xFBB9B0)
	inline constexpr uint32_t statscache_hwm_id_rva = 0x113A4068;     // SetStatChanged's static hwm id
	inline constexpr uint32_t statscache_hwm_guard_rva = 0x113A406C;  // its init guard, bit 0
	inline constexpr uint32_t str_statscache2_rva = 0x02F9A4B8;       // "statscache2"
	inline constexpr uint32_t str_statsoverflow_rva = 0x02F9A4A0;     // "EXE_PATCH_STATSOVERFLOW"
	inline constexpr uint32_t str_empty_rva = 0x02F1467C;             // ""
	// LiveStats_ResetCache (PS4 0xC63CD0) inlined in LiveStats_PreGame (0x01E94C70)
	inline constexpr uint32_t statscache_reset_rva = 0x01E94E8F;
	inline constexpr uint8_t statscache_reset_bytes[] = {
		0x48, 0x8D, 0x0D, 0xCA, 0x69, 0x50, 0x0F, // lea rcx, [statscache_rva]
		0x33, 0xD2,                               // xor edx, edx
		0x41, 0xB8, 0x08, 0x88, 0x00, 0x00,       // mov r8d, 0x8808
		0xE8, 0x4D, 0xFE, 0xD2, 0x00,             // call memset
	};
	inline constexpr uint32_t statscache_reset_call_rva = 0x01E94E9E;
	static_assert(statscache_reset_call_rva == statscache_reset_rva + 15);
	// 0x01E987A0 (PC-only statsHash command) flushes slot 0 in place: every slot-0 reference
	inline constexpr entcoll_site stats_hash_slot0_sites[] = {
		{0x01E9893F, 2, 6, true, 0x4400}, // mov edx, [slot0.count]
		{0x01E9894F, 2, 6, true, 0x4400}, // mov eax, [slot0.count]
		{0x01E98959, 3, 7, true, 0x0040}, // lea r14, [slot0.changes[0].size]
		{0x01E98960, 3, 7, true, 0x0000}, // lea rbp, [slot0]
		{0x01E989A2, 3, 7, true, 0x4400}, // mov r8d, [slot0.count]
		{0x01E989AD, 3, 7, true, 0x4400}, // mov [slot0.count], r15d
	};
}

namespace splitscreen::ezz
{
	// ======== splitscreen_ezz.hpp ========

	inline constexpr uint32_t get_xuid_rva = 0x01EBA880;          // LiveUser_GetXuid(ci)
	inline constexpr uint32_t user_get_xuid_rva = 0x01EBABC0;     // LiveUser_UserGetXuid(ci, xuid*)
	inline constexpr uint32_t get_client_name_rva = 0x01EBA850;   // LiveUser_GetClientName(ci)
	inline constexpr uint32_t get_user_data_rva = 0x01EBA3F0;     // LiveUser_GetUserDataForController(ci)

	namespace detail
	{
		struct array_alloc_site
		{
			uint32_t call_rva;
			uint32_t imul_rva;       // imul rdx/rdi, <elem> - proves what `size` counts
			uint8_t imul[7];
			size_t elem;
		};

		// CG_AllocateClientMemory (0x008408F0).
		inline constexpr array_alloc_site cg_alloc_sites[] = {
			{0x00840929, 0x00840922, {0x48, 0x69, 0xD2, 0x20, 0x27, 0x34, 0x00}, 0x342720},   // cgArray
			{0x008421C3, 0x008421AC, {0x48, 0x69, 0xD2, 0x40, 0xE9, 0x01, 0x00}, 0x1E940},    // cgsArray
			{0x00843A4F, 0x00843A2E, {0x48, 0x69, 0xFF, 0xA0, 0x03, 0x00, 0x00}, 0x3A0},      // cg_viewModelArray
		};

		// CG_InitAndAllocCGEntsArray (0x0085B990): the per-client entity pool allocation.
		inline constexpr uint32_t entity_call_rva = 0x0085B9F5;
		inline constexpr uint32_t entity_size_rva = 0x0085B9E7;
		inline constexpr uint8_t entity_size_bytes[] = {0xBA, 0x00, 0x00, 0x3F, 0x00};   // mov edx,0x3F0000
		inline constexpr size_t entity_pool_size = 0x3F0000;

		inline constexpr uint32_t client_command_rva = 0x0193DFC0;
		// mov [rsp+18h],rdi; push rbp; lea rbp,[rsp-340h] - after the host's jump
		inline constexpr uint8_t client_command_tail[] = {0x48, 0x89, 0x7C, 0x24, 0x18, 0x55, 0x48, 0x8D, 0xAC, 0x24,
		                                                  0xC0, 0xFC, 0xFF, 0xFF};
		// variants 0, 2, 4, 5, 6, 9, 11, 12, 14 (jump table at 0x019402E8)
		inline constexpr uint32_t client_command_range_tests[] = {
			0x0193E064, 0x0193E3F6, 0x0193E72E, 0x0193E8DC, 0x0193EA70,
			0x0193EFB7, 0x0193F38C, 0x0193F531, 0x0193F8F1,
		};
	}
}
