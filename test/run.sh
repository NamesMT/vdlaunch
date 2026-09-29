#!/usr/bin/env bash
# vdlaunch test harness.
#
# Runs the Windows binaries from WSL through cmd.exe interop, covering argument
# passthrough, config resolution, and real virtual-desktop placement.
#
# Scenarios that launch a target use a small .bat wrapper so the environment is
# set without the trailing-space trap and so cmd waits for the launcher to end.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="/mnt/c/Users/Administrator/Desktop/vdtest"
WORK_WIN='C:\Users\Administrator\Desktop\vdtest'

PASS=0
FAIL=0
SKIP=0
FAILED_NAMES=()

red()   { printf '\033[31m%s\033[0m\n' "$*"; }
green() { printf '\033[32m%s\033[0m\n' "$*"; }
yellow(){ printf '\033[33m%s\033[0m\n' "$*"; }

ok()   { green "  PASS  $1"; PASS=$((PASS+1)); }
bad()  { red   "  FAIL  $1"; [ -n "${2:-}" ] && printf '        %s\n' "$2"; FAIL=$((FAIL+1)); FAILED_NAMES+=("$1"); }
skip() { yellow "  SKIP  $1  ($2)"; SKIP=$((SKIP+1)); }

check() {
  if [ "$2" = "$3" ]; then ok "$1"; else bad "$1" "expected [$3] got [$2]"; fi
}
contains() {
  if printf '%s' "$2" | grep -qF -- "$3"; then ok "$1"; else bad "$1" "missing [$3] in: $(printf '%s' "$2" | head -c 300)"; fi
}

fresh() {
  rm -rf "$WORK_DIR/$1"
  mkdir -p "$WORK_DIR/$1"
}

# write_runner <dir> <exe> <test-out> [args...]
write_runner() {
  local dir="$1" exe="$2" out="$3"; shift 3
  {
    printf '@echo off\r\n'
    printf 'set "VDLAUNCH_TEST_OUT=%s"\r\n' "$out"
    if [ -n "${TESTGUI:-}" ]; then printf 'set "VDLAUNCH_TEST_GUI=1"\r\n'; fi
    if [ -n "${TESTTAG:-}" ]; then printf 'set "VDLAUNCH_TEST_TAG=%s"\r\n' "$TESTTAG"; fi
    if [ -n "${TESTVAR:-}" ]; then printf 'set "VDLAUNCH_TEST_VAR=%s"\r\n' "$TESTVAR"; fi
    if [ -n "${TESTUNSET:-}" ]; then printf 'set "VDLAUNCH_TEST_UNSET=%s"\r\n' "$TESTUNSET"; fi
    printf '%s' "$exe"
    for a in "$@"; do printf ' %s' "$a"; done
    printf '\r\n'
    printf 'echo %%ERRORLEVEL%% > exitcode.txt\r\n'
  } > "$dir/run.bat"
}

# run_scenario <dir> -> echoes the launcher exit code
run_scenario() {
  local dir="$1"
  local sub="${dir#$WORK_DIR/}"
  rm -f "$dir/exitcode.txt"
  ( cd "$dir" && cmd.exe /c "cd /d $WORK_WIN\\$sub && run.bat" ) >/dev/null 2>&1
  tr -d '\r' < "$dir/exitcode.txt" 2>/dev/null || echo "?"
}

winpath() { local p="$1"; printf '%s' "$WORK_WIN\\${p//\//\\}"; }

diag() { ( cd "$WORK_DIR" && cmd.exe /c "cd /d $WORK_WIN && vdlaunch64c.exe --diag" 2>&1 | tr -d '\r' ); }
diag_count()   { diag | sed -n 's/^desktop count : //p' | tr -d ' '; }
diag_current() { diag | sed -n 's/^current       : //p' | tr -d ' '; }

# ---------------------------------------------------------------- setup
[ -d "$WORK_DIR" ] || mkdir -p "$WORK_DIR"

if [ ! -x "$ROOT/dist-test/vdlaunch64c.exe" ]; then
  echo "building test binaries..." >&2
  (cd "$ROOT" && make console) || { red "build failed"; exit 1; }
