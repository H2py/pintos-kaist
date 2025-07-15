#!/usr/bin/env bash

# Usage: select_test.sh [-q|-g] [-r]
#   -q|-g : ì‹¤í–‰ ëª¨ë“œ ì§€ì •
#   -r    : clean & rebuild
if (( $# < 1 || $# > 2 )); then
  echo "Usage: $0 [-q|-g] [-r]"
  echo "  -q   : run tests quietly (no GDB stub)"
  echo "  -g   : attach via GDB stub (skip build)"
  echo "  -r   : force clean & full rebuild"
  exit 1
fi

MODE="$1"
if [[ "$MODE" != "-q" && "$MODE" != "-g" ]]; then
  echo "Usage: $0 [-q|-g] [-r]"
  exit 1
fi

# ë‘ ë²ˆì§¸ ì¸ìžê°€ ìžˆìœ¼ë©´ -r ì²´í¬
REBUILD=0
if (( $# == 2 )); then
  if [[ "$2" == "-r" ]]; then
    REBUILD=1
  else
    echo "Unknown option: $2"
    echo "Usage: $0 [-q|-g] [-r]"
    exit 1
  fi
fi

# ìŠ¤í¬ë¦½íŠ¸ ìžì‹ ì´ ìžˆëŠ” ë””ë ‰í„°ë¦¬ (src/threads/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# í”„ë¡œì íŠ¸ ë£¨íŠ¸ì—ì„œ Pintos í™˜ê²½ í™œì„±í™”
source "${SCRIPT_DIR}/../activate"

# 1) build/ í´ë”ê°€ ì—†ìœ¼ë©´ ë¬´ì¡°ê±´ ì²˜ìŒ ë¹Œë“œ
if [[ ! -d "${SCRIPT_DIR}/build" ]]; then
  echo "Build directory not found. Building Pintos threads..."
  make -C "${SCRIPT_DIR}" clean all
fi

# 2) -r ì˜µì…˜ì´ ìžˆìœ¼ë©´ clean & rebuild
if (( REBUILD )); then
  echo "Force rebuilding Pintos threads..."
  make -C "${SCRIPT_DIR}" clean all
fi

STATE_FILE="${SCRIPT_DIR}/.test_status"
declare -A status_map

# íŒŒì¼ì´ ìžˆìœ¼ë©´ í•œ ì¤„ì”© ì½ì–´ ë„£ê¸°
if [[ -f "$STATE_FILE" ]]; then
  while read -r test stat; do
    status_map["$test"]="$stat"
  done < "$STATE_FILE"
fi

# ê°€ëŠ¥í•œ Pintos í…ŒìŠ¤íŠ¸ ëª©ë¡
tests=(
  alarm-single
  alarm-multiple
  alarm-simultaneous
  alarm-priority
  alarm-zero
  alarm-negative
  priority-change
  priority-donate-one
  priority-donate-multiple
  priority-donate-multiple2
  priority-donate-nest
  priority-donate-sema
  priority-donate-lower
  priority-fifo
  priority-preempt
  priority-sema
  priority-condvar
  priority-donate-chain
  mlfqs-load-1
  mlfqs-load-60
  mlfqs-load-avg
  mlfqs-recent-1
  mlfqs-fair-2
  mlfqs-fair-20
  mlfqs-nice-2
  mlfqs-nice-10
  mlfqs-block
)

echo "=== Available Pintos Tests ==="
for i in "${!tests[@]}"; do
  idx=$((i+1))
  test="${tests[i]}"
  stat="${status_map[$test]:-untested}"
  # ìƒ‰ ê²°ì •: PASS=ë…¹ìƒ‰, FAIL=ë¹¨ê°•, untested=ê¸°ë³¸
  case "$stat" in
    PASS) color="\e[32m" ;;
    FAIL) color="\e[31m" ;;
    *)    color="\e[0m"  ;;
  esac
  printf " ${color}%2d) %s\e[0m\n" "$idx" "$test"
done

# 2) ì‚¬ìš©ìž ì„ íƒ (ì—¬ëŸ¬ ê°œ / ë²”ìœ„ í—ˆìš©)
read -p "Enter test numbers (e.g. '1 3 5' or '2-4'): " input
tokens=()
for tok in ${input//,/ }; do
  if [[ "$tok" =~ ^([0-9]+)-([0-9]+)$ ]]; then
    for ((n=${BASH_REMATCH[1]}; n<=${BASH_REMATCH[2]}; n++)); do
      tokens+=("$n")
    done
  else
    tokens+=("$tok")
  fi
done

declare -A seen=()
sel_tests=()
for n in "${tokens[@]}"; do
  if [[ "$n" =~ ^[0-9]+$ ]] && (( n>=1 && n<=${#tests[@]} )); then
    idx=$((n-1))
    if [[ -z "${seen[$idx]}" ]]; then
      sel_tests+=("${tests[idx]}")
      seen[$idx]=1
    fi
  else
    echo "Invalid test number: $n" >&2
    exit 1
  fi
done

echo "Selected tests: ${sel_tests[*]}"

# 3) ìˆœì°¨ ì‹¤í–‰ ë° ê²°ê³¼ ì§‘ê³„
passed=()
failed=()
{
  cd "${SCRIPT_DIR}/build" || exit 1

  count=0
  total=${#sel_tests[@]}
  for test in "${sel_tests[@]}"; do
    echo
    if [[ "$MODE" == "-q" ]]; then
      # batch ëª¨ë“œ: make íƒ€ê²Ÿìœ¼ë¡œ .result ìƒì„±
      echo -n "Running ${test} in batch mode... "
      if make -s "tests/threads/${test}.result"; then
        if grep -q '^PASS' "tests/threads/${test}.result"; then
          echo "PASS"; passed+=("$test")
        else
          echo "FAIL"; failed+=("$test")
        fi
      else
        echo "ERROR"; failed+=("$test")
      fi
    else
      # interactive debug ëª¨ë“œ: QEMU í¬ê·¸ë¼ìš´ë“œ ì‹¤í–‰ + tee ë¡œ .output ìº¡ì²˜ í›„ .result ìƒì„±
      outdir="tests/threads"
      mkdir -p "${outdir}"

      echo -e "=== Debugging \e[33m${test}\e[0m ($(( count + 1 ))/${total}) ==="
      echo " * QEMU ì°½ì´ ëœ¨ê³ , gdb stubì€ localhost:1234 ì—ì„œ ëŒ€ê¸°í•©ë‹ˆë‹¤."
      echo " * ë‚´ë¶€ ì¶œë ¥ì€ í„°ë¯¸ë„ì— ë³´ì´ë©´ì„œ '${outdir}/${test}.output'ì—ë„ ì €ìž¥ë©ë‹ˆë‹¤."
      echo

      # í„°ë¯¸ë„ê³¼ íŒŒì¼ë¡œ ë™ì‹œì— ì¶œë ¥
      pintos --gdb -- -q run "${test}" 2>&1 | tee "${outdir}/${test}.output" 

      # ì¢…ë£Œ í›„ ì²´í¬ ìŠ¤í¬ë¦½íŠ¸ë¡œ .result ìƒì„±
      repo_root="${SCRIPT_DIR}/.."   # ë¦¬í¬ì§€í„°ë¦¬ ë£¨íŠ¸(pintos/) ê²½ë¡œ
      ck="${repo_root}/tests/threads/${test}.ck"
      if [[ -f "$ck" ]]; then
      # -I ë¡œ repo rootë¥¼ @INC ì— ì¶”ê°€í•´ì•¼ tests/tests.pm ì„ ì°¾ìŠµë‹ˆë‹¤.
      perl -I "${repo_root}" \
           "$ck" "${outdir}/${test}" "${outdir}/${test}.result"
        if grep -q '^PASS' "${outdir}/${test}.result"; then
          echo "=> PASS"; passed+=("$test")
        else
          echo "=> FAIL"; failed+=("$test")
        fi
      else
        echo "=> No .ck script, skipping result."; failed+=("$test")
      fi
      echo "=== ${test} session end ==="
    fi

    # ì§„í–‰ í˜„í™© í‘œì‹œ: ë…¸ëž€ìƒ‰ìœ¼ë¡œ total i/n ì¶œë ¥
    ((count++))
    echo -e "\e[33mtest ${count}/${total} finish\e[0m"
  done
}

# 4) ìš”ì•½ ì¶œë ¥
echo
echo "=== Test Summary ==="
echo "Passed: ${#passed[@]}"
for t in "${passed[@]}"; do echo "  - $t"; done
echo "Failed: ${#failed[@]}"
for t in "${failed[@]}"; do echo "  - $t"; done

# ì´ë²ˆì— PASS ëœ í…ŒìŠ¤íŠ¸ë§Œ ë®ì–´ì“°ê¸°
for t in "${passed[@]}"; do
  status_map["$t"]="PASS"
done

# ì´ë²ˆì— FAIL ëœ í…ŒìŠ¤íŠ¸ë§Œ ë®ì–´ì“°ê¸°
for t in "${failed[@]}"; do
  status_map["$t"]="FAIL"
done


# 5) ìƒíƒœ íŒŒì¼ì— PASS/FAIL ê¸°ë¡ (untestedëŠ” ê¸°ë¡í•˜ì§€ ì•ŠìŒ)
> "$STATE_FILE"
for test in "${!status_map[@]}"; do
  echo "$test ${status_map[$test]}"
done >| "$STATE_FILE"