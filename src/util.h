#pragma once
#include <windows.h>
#include <string>
#include <vector>

std::string  utf8(const std::wstring& w);
std::wstring wide(const std::string& s);

std::wstring exe_path();
std::wstring exe_dir();
std::wstring exe_name();
std::wstring path_join(const std::wstring& a, const std::wstring& b);
std::wstring path_dir(const std::wstring& p);
std::wstring path_abs(const std::wstring& p);
bool file_exists(const std::wstring& p);
bool dir_exists(const std::wstring& p);

void log_line(const std::string& msg);
void log_line(const std::wstring& msg);
void log_open(const std::wstring& path);
void logf(const char* fmt, ...);
void logw(const wchar_t* fmt, ...);

std::string trim(const std::string& s);
std::string lower(std::string s);
bool iequals(const std::string& a, const std::string& b);
bool iequalsw(const std::wstring& a, const std::wstring& b);
std::vector<std::string> split(const std::string& s, char sep);
bool glob_match(const std::wstring& pattern, const std::wstring& text);
int  windows_build();