fi
if [ ! -f /tmp/mt/mock64.exe ]; then
  ( cd "$ROOT/test" && \
    x86_64-w64-mingw32-gcc -std=c11 -O2 -o /tmp/mt/mock64.exe mock.c -lole32 -loleaut32 -luuid && \
    i686-w64-mingw32-gcc   -std=c11 -O2 -o /tmp/mt/mock32.exe mock.c -lole32 -loleaut32 -luuid ) \
    || { red "mock build failed"; exit 1; }
fi

cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/"
cp "$ROOT/dist-test/vdlaunch32c.exe" "$WORK_DIR/"
mkdir -p "$WORK_DIR/bin64" "$WORK_DIR/bin32"
cp /tmp/mt/mock64.exe "$WORK_DIR/bin64/mock.exe"
cp /tmp/mt/mock32.exe "$WORK_DIR/bin32/mock.exe"

echo "vdlaunch test harness ($WORK_DIR)"
echo

# ---------------------------------------------------------------- 1
echo "[1] zero-config: launcher renamed over an existing app"
fresh s1
cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s1/prog.exe"
cp "$WORK_DIR/bin64/mock.exe"        "$WORK_DIR/s1/_prog.exe"
TESTTAG=s1 write_runner "$WORK_DIR/s1" prog.exe "$(winpath 's1/o.txt')" alpha beta
run_scenario "$WORK_DIR/s1" >/dev/null
OUT=$(tr -d '\r' < "$WORK_DIR/s1/o.txt" 2>/dev/null)
contains "1.1 _name.exe convention resolved" "$OUT" "=== s1 ==="
contains "1.2 first arg passed through"       "$OUT" "arg1=[alpha]"
contains "1.3 second arg passed through"      "$OUT" "arg2=[beta]"
contains "1.4 no ini required"                "$OUT" "argc=3"
contains "1.5 argv[0] is the real target"     "$OUT" "_prog.exe"

# ---------------------------------------------------------------- 2
echo
echo "[2] passthrough of awkward arguments"
fresh s2
cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s2/launcher.exe"
cp "$WORK_DIR/bin64/mock.exe"        "$WORK_DIR/s2/target.exe"
cat > "$WORK_DIR/s2/vdlaunch.ini" <<'EOF'
[launch]
target = target.exe
desktop_off = 1
EOF
TESTTAG=s2 write_runner "$WORK_DIR/s2" launcher.exe "$(winpath 's2/o.txt')" \
  '"a b"' '"c=d"' "--flag=value" /slash '"quote\"inside"' '日本語' '"--name=trailing space"'
run_scenario "$WORK_DIR/s2" >/dev/null
OUT=$(tr -d '\r' < "$WORK_DIR/s2/o.txt" 2>/dev/null)
contains "2.1 spaces preserved"     "$OUT" 'arg1=[a b]'
contains "2.2 equals preserved"     "$OUT" 'arg2=[c=d]'
contains "2.3 flag preserved"       "$OUT" 'arg3=[--flag=value]'
contains "2.4 slash preserved"      "$OUT" 'arg4=[/slash]'
contains "2.5 embedded quote kept"  "$OUT" 'quote\"inside'
contains "2.6 utf-8 preserved"      "$OUT" '日本語'
contains "2.7 inner spaces kept"    "$OUT" 'arg7=[--name=trailing space]'

# ---------------------------------------------------------------- 3
echo
echo "[3] app rules, env overrides and cwd"
fresh s3
cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s3/launcher.exe"
cp "$WORK_DIR/bin64/mock.exe"        "$WORK_DIR/s3/target.exe"
cat > "$WORK_DIR/s3/vdlaunch.ini" <<'EOF'
[launch]
target = target.exe
desktop_off = 1
env.VDLAUNCH_TEST_VAR = from-ini
env.un.VDLAUNCH_TEST_UNSET = 1
cwd = %EXE_DIR%

[apps.another*]
env.VDLAUNCH_TEST_VAR = from-rule
EOF
TESTTAG=s3a TESTVAR=caller-var TESTUNSET=should-be-removed write_runner "$WORK_DIR/s3" launcher.exe "$(winpath 's3/o.txt')" neutral
run_scenario "$WORK_DIR/s3" >/dev/null
OUT=$(tr -d '\r' < "$WORK_DIR/s3/o.txt" 2>/dev/null)
contains "3.1 ini env overrides caller"   "$OUT" "env.TESTVAR=from-ini"
contains "3.2 env.un removed a variable"  "$OUT" "env.UNSETME=<unset>"
contains "3.3 cwd from %EXE_DIR%"         "$OUT" "cwd=$(winpath s3)"

