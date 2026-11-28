#pragma once

// Portable protocol/storage codec. This file never executes config text.
#include <array>
#include <algorithm>
#include <string>
#include <string_view>

namespace splitscreen::names
{
	using slots = std::array<std::string, 4>;
	inline constexpr size_t max_bytes = 31;
	inline constexpr std::string_view header = "splitscreen_names_v1\n";

	struct match_seat
	{
		bool active = false;
		int controller = -1;
		int client = -1;
	};

	inline int match_controller(const std::array<match_seat, 4>& seats, const int client)
	{
		if (client < 0 || client >= 64) { return -1; }
		int result = -1;
		for (const auto& seat : seats)
		{
			if (!seat.active || seat.client != client) { continue; }
			if (seat.controller < 0 || seat.controller >= 4 || result != -1) { return -1; }
			result = seat.controller;
		}
		if (result >= 0)
		{
			size_t owners = 0;
			for (const auto& seat : seats)
			{
				if (seat.active && seat.controller == result) { ++owners; }
			}
			if (owners != 1) { return -1; }
		}
		return result;
	}

	// Replace only the stock terminal name, retaining its cleaned clan prefix.
	inline bool format_match_name(const std::string_view stock, const std::string_view plain,
		const std::string_view replacement, const size_t capacity, std::string& result)
	{
		if (capacity < 2 || capacity > 1024 || replacement.empty() || plain.empty()
			|| stock.size() >= capacity || stock.size() < plain.size()
			|| stock.substr(stock.size() - plain.size()) != plain) { return false; }
		const auto prefix = stock.substr(0, stock.size() - plain.size());
		if (prefix.size() >= capacity - 1) { return false; }
		size_t count = std::min(replacement.size(), capacity - 1 - prefix.size());
		// Native snapshots contain sanitized UTF-8; do not cut a codepoint.
		while (count < replacement.size() && count
			&& (static_cast<unsigned char>(replacement[count]) & 0xC0) == 0x80) { --count; }
		if (!count) { return false; }
		result = std::string(prefix) + std::string(replacement.substr(0, count));
		return true;
	}

	inline std::string sanitize(const std::string_view input)
	{
		std::string result;
		for (size_t i = 0; i < input.size();)
		{
			const auto c = static_cast<unsigned char>(input[i]);
			size_t n = 1;
			unsigned cp = c;
			if (c >= 0xC2 && c <= 0xDF) { n = 2; cp = c & 0x1F; }
			else if (c >= 0xE0 && c <= 0xEF) { n = 3; cp = c & 0x0F; }
			else if (c >= 0xF0 && c <= 0xF4) { n = 4; cp = c & 7; }
			else if (c >= 0x80) { ++i; continue; }
			bool valid = i + n <= input.size();
			for (size_t j = 1; valid && j < n; ++j)
			{
				const auto next = static_cast<unsigned char>(input[i + j]);
				valid = (next & 0xC0) == 0x80;
				cp = (cp << 6) | (next & 0x3F);
			}
			valid = valid && !(n == 2 && cp < 0x80) && !(n == 3 && cp < 0x800)
				&& !(n == 4 && cp < 0x10000) && cp <= 0x10FFFF
				&& !(cp >= 0xD800 && cp <= 0xDFFF);
			if (!valid) { ++i; continue; }
			if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F) || cp == '"' || cp == ';'
				|| cp == '\\' || cp == 0x2028 || cp == 0x2029)
			{
				i += n;
				continue;
			}
			if (result.size() + n > max_bytes) { break; }
			result.append(input.substr(i, n));
			i += n;
		}
		return result;
	}

	inline std::string encode(const slots& values)
	{
		std::string out(header);
		for (const auto& value : values) { out += sanitize(value); out += '\n'; }
		return out;
	}

	inline bool decode(std::string_view text, slots& output)
	{
		if (text.substr(0, header.size()) != header) { return false; }
		text.remove_prefix(header.size());
		slots candidate;
		for (auto& value : candidate)
		{
			const auto end = text.find('\n');
			if (end == std::string_view::npos) { return false; }
			const auto raw = text.substr(0, end);
			value = sanitize(raw);
			if (value != raw) { return false; }
			text.remove_prefix(end + 1);
		}
		if (!text.empty()) { return false; }
		output = candidate;
		return true;
	}
}