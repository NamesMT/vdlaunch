#include "switches.h"
#include "util.h"

namespace vd {

// Command-line tokenizer mirroring how the CRT parses a command line: quotes
// open/close quoting, "" inside a quoted run is a literal quote, and backslash
// escapes a following quote.
static size_t skip_ws(const std::wstring& s, size_t p) {
  while (p < s.size() && (s[p] == L' ' || s[p] == L'\t')) p++;
  return p;
}

static size_t token_end(const std::wstring& s, size_t p) {
  p = skip_ws(s, p);
  bool quoted = false;
  while (p < s.size()) {
    wchar_t c = s[p];
    if (c == L'\\' && p + 1 < s.size() && s[p + 1] == L'"') { p += 2; continue; }
    if (c == L'"') {
      if (quoted && p + 1 < s.size() && s[p + 1] == L'"') { p += 2; continue; }
      quoted = !quoted;
      p++;
      continue;
    }
    if (!quoted && (c == L' ' || c == L'\t')) break;
    p++;
  }
  return p;
}

Cli parse_cli(int argc, LPWSTR* argv, const std::wstring& raw) {
  Cli cli;
  if (!argv || argc < 1) {
    cli.tail = raw;
    return cli;
  }

  // Walk the raw string in lockstep with argv so we know exactly where the
  // wrapped app's arguments begin. Our switches only count in leading position.
  size_t start = 0;   // first character of argv[i]
  bool   tail_set = false;

  for (int i = 0; i < argc; i++) {
    start = skip_ws(raw, start);
    size_t end = token_end(raw, start);
    bool consumed = true;

    if (i > 0) {
      std::wstring a = argv[i];
      if (iequalsw(a, L"--print-config"))   cli.print_config = true;
      else if (iequalsw(a, L"--diag"))      cli.diag = true;
      else if (iequalsw(a, L"--log"))       cli.log = true;
      else if (iequalsw(a, L"--version"))   cli.version = true;
      else if (iequalsw(a, L"--help") || iequalsw(a, L"-h")) cli.help = true;
      else if (iequalsw(a, L"--cleanup-desktops")) {
        cli.cleanup_keep = 1;
        if (i + 1 < argc && iswdigit(argv[i + 1][0])) cli.cleanup_keep = _wtoi(argv[++i]);
      }
      else if (iequalsw(a, L"--ini") || iequalsw(a, L"--target")) {
        if (i + 1 >= argc) { cli.blocked = true; break; }
        std::wstring value = argv[++i];
        if (iequalsw(a, L"--ini")) cli.ini = value;
        else cli.target = value;
        start = token_end(raw, end);   // also skip its value token
        continue;
      }
      else consumed = false;
    }

    if (!consumed) { tail_set = true; break; }
    start = end;
  }

  if (!tail_set) start = raw.size();
  start = skip_ws(raw, start);
  cli.tail = start < raw.size() ? raw.substr(start) : L"";
  return cli;
}

}  // namespace vd
