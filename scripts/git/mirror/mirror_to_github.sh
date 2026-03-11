#!/usr/bin/env bash
# ALIS public mirror policy:
# - open-source, public, decentralized flow by default
# - code, docs, and scripts are mirrorable unless they contain concrete secret/licensing risk
# - secrets must stay outside the repository and outside the mirror
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

REMOTE_URL="${MIRROR_REMOTE_URL:-}"
BRANCH="main"
EXCLUDE_FILE="$SCRIPT_DIR/mirror.exclude"
FORBIDDEN_PATTERNS_FILE="${MIRROR_FORBIDDEN_PATTERNS_FILE:-$SCRIPT_DIR/forbidden_text_patterns.regex}"
ALLOW_DIRTY=0
DO_PUSH=0
EPHEMERAL_PREVIEW=0

usage() {
  cat <<'USAGE'
Usage:
  ./scripts/git/mirror/mirror_to_github.sh [options]

Options:
  --remote-url <url>                Target mirror remote URL.
                                    Optional for dry-run, required for --push.
                                    Override via env: MIRROR_REMOTE_URL
  --branch <name>                   Target branch name (default: main).
  --exclude-file <path>             Exclude rules file (default: scripts/git/mirror/mirror.exclude).
  --forbidden-patterns-file <path>  Regex file for hard-fail content validation.
                                    Default: scripts/git/mirror/forbidden_text_patterns.regex
  --push                            Push to remote.
  --dry-run                         Do not push. Compares against remote branch when remote exists.
  --ephemeral-preview               Allow local preview without a remote baseline.
  --force                           Allow running from a dirty source repository.
  --allow-dirty                     Backward-compatible alias for --force.
  -h, --help                        Show this help.

Examples:
  ./scripts/git/mirror/mirror_to_github.sh --remote-url git@github.com:org/repo.git --dry-run
  ./scripts/git/mirror/mirror_to_github.sh --dry-run --ephemeral-preview
  ./scripts/git/mirror/mirror_to_github.sh --remote-url git@github.com:org/repo.git --push
USAGE
}

info() {
  printf '[INFO] %s\n' "$*"
}

fail() {
  printf '[FAIL] %s\n' "$*" >&2
  exit 1
}

require_cmd() {
  local cmd="$1"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    fail "Required command not found: $cmd"
  fi
}

git_safe() {
  git -c core.fsmonitor=false -c core.hooksPath=/dev/null "$@"
}