TESTTAG=s3b TESTVAR=caller-var write_runner "$WORK_DIR/s3" launcher.exe "$(winpath 's3/o2.txt')" another
run_scenario "$WORK_DIR/s3" >/dev/null
OUT=$(tr -d '\r' < "$WORK_DIR/s3/o2.txt" 2>/dev/null)
contains "3.4 later app rule wins"        "$OUT" "env.TESTVAR=from-rule"

# ---------------------------------------------------------------- 4
echo
echo "[4] cwd modes"
fresh s4
cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s4/launcher.exe"
mkdir -p "$WORK_DIR/s4/sub"
cp "$WORK_DIR/bin64/mock.exe" "$WORK_DIR/s4/sub/target.exe"
cat > "$WORK_DIR/s4/vdlaunch.ini" <<'EOF'
[launch]
target = sub\target.exe
desktop_off = 1
cwd = target
EOF
TESTTAG=s4 write_runner "$WORK_DIR/s4" launcher.exe "$(winpath 's4/o.txt')" x
run_scenario "$WORK_DIR/s4" >/dev/null
OUT=$(tr -d '\r' < "$WORK_DIR/s4/o.txt" 2>/dev/null)
contains "4.1 cwd=target uses the app folder" "$OUT" "cwd=$(winpath s4)\\sub"

cat > "$WORK_DIR/s4/vdlaunch.ini" <<'EOF'
[launch]
target = sub\target.exe
desktop_off = 1
cwd = inherit
EOF
TESTTAG=s4b write_runner "$WORK_DIR/s4" launcher.exe "$(winpath 's4/o2.txt')" x
run_scenario "$WORK_DIR/s4" >/dev/null
OUT=$(tr -d '\r' < "$WORK_DIR/s4/o2.txt" 2>/dev/null)
contains "4.2 cwd=inherit keeps caller folder" "$OUT" "cwd=$(winpath s4)"

# ---------------------------------------------------------------- 5
echo
echo "[5] args modes"
fresh s5
cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s5/launcher.exe"
cp "$WORK_DIR/bin64/mock.exe"        "$WORK_DIR/s5/target.exe"
cat > "$WORK_DIR/s5/vdlaunch.ini" <<'EOF'
[launch]
target = target.exe
desktop_off = 1
args = none
EOF
TESTTAG=s5 write_runner "$WORK_DIR/s5" launcher.exe "$(winpath 's5/o.txt')" passed-through
run_scenario "$WORK_DIR/s5" >/dev/null
OUT=$(tr -d '\r' < "$WORK_DIR/s5/o.txt" 2>/dev/null)
contains "5.1 args=none drops target args" "$OUT" "argc=1"

cat > "$WORK_DIR/s5/vdlaunch.ini" <<'EOF'
[launch]
target = target.exe
desktop_off = 1
args = --forced
EOF
TESTTAG=s5b write_runner "$WORK_DIR/s5" launcher.exe "$(winpath 's5/o2.txt')" ignored
run_scenario "$WORK_DIR/s5" >/dev/null
OUT=$(tr -d '\r' < "$WORK_DIR/s5/o2.txt" 2>/dev/null)
contains "5.2 args=<literal> replaces them" "$OUT" "arg1=[--forced]"

# ---------------------------------------------------------------- 6
echo
echo "[6] auto dispatcher against a 32-bit target"
fresh s6
# prog32.exe is the auto build renamed over the app; vdlaunch32.exe is the
# helper it needs, and _prog32.exe is the real 32-bit app.
cp "$ROOT/dist/vdlaunch.exe"         "$WORK_DIR/s6/prog32.exe"
cp "$ROOT/dist-test/vdlaunch32c.exe" "$WORK_DIR/s6/vdlaunch32.exe"
cp "$WORK_DIR/bin32/mock.exe"        "$WORK_DIR/s6/_prog32.exe"
cat > "$WORK_DIR/s6/run.bat" <<EOF
@echo off
set "VDLAUNCH_TEST_OUT=$(winpath 's6/o.txt')"
set "VDLAUNCH_TEST_TAG=s6"
start /wait "" prog32.exe hello world
echo %ERRORLEVEL% > exitcode.txt
EOF
run_scenario "$WORK_DIR/s6" >/dev/null
sleep 1
OUT=$(tr -d '\r' < "$WORK_DIR/s6/o.txt" 2>/dev/null)
if [ -z "$OUT" ]; then
  skip "6.x 32-bit auto dispatch" "no output captured"
