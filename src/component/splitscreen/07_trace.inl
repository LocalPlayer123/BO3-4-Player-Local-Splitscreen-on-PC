// Trace file sink for note() (diagnostic build only writes it) and a call-site check.
// Part of splitscreen.cpp, included in order inside namespace splitscreen::{anon}.

		// ---- Trace file (diagnostic build only) ----
		// Appends a line to %LOCALAPPDATA%\boiii\splitscreen_ui_trace.txt. Plain Win32 file
		// calls, no CRT (see note()).
		struct trace_line
		{
			char b[1536];
			size_t n = 0;

			void str(const char* s)
			{
				while (s && *s && n < sizeof(b) - 3)
				{
					b[n++] = *s++;
				}
			}
		};

		// Diagnostic build only; the player build writes no files.
		void trace_write(trace_line& l)
		{
#ifndef SS_DIAG
			(void)l;
#else
			l.b[l.n++] = '\r';
			l.b[l.n++] = '\n';
			char path[MAX_PATH]{};
			const auto len = GetEnvironmentVariableA("LOCALAPPDATA", path, MAX_PATH);
			constexpr char leaf[] = "\\boiii\\splitscreen_ui_trace.txt";
			if (len == 0 || len + sizeof(leaf) > MAX_PATH)
			{
				return;
			}
			std::memcpy(path + len, leaf, sizeof(leaf));
			const auto file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
			                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE)
			{
				return;
			}
			DWORD written{};
			WriteFile(file, l.b, static_cast<DWORD>(l.n), &written, nullptr);
			CloseHandle(file);
#endif
		}

		void trace_text(const char* text)
		{
			trace_line l;
			l.str(text);
			trace_write(l);
		}

		// True if `site` is a rel32 call to `target`.
		bool call_site_targets(const uint32_t site, const uint32_t target)
		{
			const auto* p = reinterpret_cast<const uint8_t*>(base() + site);
			if (!readable(p, 5) || p[0] != 0xE8)
			{
				return false;
			}
			int32_t rel{};
			std::memcpy(&rel, p + 1, sizeof(rel));
			return site + 5 + static_cast<int64_t>(rel) == target;
		}
