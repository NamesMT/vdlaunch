#pragma once
#include <string>
#include <vector>

namespace vd {

struct IniEntry {
  std::string key;
  std::string value;
  int         line = 0;
};

struct IniSection {
  std::string           name;
  std::vector<IniEntry> entries;

  const std::string* get(const std::string& key) const;
};

struct IniFile {
  std::vector<IniSection> sections;
  bool loaded = false;

  const IniSection* section(const std::string& name) const;
  // All sections whose name matches `pattern` (e.g. "apps.*"), in file order.
  std::vector<const IniSection*> sections_like(const std::string& pattern) const;
  const std::string* value(const std::string& section, const std::string& key) const;
};

// Minimal INI reader: `[section]`, `key=value`, `;`/`#` comments, no escapes.
IniFile ini_parse_file(const std::string& path);
IniFile ini_parse(const std::string& text);

}  // namespace vd