else
  contains "6.1 32-bit target reached via dispatcher" "$OUT" "=== s6 ==="
  contains "6.2 args survive dispatch"                "$OUT" "arg1=[hello]"
  contains "6.3 second arg survives dispatch"         "$OUT" "arg2=[world]"
fi

# ---------------------------------------------------------------- 7
echo
echo "[7] error handling"
fresh s7
cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s7/launcher.exe"
cat > "$WORK_DIR/s7/vdlaunch.ini" <<'EOF'
[launch]
target = does-not-exist.exe
quiet = true
EOF
TESTTAG=s7 write_runner "$WORK_DIR/s7" launcher.exe "$(winpath 's7/o.txt')"
RC=$(run_scenario "$WORK_DIR/s7")
check    "7.1 missing target exits non-zero" "$([ "${RC:-0}" -ne 0 ] && echo yes || echo no)" "yes"
contains "7.2 missing target is logged" "$(tr -d '\r' < "$WORK_DIR/s7/vdlaunch.log" 2>/dev/null)" "target not found"

fresh s8
cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s8/launcher.exe"
cat > "$WORK_DIR/s8/vdlaunch.ini" <<'EOF'
[launch]
target = launcher.exe
quiet = true
EOF
TESTTAG=s8 write_runner "$WORK_DIR/s8" launcher.exe "$(winpath 's8/o.txt')"
RC=$(run_scenario "$WORK_DIR/s8")
check    "7.3 self-target refused" "$([ "${RC:-0}" -ne 0 ] && echo yes || echo no)" "yes"
contains "7.4 self-target logged" "$(tr -d '\r' < "$WORK_DIR/s8/vdlaunch.log" 2>/dev/null)" "launcher itself"

# ---------------------------------------------------------------- 8
echo
echo "[8] virtual desktop placement (needs a real desktop session)"
COUNTV=$(diag_count)
CURRENT=$(diag_current)
if [ -z "${COUNTV:-}" ] || [ "${COUNTV:-0}" -lt 1 ]; then
  skip "8.x desktop placement" "no manager"
else
  echo "  (host: $COUNTV desktop(s), currently on $CURRENT)"

  fresh s8a
  cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s8a/launcher.exe"
  cp "$WORK_DIR/bin64/mock.exe"        "$WORK_DIR/s8a/target.exe"
  cat > "$WORK_DIR/s8a/vdlaunch.ini" <<'EOF'
[launch]
target = target.exe
desktop = current
switch = false
EOF
  TESTTAG=s8a TESTGUI=1 write_runner "$WORK_DIR/s8a" launcher.exe "$(winpath 's8a/o.txt')"
  run_scenario "$WORK_DIR/s8a" >/dev/null
  OUT=$(tr -d '\r' < "$WORK_DIR/s8a/o.txt" 2>/dev/null)
  contains "8a.1 GUI target launched"             "$OUT" "=== s8a ==="
  contains "8a.2 window on the current desktop"   "$OUT" "onCurrentDesktop=1"
  check    "8a.3 launch did not switch desktop"   "$(diag_current)" "$CURRENT"

  fresh s8b
  cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s8b/launcher.exe"
  cp "$WORK_DIR/bin64/mock.exe"        "$WORK_DIR/s8b/target.exe"
  cat > "$WORK_DIR/s8b/vdlaunch.ini" <<'EOF'