resolve_file() {
  local input="$1"
  local repo_root="$2"
  if [[ -z "$input" ]]; then
    printf '\n'
    return 0
  fi
  if [[ "$input" = /* ]]; then
    printf '%s\n' "$input"
    return 0
  fi
  if [[ -f "$PWD/$input" ]]; then
    printf '%s\n' "$PWD/$input"
    return 0
  fi
  if [[ -f "$repo_root/$input" ]]; then
    printf '%s\n' "$repo_root/$input"
    return 0
  fi
  printf '%s\n' "$input"
}

build_filtered_snapshot() {
  local repo_root="$1"
  local exclude_file="$2"
  local filtered_dir="$3"
  local temp_root="$4"
  local tracked_paths_file="$temp_root/tracked_paths.nul"
  local allowed_paths_file="$temp_root/allowed_paths.nul"
  local head_index_file="$temp_root/head.index"

  info "Selecting tracked HEAD files that survive blacklist rules"
  git_safe -C "$repo_root" ls-tree -r --name-only -z HEAD > "$tracked_paths_file"

  python3 - "$exclude_file" "$tracked_paths_file" "$allowed_paths_file" <<'PY'
import sys
from fnmatch import fnmatchcase

exclude_file, tracked_paths_file, allowed_paths_file = sys.argv[1:4]
patterns = []

with open(exclude_file, "r", encoding="utf-8") as handle:
    for raw_line in handle:
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        line = line.replace("\\", "/")
        if line.endswith("/"):
            line += "**"
        patterns.append(line)

raw_paths = open(tracked_paths_file, "rb").read().split(b"\0")
allowed = []
for raw_path in raw_paths:
    if not raw_path:
        continue
    rel_path = raw_path.decode("utf-8", errors="surrogateescape").replace("\\", "/")
    if any(fnmatchcase(rel_path, pattern) for pattern in patterns):
        continue
    allowed.append(raw_path)

with open(allowed_paths_file, "wb") as handle:
    if allowed:
        handle.write(b"\0".join(allowed))
PY

  mkdir -p "$filtered_dir"
  : > "$head_index_file"
  GIT_INDEX_FILE="$head_index_file" git_safe -C "$repo_root" read-tree HEAD
  if [[ -s "$allowed_paths_file" ]]; then
    GIT_INDEX_FILE="$head_index_file" git_safe -C "$repo_root" checkout-index -q -z --stdin --prefix="$filtered_dir/" < "$allowed_paths_file"
  fi
}

sanitize_filtered_tree() {
  local filtered_dir="$1"

  python3 - "$filtered_dir" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
text_suffixes = {
    ".md",
    ".txt",
    ".json",
    ".ini",
    ".cs",
    ".cpp",
    ".c",
    ".h",
    ".hpp",
    ".inl",
    ".ps1",
    ".bat",
    ".sh",
    ".py",
    ".yml",
    ".yaml",
    ".dsl",
    ".uplugin",
    ".uproject",
    ".disabled",
}

text_replacements = [
    (re.compile(r"[A-Za-z]:\\Repos_Alis\\site"), r"<site-root>"),
    (re.compile(r"[A-Za-z]:/Repos_Alis/site"), r"<site-root>"),
    (re.compile(r"[A-Za-z]:\\Repos_Alis\\Alis"), r"<project-root>"),
    (re.compile(r"[A-Za-z]:/Repos_Alis/Alis"), r"<project-root>"),
    (re.compile(r"/mnt/[A-Za-z]/Repos_Alis/site"), r"<site-root>"),
    (re.compile(r"/mnt/[A-Za-z]/Repos_Alis/Alis"), r"<project-root>"),
    (re.compile(r"\\\\wsl\.localhost\\[^\\]+\\home\\[^\\]+\\repos_alis\\cdn"), r"<cdn-repo>"),
    (re.compile(r"/home/[^/]+/repos_alis/cdn"), r"<cdn-repo>"),
    (re.compile(r"[A-Za-z]:\\UnrealEngine(?:-[0-9.]+|\\UE_[0-9.]+)"), r"<ue-path>"),
    (re.compile(r"[A-Za-z]:/UnrealEngine(?:-[0-9.]+|/UE_[0-9.]+)"), r"<ue-path>"),
    (re.compile(r"[A-Za-z]:\\Program Files(?: \\(x86\\))?\\Epic Games\\UE_[0-9.]+"), r"<ue-path>"),
    (re.compile(r"[A-Za-z]:/Program Files(?: \\(x86\\))?/Epic Games/UE_[0-9.]+"), r"<ue-path>"),
    (re.compile(r"[A-Za-z]:\\Program Files(?: \\(x86\\))?\\Git(?:\\(?:usr|mingw64)\\bin)?\\gpg\.exe"), r"gpg"),
    (re.compile(r"[A-Za-z]:/Program Files(?: \\(x86\\))?/Git(?:/(?:usr|mingw64)/bin)?/gpg\.exe"), r"gpg"),
    (re.compile(r"[A-Za-z]:\\Program Files\\Python[0-9]+\\python\.exe"), r"python"),
    (re.compile(r"[A-Za-z]:/Program Files/Python[0-9]+/python\.exe"), r"python"),
    (re.compile(r"[A-Za-z]:\\Program Files(?: \\(x86\\))?\\Windows Kits\\10\\Debuggers\\x64\\cdb\.exe"), r"<debugger-path>"),
    (re.compile(r"[A-Za-z]:/Program Files(?: \\(x86\\))?/Windows Kits/10/Debuggers/x64/cdb\.exe"), r"<debugger-path>"),
    (re.compile(r"[A-Za-z]:\\Symbols"), r"<symbols-dir>"),
    (re.compile(r"[A-Za-z]:/Symbols"), r"<symbols-dir>"),
    (re.compile(r"[A-Za-z]:\\Builds\\[A-Za-z0-9_.-]+"), r"<build-dir>"),
    (re.compile(r"[A-Za-z]:/Builds/[A-Za-z0-9_.-]+"), r"<build-dir>"),
    (re.compile(r"[A-Za-z]:\\Games\\Alis"), r"<install-root>"),
    (re.compile(r"[A-Za-z]:/Games/Alis"), r"<install-root>"),
    (re.compile(r"[A-Za-z]:\\Users\\[^\\]+\\AppData\\Local\\Temp\\"), r"%TEMP%\\"),
    (re.compile(r"[A-Za-z]:/Users/[^/]+/AppData/Local/Temp/"), r"%TEMP%/"),
    (re.compile(r"[A-Za-z]:\\Users\\[^\\]+\\AppData\\Local\\"), r"%LOCALAPPDATA%\\"),
    (re.compile(r"[A-Za-z]:/Users/[^/]+/AppData/Local/"), r"%LOCALAPPDATA%/"),
    (re.compile(r"[A-Za-z]:\\Users\\[^\\]+\\Documents\\"), r"%USERPROFILE%\\Documents\\"),
    (re.compile(r"[A-Za-z]:/Users/[^/]+/Documents/"), r"%USERPROFILE%/Documents/"),
    (re.compile(r"[A-Za-z]:\\Users\\[^\\]+\\"), r"%USERPROFILE%\\"),
    (re.compile(r"[A-Za-z]:/Users/[^/]+/"), r"%USERPROFILE%/"),
    (re.compile(r"\\\\wsl\.localhost\\[^\\]+\\home\\[^\\]+\\"), r"%WSL_HOME%\\"),
    (re.compile(r"/home/[^/]+/"), r"$HOME/"),
    (re.compile(r"%USERPROFILE%"), r"<user-home>"),
    (re.compile(r"%LOCALAPPDATA%"), r"<local-app-data>"),
    (re.compile(r"%TEMP%"), r"<temp-dir>"),
    (re.compile(r"%WSL_HOME%"), r"<wsl-home>"),
    (re.compile(r"\$HOME"), r"<home>"),
    (re.compile(r"<wsl-home>[\\/]+repos_alis[\\/]+cdn"), r"<cdn-repo>"),
    (re.compile(r"<home>[\\/]+repos_alis[\\/]+cdn"), r"<cdn-repo>"),
]

identity_replacements = [
    (re.compile(r"\bAlis Team\b"), "ALIS"),
    (re.compile(r"\bvslvg\b"), "<user>"),
    (re.compile(r"\bKATANA\b"), "<user>"),
    (re.compile(r"\bfallintodusk\b"), "<user>"),
]

uplugin_replacements = [
    (re.compile(r'("CreatedBy"\s*:\s*)".*?"'), r'\1"ALIS"'),
    (re.compile(r'("CreatedByURL"\s*:\s*)".*?"'), r'\1""'),
    (re.compile(r'("SupportURL"\s*:\s*)".*?"'), r'\1""'),
]

for path in root.rglob("*"):
    if not path.is_file():
        continue

    rel_path_posix = path.relative_to(root).as_posix()
    if rel_path_posix.startswith("scripts/git/mirror/"):
        continue

    name_lc = path.name.lower()
    suffix_lc = path.suffix.lower()
    if suffix_lc not in text_suffixes and not name_lc.startswith("readme"):
        continue

    try:
        original = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        continue

    text = original
    for pattern, replacement in text_replacements:
        text = pattern.sub(replacement, text)
    for pattern, replacement in identity_replacements:
        text = pattern.sub(replacement, text)

    if suffix_lc == ".uplugin":
        for pattern, replacement in uplugin_replacements:
            text = pattern.sub(replacement, text)

    if text == original:
        continue

    newline = "\r\n" if "\r\n" in original else "\n"
    with open(path, "w", encoding="utf-8", newline=newline) as handle:
        handle.write(text)
PY
}

validate_filtered_tree() {
  local filtered_dir="$1"
  local forbidden_patterns_file="$2"
  local fail_flag=0
  local rel_path=""
  local rel_path_lc=""
  local item=""
  local text_patterns_compiled=""

  while IFS= read -r -d '' item; do
    rel_path="${item#$filtered_dir/}"
    rel_path_lc="$(printf '%s' "$rel_path" | tr '[:upper:]' '[:lower:]')"

    case "$rel_path_lc" in
      binaries|binaries/*|build|build/*|content|content/*|config|config/*|deriveddatacache|deriveddatacache/*|intermediate|intermediate/*|saved|saved/*|releases|releases/*|artifacts|artifacts/*|localappdata|localappdata/*|langchain_env|langchain_env/*|plugins/*/binaries|plugins/*/binaries/*|plugins/*/content|plugins/*/content/*|plugins/*/intermediate|plugins/*/intermediate/*|plugins/*/resources|plugins/*/resources/*|plugins/*/data|plugins/*/data/*|plugins/*/thirdparty|plugins/*/thirdparty/*)
        printf '[FAIL] Forbidden path survived filtering: %s\n' "$rel_path" >&2
        fail_flag=1
        ;;
    esac

    if [[ -f "$item" ]]; then
      case "$rel_path_lc" in
        *.uasset|*.umap|*.ubulk|*.uexp|*.utoc|*.ucas|*.pak|*.dll|*.exe|*.pdb|*.obj|*.lib|*.so|*.dylib|*.app|*.ipa|*.kdbx|*.pem|*.pfx|*.key)
          printf '[FAIL] Forbidden file type survived filtering: %s\n' "$rel_path" >&2
          fail_flag=1
          ;;
      esac
    fi
  done < <(find "$filtered_dir" -mindepth 1 -print0)

  if [[ -f "$forbidden_patterns_file" ]] && grep -Eqv '^[[:space:]]*(#|$)' "$forbidden_patterns_file"; then
    text_patterns_compiled="$(mktemp "${TMPDIR:-/tmp}/mirror-forbidden.XXXXXX")"
    grep -Ev '^[[:space:]]*(#|$)' "$forbidden_patterns_file" > "$text_patterns_compiled"
    while IFS= read -r -d '' item; do
      rel_path="${item#$filtered_dir/}"
      rel_path_lc="$(printf '%s' "$rel_path" | tr '[:upper:]' '[:lower:]')"
      if [[ "$rel_path_lc" == scripts/git/mirror/* ]]; then
        continue
      fi
      if LC_ALL=C grep -I -n -H -E -f "$text_patterns_compiled" "$item" >/tmp/mirror_forbidden_matches.txt 2>/dev/null; then
        printf '[FAIL] Forbidden content matched in %s\n' "$rel_path" >&2
        cat /tmp/mirror_forbidden_matches.txt >&2
        fail_flag=1
      fi
    done < <(find "$filtered_dir" -type f \
      \( -iname '*.md' -o -iname '*.txt' -o -iname '*.json' -o -iname '*.ini' -o -iname '*.cs' -o -iname '*.cpp' -o -iname '*.c' -o -iname '*.h' -o -iname '*.hpp' -o -iname '*.inl' -o -iname '*.ps1' -o -iname '*.bat' -o -iname '*.sh' -o -iname '*.py' -o -iname '*.yml' -o -iname '*.yaml' -o -iname '*.dsl' -o -iname '*.uplugin' -o -iname '*.uproject' -o -iname 'readme*' \) -print0)
    rm -f "$text_patterns_compiled" /tmp/mirror_forbidden_matches.txt
  fi

  if [[ "$fail_flag" -ne 0 ]]; then
    fail "Filtered mirror tree failed validation."
  fi
}

while (($# > 0)); do
  case "$1" in
    --remote-url)
      shift
      (($# > 0)) || fail "Missing value for --remote-url"
      REMOTE_URL="$1"
      ;;
    --branch)
      shift
      (($# > 0)) || fail "Missing value for --branch"
      BRANCH="$1"
      ;;
    --exclude-file)
      shift
      (($# > 0)) || fail "Missing value for --exclude-file"
      EXCLUDE_FILE="$1"
      ;;
    --forbidden-patterns-file)
      shift
      (($# > 0)) || fail "Missing value for --forbidden-patterns-file"
      FORBIDDEN_PATTERNS_FILE="$1"
      ;;
    --push)
      DO_PUSH=1
      ;;
    --dry-run)
      DO_PUSH=0
      ;;
    --ephemeral-preview)
      EPHEMERAL_PREVIEW=1
      ;;
    --allow-dirty)
      ALLOW_DIRTY=1
      ;;
    --force)
      ALLOW_DIRTY=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "Unknown argument: $1"
      ;;
  esac
  shift
done

[[ -n "$BRANCH" ]] || fail "Branch cannot be empty"
if [[ "$DO_PUSH" -eq 1 ]] && [[ -z "$REMOTE_URL" ]]; then
  fail "Remote URL is required when --push is used."
fi
if [[ "$DO_PUSH" -eq 0 ]] && [[ -z "$REMOTE_URL" ]] && [[ "$EPHEMERAL_PREVIEW" -eq 0 ]]; then
  fail "Remote URL is required for dry-run baseline comparison. Use --ephemeral-preview only for one-off local preview."
fi

require_cmd git
require_cmd rsync
require_cmd mktemp
require_cmd date
require_cmd find
require_cmd grep
require_cmd python3

REPO_ROOT="$(git_safe -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null || true)"
[[ -n "$REPO_ROOT" ]] || fail "Could not detect repository root from script location"

EXCLUDE_FILE="$(resolve_file "$EXCLUDE_FILE" "$REPO_ROOT")"
[[ -f "$EXCLUDE_FILE" ]] || fail "Exclude file not found: $EXCLUDE_FILE"

if [[ -n "$FORBIDDEN_PATTERNS_FILE" ]]; then
  FORBIDDEN_PATTERNS_FILE="$(resolve_file "$FORBIDDEN_PATTERNS_FILE" "$REPO_ROOT")"
fi
if [[ -n "$FORBIDDEN_PATTERNS_FILE" ]] && [[ ! -f "$FORBIDDEN_PATTERNS_FILE" ]]; then
  fail "Forbidden patterns file not found: $FORBIDDEN_PATTERNS_FILE"
fi

if [[ "$ALLOW_DIRTY" -eq 0 ]]; then
  STAGED_CHANGES="$(git_safe -C "$REPO_ROOT" diff --cached --name-only)"
  UNSTAGED_CHANGES="$(git_safe -C "$REPO_ROOT" diff --name-only)"
  UNTRACKED_CHANGES="$(git_safe -C "$REPO_ROOT" ls-files --others --exclude-standard)"

  if [[ -n "$STAGED_CHANGES$UNSTAGED_CHANGES$UNTRACKED_CHANGES" ]]; then
    printf '[FAIL] Source repository has local changes. Mirror aborted.\n' >&2
    if [[ -n "$STAGED_CHANGES" ]]; then
      printf '[FAIL] Staged files detected:\n%s\n' "$STAGED_CHANGES" >&2
    fi
    if [[ -n "$UNSTAGED_CHANGES" ]]; then
      printf '[FAIL] Uncommitted files detected:\n%s\n' "$UNSTAGED_CHANGES" >&2
    fi
    if [[ -n "$UNTRACKED_CHANGES" ]]; then
      printf '[FAIL] Untracked files detected:\n%s\n' "$UNTRACKED_CHANGES" >&2
    fi
    fail "Commit/stash changes first, or re-run with --force."
  fi
fi

TEMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/repo-mirror.XXXXXX")"
FILTERED_DIR="$TEMP_ROOT/filtered"
MIRROR_DIR="$TEMP_ROOT/mirror"

cleanup() {
  rm -rf "$TEMP_ROOT" 2>/dev/null || true
}
trap cleanup EXIT

mkdir -p "$FILTERED_DIR" "$MIRROR_DIR"

build_filtered_snapshot "$REPO_ROOT" "$EXCLUDE_FILE" "$FILTERED_DIR" "$TEMP_ROOT"
info "Sanitizing filtered mirror tree for public anonymity"
sanitize_filtered_tree "$FILTERED_DIR"

info "Validating filtered mirror tree"
validate_filtered_tree "$FILTERED_DIR" "$FORBIDDEN_PATTERNS_FILE"

info "Preparing temporary mirror repository"
git_safe -c init.defaultBranch="$BRANCH" init -q "$MIRROR_DIR"
git_safe -C "$MIRROR_DIR" config gc.auto 0
git_safe -C "$MIRROR_DIR" config core.autocrlf false
git_safe -C "$MIRROR_DIR" config core.safecrlf false

if [[ -n "$REMOTE_URL" ]]; then
  git_safe -C "$MIRROR_DIR" remote add mirror "$REMOTE_URL"
  if git_safe -C "$MIRROR_DIR" ls-remote --exit-code --heads mirror "$BRANCH" >/dev/null 2>&1; then
    git_safe -C "$MIRROR_DIR" fetch --depth=1 --quiet mirror "$BRANCH"
    git_safe -C "$MIRROR_DIR" checkout -q -B "$BRANCH" FETCH_HEAD
  else
    git_safe -C "$MIRROR_DIR" checkout -q --orphan "$BRANCH"
  fi
else
  git_safe -C "$MIRROR_DIR" checkout -q --orphan "$BRANCH"
  info "Running without remote baseline in ephemeral preview mode"
fi

rsync -a --delete --exclude='.git/' "$FILTERED_DIR"/ "$MIRROR_DIR"/

git_safe -C "$MIRROR_DIR" add -A
if git_safe -C "$MIRROR_DIR" diff --cached --quiet; then
  printf '[SUMMARY] no changes after filtering; nothing to commit\n'
  exit 0
fi

git_safe -C "$MIRROR_DIR" config user.name "mirror-bot"
git_safe -C "$MIRROR_DIR" config user.email "mirror-bot@localhost"

FILES_CHANGED="$(git_safe -C "$MIRROR_DIR" diff --cached --name-only | wc -l | tr -d '[:space:]')"
COMMIT_MESSAGE="mirror_$(date -u '+%Y%m%d_%H%M%S_UTC')"

GIT_AUTHOR_NAME="mirror-bot" \
GIT_AUTHOR_EMAIL="mirror-bot@localhost" \
GIT_COMMITTER_NAME="mirror-bot" \
GIT_COMMITTER_EMAIL="mirror-bot@localhost" \
git_safe -C "$MIRROR_DIR" commit -m "$COMMIT_MESSAGE" >/dev/null
COMMIT_SHA="$(git_safe -C "$MIRROR_DIR" rev-parse --short HEAD)"

if [[ "$DO_PUSH" -eq 1 ]]; then
  info "Pushing commit to remote branch $BRANCH"
  git_safe -C "$MIRROR_DIR" push mirror "HEAD:$BRANCH"
  PUSH_RESULT="pushed"
elif [[ -z "$REMOTE_URL" ]]; then
  info "Ephemeral preview complete. No remote baseline or push was used."
  PUSH_RESULT="ephemeral-preview"
else
  info "Dry run complete. Use --push to publish."
  PUSH_RESULT="dry-run"
fi

printf '[SUMMARY] branch=%s commit=%s files_changed=%s push=%s remote=%s\n' \
  "$BRANCH" "$COMMIT_SHA" "$FILES_CHANGED" "$PUSH_RESULT" "${REMOTE_URL:-none}"
