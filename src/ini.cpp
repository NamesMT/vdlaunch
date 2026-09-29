#include "ini.h"
#include "util.h"
#include <cstdio>

namespace vd {

const std::string* IniSection::get(const std::string& key) const {
  for (auto& e : entries)
    if (iequals(e.key, key)) return &e.value;
  return nullptr;
}

const IniSection* IniFile::section(const std::string& name) const {
  for (auto& s : sections)
    if (iequals(s.name, name)) return &s;
  return nullptr;
}

std::vector<const IniSection*> IniFile::sections_like(const std::string& pattern) const {
  std::vector<const IniSection*> out;
  for (auto& s : sections) {
    std::wstring p = wide(pattern), n = wide(s.name);
    if (glob_match(p, n)) out.push_back(&s);
  }
  return out;
}

const std::string* IniFile::value(const std::string& sec, const std::string& key) const {
  auto s = section(sec);
  return s ? s->get(key) : nullptr;
}

IniFile ini_parse(const std::string& text) {
  IniFile ini;
  IniSection* cur = nullptr;
  int line_no = 0;
  size_t pos = 0;

  while (pos <= text.size()) {
    size_t nl = text.find('\n', pos);
    std::string raw = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    pos = (nl == std::string::npos) ? text.size() + 1 : nl + 1;
    line_no++;

    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    std::string s = trim(raw);
    if (s.empty()) continue;
    if (s[0] == ';' || s[0] == '#') continue;

    if (s[0] == '[') {
      size_t close = s.find(']');
      if (close == std::string::npos) continue;
      ini.sections.push_back(IniSection{trim(s.substr(1, close - 1)), {}});
      cur = &ini.sections.back();
      continue;
    }

    size_t eq = s.find('=');
    if (eq == std::string::npos) continue;
    std::string key = trim(s.substr(0, eq));
    std::string val = trim(s.substr(eq + 1));
    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
    if (key.empty()) continue;
    if (!cur) {
      ini.sections.push_back(IniSection{"", {}});
      cur = &ini.sections.back();
    }
    cur->entries.push_back(IniEntry{key, val, line_no});
  }
  ini.loaded = true;
  return ini;
}

IniFile ini_parse_file(const std::string& path) {
  std::wstring wpath = wide(path);
  FILE* f = _wfopen(wpath.c_str(), L"rb");
  if (!f) return IniFile{};
  std::string data;
  char buf[8192];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
  fclose(f);
  if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF)
    data.erase(0, 3);
  return ini_parse(data);
}

}  // namespace vd
