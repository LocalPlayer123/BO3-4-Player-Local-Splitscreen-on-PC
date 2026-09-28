#pragma once

#include <Windows.h>

namespace release_policy
{
	// Stands in for GetEnvironmentVariableA inside the component. Answers only
	// the switches of the tested configuration; every other name reads as unset.
	DWORD get_environment_variable(LPCSTR name, LPSTR buffer, DWORD size);

	// Stands in for CreateFileA inside the component. Always fails, so the
	// release never creates or writes a file.
	HANDLE create_file(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
	                   DWORD disposition, DWORD flags, HANDLE template_file);
}