[launch]
target = target.exe
desktop = new
switch = false
EOF
  TESTTAG=s8b TESTGUI=1 write_runner "$WORK_DIR/s8b" launcher.exe "$(winpath 's8b/o.txt')"
  run_scenario "$WORK_DIR/s8b" >/dev/null
  OUT=$(tr -d '\r' < "$WORK_DIR/s8b/o.txt" 2>/dev/null)
  NEWCOUNT=$(diag_count)
  contains "8b.1 GUI target launched"                 "$OUT" "=== s8b ==="
  check    "8b.2 a new desktop was created"           "${NEWCOUNT:-x}" "$((COUNTV+1))"
  check    "8b.3 launch stayed in the background"     "$(diag_current)" "$CURRENT"
  contains "8b.4 window moved off the current desktop" "$OUT" "onCurrentDesktop=0"

  fresh s8c
  cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s8c/launcher.exe"
  cp "$WORK_DIR/bin64/mock.exe"        "$WORK_DIR/s8c/target.exe"
  WANT=$(( ${NEWCOUNT:-$COUNTV} + 2 ))
  cat > "$WORK_DIR/s8c/vdlaunch.ini" <<EOF
[launch]
target = target.exe
desktop = $WANT
create = true
switch = false
EOF
  TESTTAG=s8c TESTGUI=1 write_runner "$WORK_DIR/s8c" launcher.exe "$(winpath 's8c/o.txt')"
  run_scenario "$WORK_DIR/s8c" >/dev/null
  OUT=$(tr -d '\r' < "$WORK_DIR/s8c/o.txt" 2>/dev/null)
  check    "8c.1 missing index auto-created"    "$(diag_count)" "$WANT"
  contains "8c.2 window on the created desktop" "$OUT" "onCurrentDesktop=0"

  fresh s8d
  cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s8d/launcher.exe"
  cp "$WORK_DIR/bin64/mock.exe"        "$WORK_DIR/s8d/target.exe"
  cat > "$WORK_DIR/s8d/vdlaunch.ini" <<'EOF'
[launch]
target = target.exe
desktop = 77
create = false
switch = false
quiet = true
EOF
  TESTTAG=s8d write_runner "$WORK_DIR/s8d" launcher.exe "$(winpath 's8d/o.txt')"
  run_scenario "$WORK_DIR/s8d" >/dev/null
  OUT=$(tr -d '\r' < "$WORK_DIR/s8d/o.txt" 2>/dev/null)
  check    "8d.1 create=false added no desktop" "$(diag_count)" "$WANT"
  contains "8d.2 app still launched anyway"     "$OUT" "=== s8d ==="

  fresh s8e
  cp "$ROOT/dist-test/vdlaunch64c.exe" "$WORK_DIR/s8e/launcher.exe"
  cp "$WORK_DIR/bin64/mock.exe"        "$WORK_DIR/s8e/target.exe"
  TARGETIDX=$(( ${NEWCOUNT:-$COUNTV} + 1 ))
  cat > "$WORK_DIR/s8e/vdlaunch.ini" <<EOF
[launch]
target = target.exe
desktop = $TARGETIDX
switch = true
EOF
  TESTTAG=s8e write_runner "$WORK_DIR/s8e" launcher.exe "$(winpath 's8e/o.txt')"
  run_scenario "$WORK_DIR/s8e" >/dev/null
  check "8e.1 switch=true moved the visible desktop" "$(diag_current)" "$TARGETIDX"

  cat > "$WORK_DIR/restore.ini" <<EOF
[launch]
target = bin64\mock.exe
desktop = $CURRENT
switch = true
EOF
  ( cd "$WORK_DIR" && cmd.exe /c "cd /d $WORK_WIN && vdlaunch64c.exe --ini $WORK_WIN\\restore.ini" ) >/dev/null 2>&1
  sleep 1
  check "8e.2 restored the original desktop" "$(diag_current)" "$CURRENT"

  # Undo the desktops this suite created so the host is left as we found it.
  ( cd "$WORK_DIR" && cmd.exe /c "cd /d $WORK_WIN && vdlaunch64c.exe --cleanup-desktops $COUNTV" ) >/dev/null 2>&1
  sleep 1
  check "8f.1 desktop count restored" "$(diag_count)" "$COUNTV"
fi

# ---------------------------------------------------------------- summary
echo
echo "----------------------------------------"
printf 'passed %d, failed %d, skipped %d\n' "$PASS" "$FAIL" "$SKIP"
if [ "$FAIL" -ne 0 ]; then
  echo "failed checks:"
  for n in "${FAILED_NAMES[@]}"; do echo "  - $n"; done
  exit 1
fi
