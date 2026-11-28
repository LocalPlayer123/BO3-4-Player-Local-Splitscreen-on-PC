#include "component/splitscreen_names.hpp"
#include "regression_check.hpp"
#include <iostream>

int main()
{
	using namespace splitscreen::names;
	CHECK(sanitize("").empty());
	CHECK(sanitize("Player 1") == "Player 1");
	CHECK(sanitize(" a\";\\\r\n\t\x7f" "b ") == " ab ");
	CHECK(sanitize(std::string("a\0b", 3)) == "ab");
	CHECK(sanitize(std::string(40, 'a')) == std::string(31, 'a'));
	const std::string two = "\xC3\xA9", three = "\xE7\x8E\xA9", four = "\xF0\x9F\x8E\xAE";
	CHECK(sanitize(two + three + four) == two + three + four);
	for (const auto& cp : {two, three, four})
	{
		CHECK(sanitize(std::string(31 - cp.size(), 'a') + cp + "x") == std::string(31 - cp.size(), 'a') + cp);
		CHECK(sanitize(std::string(32 - cp.size(), 'a') + cp) == std::string(32 - cp.size(), 'a'));
		for (size_t n = 1; n < cp.size(); ++n) { CHECK(sanitize(cp.substr(0, n)).empty()); }
	}
	CHECK(sanitize("\xC0\xAF\xED\xA0\x80\xF4\x90\x80\x80\xFF\x80").empty());
	CHECK(sanitize("\xC2\x85\xE2\x80\xA8\xE2\x80\xA9").empty());
	std::array<match_seat, 4> seats = {{{true, 2, 7}, {true, 0, 11}, {true, 3, 4}, {true, 1, 2}}};
	CHECK(match_controller(seats, 7) == 2);
	CHECK(match_controller(seats, 11) == 0);
	CHECK(match_controller(seats, 4) == 3);
	CHECK(match_controller(seats, 2) == 1);
	CHECK(match_controller(seats, 38) == -1); // remote / bot
	CHECK(match_controller(seats, -1) == -1);
	CHECK(match_controller(seats, 64) == -1);
	seats[1].client = 7;
	CHECK(match_controller(seats, 7) == -1); // duplicate client
	seats[1].active = false;
	CHECK(match_controller(seats, 7) == 2);
	seats[2].controller = 2;
	CHECK(match_controller(seats, 7) == -1); // duplicate controller
	seats[2].active = false;
	seats[0].controller = 4;
	CHECK(match_controller(seats, 7) == -1);

	std::string formatted = "unchanged";
	CHECK(format_match_name("original(2)", "original(2)", "Guest", 32, formatted));
	CHECK(formatted == "Guest");
	CHECK(format_match_name("[CLAN]original(2)", "original(2)", "Guest", 40, formatted));
	CHECK(formatted == "[CLAN]Guest");
	CHECK(format_match_name("[TAG]old", "old", std::string(31, 'x'), 32, formatted));
	CHECK(formatted == "[TAG]" + std::string(26, 'x'));
	for (const auto& cp : {two, three, four})
	{
				CHECK(format_match_name("o", "o", "a" + cp + "z", cp.size() + 1, formatted));
		CHECK(formatted == "a");
				CHECK(format_match_name("o", "o", "a" + cp, cp.size() + 2, formatted));
		CHECK(formatted == "a" + cp);
	}
	for (const size_t size : {size_t(0), size_t(1), size_t(1025)})
	{
		formatted = "unchanged";
		CHECK(!format_match_name("old", "old", "Guest", size, formatted));
		CHECK(formatted == "unchanged");
	}
	CHECK(!format_match_name("old", "old", "", 32, formatted)); // reset
	CHECK(!format_match_name("", "", "Guest", 32, formatted));
	CHECK(!format_match_name("[TAG]truncated", "original", "Guest", 32, formatted));
	CHECK(!format_match_name("[TOOLONG]old", "old", "Guest", 8, formatted));
	const slots expected = {"Host", "", two + three, four};
	slots decoded = {"old", "old", "old", "old"};
	CHECK(decode(encode(expected), decoded));
	CHECK(decoded == expected);
	CHECK(decode(encode(slots{}), decoded));
	CHECK(decoded == slots{});
	for (const auto& text : {std::string{}, std::string("bad\n\n\n\n\n"),
		std::string(header) + "one\ntwo\nthree\n", encode(expected) + "extra\n",
		std::string(header) + "bad;command\n\n\n\n", std::string(header) + std::string(32, 'x') + "\n\n\n\n",
		std::string(header) + "\xC3\n\n\n\n"})
	{
		decoded = expected;
		CHECK(!decode(text, decoded));
		CHECK(decoded == expected); // malformed/partial loads are transactional
	}
	for (unsigned seed = 0; seed < 256; ++seed)
	{
		std::string input;
		for (unsigned i = 0; i < 128; ++i) { input += static_cast<char>((seed + i * 37) & 255); }
		const auto clean = sanitize(input);
		CHECK(clean.size() <= max_bytes);
		CHECK(sanitize(clean) == clean);
		const slots values = {clean, "", clean, ""};
		CHECK(decode(encode(values), decoded) && decoded == values);
	}
	std::cout << "PASS name UTF-8 sanitization and four-slot persistence codec\n";
}