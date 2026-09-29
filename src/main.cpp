#include "config.h"
#include "desktop.h"
#include "switches.h"
#include "launch.h"
#include "util.h"
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <string>

using namespace vd;

static const char* kVersion = "0.1.0";

static void attach_parent_console() {
  if (GetConsoleWindow()) return;
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
  }
}

static void print_usage() {
  attach_parent_console();
  fprintf(stderr, "vdlaunch %s - launch a program on a Windows virtual desktop\n", kVersion);
  fprintf(stderr, "The target app and its desktop are read from vdlaunch.ini beside this exe,\n");
  fprintf(stderr, "or from _%s in the same folder when no config exists.\n", "thisname.exe");
  fprintf(stderr, "All arguments are passed through to the target.\n\n");
  fprintf(stderr, "Launcher switches (recognised only in the leading position):\n");
  fprintf(stderr, "  --print-config   report the resolved configuration and exit\n");
  fprintf(stderr, "  --diag           report COM / virtual-desktop diagnostics\n");
  fprintf(stderr, "  --ini <path>     use another ini file\n");
  fprintf(stderr, "  --log            open the log console\n");
  fprintf(stderr, "  --version, --help\n");
}

int main() {
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::wstring raw = GetCommandLineW();

  Cli cli = parse_cli(argc, argv, raw);
  if (cli.blocked) {
    attach_parent_console();
    fprintf(stderr, "vdlaunch: a switch is missing its value\n");
    return 2;
  }
  if (cli.help) { print_usage(); return 0; }
  if (cli.version) { attach_parent_console(); printf("vdlaunch %s\n", kVersion); return 0; }

  if (!cli.ini.empty()) set_ini_override(cli.ini);
  set_tail_override(cli.tail);
  log_open(path_join(exe_dir(), L"vdlaunch.log"));

  Config cfg = config_load(raw);
  if (!cli.target.empty()) {
    cfg.target = path_abs(cli.target);
    cfg.target_raw = utf8(cli.target);
    cfg.auto_target = false;
  }
  if (cfg.log || cli.log || cli.print_config || cli.diag) {
    logf("--- vdlaunch %s pid=%lu arch=%s ---", kVersion, (unsigned long)GetCurrentProcessId(),
#ifdef _WIN64
         "x64"
#else
         "x86"
#endif
    );
    log_line("config: " + config_summary(cfg));
  }

  if (cli.cleanup_keep > 0) {
    attach_parent_console();
    if (!vd::init()) { printf("no virtual desktop manager\n"); return 1; }
    int total = vd::count();
    if (total <= cli.cleanup_keep) { printf("nothing to clean up (%d desktop(s))\n", total); vd::shutdown(); return 0; }
    vd::switch_desktop(1);
    int removed = 0;
    while (vd::count() > cli.cleanup_keep) {
      int n = vd::count();
      if (!vd::remove_desktop(n, 1)) break;
      removed++;
    }
    printf("removed %d desktop(s); %d remain\n", removed, vd::count());
    vd::shutdown();
    return 0;
  }

  if (cli.print_config) {
    attach_parent_console();
    printf("vdlaunch %s\n", kVersion);
    printf("launcher   : %s\n", utf8(exe_path()).c_str());
    printf("ini        : %s%s\n", cfg.ini_path.c_str(), cfg.loaded ? "" : "  (not found)");
    printf("rule       : %s\n", cfg.matched_rule.empty() ? "<none>" : cfg.matched_rule.c_str());
    printf("target     : %s%s\n", utf8(cfg.target).c_str(), file_exists(cfg.target) ? "" : "  (MISSING)");
    printf("desktop    : %s\n", cfg.has_desktop ? cfg.desktop.c_str() : "<none>");
    printf("mode       : %s\n", cfg.switch_to ? "switch to desktop" : "background (move window)");
    printf("create     : %s\n", cfg.create ? "yes" : "no");
    printf("cwd        : %s\n", cfg.cwd == CwdMode::Custom ? utf8(cfg.cwd_custom).c_str()
                              : cfg.cwd == CwdMode::Inherit ? "(inherit)" : utf8(path_dir(cfg.target)).c_str());
    printf("dry run    : nothing was launched\n");
    return 0;
  }

  if (cli.diag) {
    attach_parent_console();
    printf("vdlaunch %s diagnostics\n", kVersion);
    printf("launcher      : %s\n", utf8(exe_path()).c_str());
    bool ok = vd::init();
    printf("virtual desktops: %s\n", ok ? "available" : "UNAVAILABLE");
    if (ok) {
      printf("desktop count : %d\n", vd::count());
      printf("current       : %d\n", vd::current_index());
      for (auto& d : vd::list()) printf("  desktop %d : %s\n", d.index, utf8(d.id).c_str());
    }
    vd::shutdown();
    return ok ? 0 : 1;
  }

  if (!cfg.quiet && !cfg.has_desktop && !file_exists(cfg.target)) {
    // Nothing configured anywhere: say so instead of failing silently.
    MessageBoxW(nullptr,
                (L"vdlaunch could not work out which program to start.\n\nExpected an ini at:\n" +
                 wide(cfg.ini_path) + L"\nor a target app at:\n" + cfg.target).c_str(),
                L"vdlaunch", MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);
    return 2;
  }

  int rc = run(cfg, raw);
  if (cli.log) {
    attach_parent_console();
    printf("vdlaunch: finished with exit code %d\n", rc);
  }
  return rc;
}
