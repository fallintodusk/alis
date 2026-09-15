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
PRIVATE_FORBIDDEN_PATTERNS_FILE="$SCRIPT_DIR/forbidden_private_text_patterns.regex"
ALLOW_DIRTY=0
DO_PUSH=0
EPHEMERAL_PREVIEW=0
DEVELOPER_RELEASE_DIR=""
DEVELOPER_VERSION=""
DEVELOPER_PART_SIZE_MIB=1900
SOURCE_REVISION="HEAD"
CANDIDATE_DIR=""
PUBLIC_WORLD_MANIFEST_ROOT=""
DEVELOPER_ASSET_ROOT=""
REPORT_PATH=""
OFFICIAL_RELEASE=0
OFFICIAL_BASE_SHA=""
CLEAN_PREVALIDATED="${MIRROR_CLEAN_PREVALIDATED:-0}"

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
  --developer-release-dir <path>    Compose the public developer payload in this empty directory.
  --developer-version <version>     Human release version used in developer archive names.
  --developer-part-size-mib <int>   Split size in MiB (default and maximum: 1900).
  --source-revision <commit>        Private source commit to filter (default: HEAD).
  --candidate-dir <path>            Preserve the local filtered Git repository here.
  --public-world-manifest-root <path>
                                    Project public World manifests into filtered source.
  --developer-asset-root <path>     Compose payload bytes from this generated project root.
  --report <path>                   Write an accepted machine-readable privacy/projection report.
  --official-release                Require the canonical ALIS GitHub remote, a release branch,
                                    and the accepted public World manifest projection.
  --force                           Project the current tracked/untracked non-ignored worktree.
  --allow-dirty                     Backward-compatible alias for --force.
  -h, --help                        Show this help.

Examples:
  ./scripts/git/mirror/mirror_to_github.sh --remote-url git@github.com:org/repo.git --dry-run
  ./scripts/git/mirror/mirror_to_github.sh --dry-run --ephemeral-preview \
    --developer-release-dir ../alis-developer-1.0.0 --developer-version 1.0.0
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

warn() {
  printf '[WARN] %s\n' "$*" >&2
}

write_report() {
  local report_path="$1"
  local source_revision="$2"
  local source_tree="$3"
  local files_changed="$4"
  local push_result="$5"
  mkdir -p "$(dirname -- "$report_path")"
  python3 - "$report_path" "$source_revision" "$source_tree" "$BRANCH" \
    "$files_changed" "$push_result" "${REMOTE_URL:-none}" <<'PY'
import json
import sys
from pathlib import Path

path, revision, tree, branch, changed, push, remote = sys.argv[1:]
report = {
    "schema": "alis-public-source-privacy-v1",
    "status": "accepted",
    "source_revision": revision,
    "source_tree": tree,
    "branch": branch,
    "files_changed": int(changed),
    "publication_result": push,
    "remote": remote,
    "issue_count": 0,
}
Path(path).write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
PY
}

require_cmd() {
  local cmd="$1"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    fail "Required command not found: $cmd"
  fi
}

# Hooks are disabled by pointing at a UNIQUE, empty, per-invocation directory -
# never at /dev/null. On Windows a native git resolves "/dev/null" RELATIVE to the
# repository (it has no drive letter), so git-lfs creates a literal
# <repo>/dev/null/ directory and installs post-checkout, post-commit, post-merge
# and pre-push into it. Git Bash happens to rewrite the argument to "nul" today,
# which only hides the hazard. See git-lfs issue 6297.
# The directory must be per-invocation: a fixed shared path persists between runs,
# so anything that ever drops a hook there would be executed by every later
# "safe" call - the opposite of the guarantee this makes.
PROJECT_TMP_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)/tmp/git/mirror"
mkdir -p "$PROJECT_TMP_ROOT"
PROJECT_EMPTY_HOOKS="$(mktemp -d "$PROJECT_TMP_ROOT/project-empty-hooks.XXXXXX")"
trap 'rm -rf "$PROJECT_EMPTY_HOOKS" 2>/dev/null || true' EXIT

git_safe() {
  git -c core.fsmonitor=false -c core.hooksPath="$PROJECT_EMPTY_HOOKS" -c core.longpaths=true "$@"
}

compose_developer_payload() {
  local public_revision="$1"
  local compose_args=(
    --repo-root "$DEVELOPER_ASSET_ROOT"
    --output-dir "$DEVELOPER_RELEASE_DIR"
    --release-version "$DEVELOPER_VERSION"
    --public-source-tag "v${DEVELOPER_VERSION#v}"
    --public-source-revision "$public_revision"
    --public-source-branch "$BRANCH"
    --world-manifest-root "$MIRROR_DIR/Plugins/World/ProjectWorldData/Data/Manifests"
    --part-size-mib "$DEVELOPER_PART_SIZE_MIB"
    --owner ProjectWorldData
  )
  if [[ "$DEVELOPER_ASSET_ROOT" != "$REPO_ROOT" || "$ALLOW_DIRTY" -eq 1 ]]; then
    compose_args+=(--allow-dirty)
  fi
  info "Composing developer payload for public revision $public_revision"
  python3 "$SCRIPT_DIR/compose_developer_payload.py" "${compose_args[@]}"
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
  local source_revision="$5"
  local tracked_paths_file="$temp_root/tracked_paths.nul"
  local allowed_paths_file="$temp_root/allowed_paths.nul"
  local head_index_file="$temp_root/head.index"

  if [[ "$ALLOW_DIRTY" -eq 1 ]]; then
    info "Selecting current tracked/untracked non-ignored worktree files that survive blacklist rules"
    git_safe -C "$repo_root" ls-files --cached --others --exclude-standard -z > "$tracked_paths_file"
  else
    info "Selecting tracked source files that survive blacklist rules"
    git_safe -C "$repo_root" ls-tree -r --name-only -z "$source_revision" > "$tracked_paths_file"
  fi

  python3 - "$exclude_file" "$tracked_paths_file" "$allowed_paths_file" "$repo_root" "$ALLOW_DIRTY" <<'PY'
import pathlib
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
    if not (pathlib.Path(sys.argv[4]) / rel_path).exists() and sys.argv[5] == "1":
        continue
    allowed.append(raw_path)

with open(allowed_paths_file, "wb") as handle:
    if allowed:
        handle.write(b"\0".join(allowed))
PY

  mkdir -p "$filtered_dir"
  if [[ "$ALLOW_DIRTY" -eq 1 ]]; then
    rsync -a --from0 --files-from="$allowed_paths_file" "$repo_root/" "$filtered_dir/"
  else
    : > "$head_index_file"
    GIT_INDEX_FILE="$head_index_file" git_safe -C "$repo_root" read-tree "$source_revision"
    if [[ -s "$allowed_paths_file" ]]; then
      GIT_INDEX_FILE="$head_index_file" git_safe -C "$repo_root" checkout-index -q -z --stdin --prefix="$filtered_dir/" < "$allowed_paths_file"
    fi
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
    (re.compile(r"[A-Za-z]:\\+Repos_Alis\\+site"), r"<site-root>"),
    (re.compile(r"[A-Za-z]:/Repos_Alis/site"), r"<site-root>"),
    (re.compile(r"[A-Za-z]:\\+Repos_Alis\\+Alis"), r"<project-root>"),
    (re.compile(r"[A-Za-z]:/Repos_Alis/Alis"), r"<project-root>"),
    (re.compile(r"/mnt/[A-Za-z]/Repos_Alis/site"), r"<site-root>"),
    (re.compile(r"/mnt/[A-Za-z]/Repos_Alis/Alis"), r"<project-root>"),
    (re.compile(r"\\\\wsl\.localhost\\[^\\]+\\home\\[^\\]+\\repos_alis\\cdn"), r"<cdn-repo>"),
    (re.compile(r"/home/[^/]+/repos_alis/cdn"), r"<cdn-repo>"),
    (re.compile(r"~/repos_alis/cdn"), r"<cdn-repo>"),
    (re.compile(r"~/repos_alis/site"), r"<site-root>"),
    (re.compile(r"~/repos_alis/Alis"), r"<project-root>"),
    (re.compile(r"~/repos_alis/"), r"$HOME/repos_alis/"),
    (re.compile(r"[A-Za-z]:\\UnrealEngine(?:-[0-9.]+|\\UE_[0-9.]+)"), r"<ue-path>"),
    (re.compile(r"[A-Za-z]:/UnrealEngine(?:-[0-9.]+|/UE_[0-9.]+)"), r"<ue-path>"),
    (re.compile(r"[A-Za-z]:\\Program Files(?: \\(x86\\))?\\Epic Games\\UE_[0-9.]+"), r"<ue-path>"),
    (re.compile(r"[A-Za-z]:/Program Files(?: \\(x86\\))?/Epic Games/UE_[0-9.]+"), r"<ue-path>"),
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

neutralize_lfs_attributes() {
  local filtered_dir="$1"

  # The public mirror is code+docs only and ships no Git LFS store. Any
  # LFS-tracked file that legitimately survives filtering (e.g. a small doc
  # or README image) must be committed as a plain blob. Leaving `filter=lfs` in
  # the mirrored `.gitattributes` makes `git add` emit an LFS pointer whose
  # object is never pushed, which GitHub rejects with GH008. Strip the LFS
  # directives but keep binary/eol normalization.
  python3 - "$filtered_dir" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
lfs_tokens = {"filter=lfs", "merge=lfs", "diff=lfs"}
binary_markers = {"-text", "text", "binary"}

for path in root.rglob(".gitattributes"):
    if not path.is_file():
        continue

    try:
        original = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        continue

    newline = "\r\n" if "\r\n" in original else "\n"
    trailing_newline = original.endswith("\n")

    out_lines = []
    changed = False
    for line in original.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or "=lfs" not in line:
            out_lines.append(line)
            continue

        tokens = line.split()
        kept = [token for token in tokens if token not in lfs_tokens]
        # Keep dropped LFS binaries marked as binary so the public mirror does
        # not CRLF-mangle them after losing the LFS filter.
        if not any(marker in kept for marker in binary_markers):
            kept.append("-text")

        new_line = " ".join(kept)
        if new_line != line:
            changed = True
        out_lines.append(new_line)

    if not changed:
        continue

    text = newline.join(out_lines)
    if trailing_newline:
        text += newline

    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(text)
PY
}

# True only when a file IS a genuine Git LFS pointer, matched by full pointer
# shape - NOT by mentioning the signature anywhere. Per the LFS spec the
# canonical sha256 pointer is exactly:
#   line 1: version https://git-lfs.github.com/spec/v1
#   line 2: oid sha256:<64 lowercase hex>
#   line 3: size <integer>
# Markdown/scripts/docs that merely quote this text (this script, canonical.md
# section 8.6, the foliage-recovery todo) never match all three lines and pass.
# CR is stripped so CRLF checkouts on Windows still match; the 'q' in each sed
# bounds the read so a large file is never slurped whole. A real pointer of ANY
# extension fails. (Exotic pointers with custom ext-* lines are not matched
# here; GH008 on push is the backstop for that rare case.)
is_lfs_pointer_file() {
  local item="$1"
  local line1 line2 line3
  line1="$(LC_ALL=C sed -n '1{p;q}' "$item" 2>/dev/null)"; line1="${line1%$'\r'}"
  line2="$(LC_ALL=C sed -n '2{p;q}' "$item" 2>/dev/null)"; line2="${line2%$'\r'}"
  line3="$(LC_ALL=C sed -n '3{p;q}' "$item" 2>/dev/null)"; line3="${line3%$'\r'}"

  [[ "$line1" == "version https://git-lfs.github.com/spec/v1" ]] &&
  [[ "$line2" =~ ^oid[[:space:]]+sha256:[0-9a-f]{64}$ ]] &&
  [[ "$line3" =~ ^size[[:space:]]+[0-9]+$ ]]
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
      binaries|binaries/*|build|build/*|content|content/*|deriveddatacache|deriveddatacache/*|intermediate|intermediate/*|saved|saved/*|releases|releases/*|artifacts|artifacts/*|localappdata|localappdata/*|langchain_env|langchain_env/*|plugins/*/binaries|plugins/*/binaries/*|plugins/*/intermediate|plugins/*/intermediate/*|plugins/*/resources|plugins/*/resources/*|plugins/*/thirdparty|plugins/*/thirdparty/*)
        printf '[FAIL] Forbidden path survived filtering: %s\n' "$rel_path" >&2
        fail_flag=1
        ;;
    esac

    if [[ -f "$item" ]]; then
      case "$rel_path_lc" in
        *.uasset|*.umap|*.ubulk|*.uexp|*.uptnl|*.ushaderbytecode|*.utoc|*.ucas|*.pak|*.dll|*.exe|*.pdb|*.obj|*.lib|*.so|*.dylib|*.app|*.ipa|*.kdbx|*.pem|*.pfx|*.p12|*.key|*.png|*.jpg|*.jpeg|*.gif|*.webp|*.bmp|*.tga|*.tiff|*.exr|*.ico|*.psd|*.xcf|*.ai|*.fbx|*.blend|*.3ds|*.glb|*.gltf|*.wav|*.mp3|*.ogg|*.flac|*.mp4|*.mov|*.avi|*.webm)
          printf '[FAIL] Forbidden file type survived filtering: %s\n' "$rel_path" >&2
          fail_flag=1
          ;;
      esac

      # Generic binary guard: the public mirror is source/docs/text data only.
      # grep -I treats a NUL-containing file as binary and yields no match, so
      # this fails ANY non-empty binary file regardless of extension. This is what
      # makes "text data only" true rather than trusting the extension denylist
      # alone (an unknown-extension binary under a now-published Data/ dir, etc.).
      # Empty pattern '' matches every line (incl. blank), so a blank-line-only
      # text file passes; only NUL/binary content fails. NOTE: UTF-16 files
      # contain NUL bytes and will fail here by design -- keep mirrored text UTF-8.
      if [[ -s "$item" ]] && ! LC_ALL=C grep -Iq '' "$item"; then
        printf '[FAIL] Binary-like file survived filtering: %s\n' "$rel_path" >&2
        fail_flag=1
      fi
    fi
  done < <(find "$filtered_dir" -mindepth 1 -print0)

  if [[ -f "$forbidden_patterns_file" ]] && grep -Eqv '^[[:space:]]*(#|$)' "$forbidden_patterns_file"; then
    text_patterns_compiled="$TEMP_ROOT/mirror-forbidden.regex"
    grep -Ev '^[[:space:]]*(#|$)' "$forbidden_patterns_file" > "$text_patterns_compiled"
    if [[ -f "$PRIVATE_FORBIDDEN_PATTERNS_FILE" ]]; then
      grep -Ev '^[[:space:]]*(#|$)' "$PRIVATE_FORBIDDEN_PATTERNS_FILE" >> "$text_patterns_compiled"
    fi
    while IFS= read -r -d '' item; do
      rel_path="${item#$filtered_dir/}"
      rel_path_lc="$(printf '%s' "$rel_path" | tr '[:upper:]' '[:lower:]')"
      if [[ "$rel_path_lc" == scripts/git/mirror/tests/* ||
            "$rel_path_lc" == scripts/git/mirror/forbidden_text_patterns.regex ||
            "$rel_path_lc" == scripts/git/mirror/mirror_to_github.sh ]]; then
        continue
      fi
      if LC_ALL=C grep -I -n -H -E -f "$text_patterns_compiled" "$item" >"$TEMP_ROOT/mirror_forbidden_matches.txt" 2>/dev/null; then
        printf '[FAIL] Forbidden content matched in %s\n' "$rel_path" >&2
        cat "$TEMP_ROOT/mirror_forbidden_matches.txt" >&2
        fail_flag=1
      fi
    done < <(find "$filtered_dir" -type f \
      \( -iname '*.md' -o -iname '*.txt' -o -iname '*.json' -o -iname '*.ini' -o -iname '*.cs' -o -iname '*.cpp' -o -iname '*.c' -o -iname '*.h' -o -iname '*.hpp' -o -iname '*.inl' -o -iname '*.ps1' -o -iname '*.bat' -o -iname '*.sh' -o -iname '*.py' -o -iname '*.yml' -o -iname '*.yaml' -o -iname '*.dsl' -o -iname '*.uplugin' -o -iname '*.uproject' -o -iname 'readme*' \) -print0)
    rm -f "$text_patterns_compiled" "$TEMP_ROOT/mirror_forbidden_matches.txt"
  fi

  # Hard guard: a Git LFS pointer that leaked into the snapshot (object not
  # smudged at checkout, or attributes missed) would publish broken pointer
  # text and can trigger GH008 on push. Refuse it explicitly. Detection matches
  # full pointer SHAPE (see is_lfs_pointer_file), never a mention of the
  # signature, so docs/scripts/todos that quote the format stay public.
  while IFS= read -r -d '' item; do
    if is_lfs_pointer_file "$item"; then
      rel_path="${item#$filtered_dir/}"
      printf '[FAIL] Git LFS pointer survived filtering: %s\n' "$rel_path" >&2
      fail_flag=1
    fi
  done < <(find "$filtered_dir" -type f -print0)

  # Character-set policy: no foreign-script symbols/comments (Cyrillic, CJK) in
  # published docs/text or surviving paths. Reuses the governance validator
  # (validate_text_format) so local checks and the mirror share one rule. The
  # filtered snapshot is exactly what will be published. Disallowed blocks are
  # extensible data in that script; paths are strict ASCII.
  local text_format_checker="$REPO_ROOT/scripts/ue/check/governance/validate_text_format.py"
  if [[ -f "$text_format_checker" ]]; then
    local tf_out
    if ! tf_out="$(python3 "$text_format_checker" --root "$filtered_dir" --all-text 2>&1)"; then
      printf '%s\n' "$tf_out" >&2
      fail_flag=1
    fi
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
    --developer-release-dir)
      shift
      (($# > 0)) || fail "Missing value for --developer-release-dir"
      DEVELOPER_RELEASE_DIR="$1"
      ;;
    --developer-version)
      shift
      (($# > 0)) || fail "Missing value for --developer-version"
      DEVELOPER_VERSION="$1"
      ;;
    --developer-part-size-mib)
      shift
      (($# > 0)) || fail "Missing value for --developer-part-size-mib"
      DEVELOPER_PART_SIZE_MIB="$1"
      ;;
    --source-revision)
      shift
      (($# > 0)) || fail "Missing value for --source-revision"
      SOURCE_REVISION="$1"
      ;;
    --candidate-dir)
      shift
      (($# > 0)) || fail "Missing value for --candidate-dir"
      CANDIDATE_DIR="$1"
      ;;
    --public-world-manifest-root)
      shift
      (($# > 0)) || fail "Missing value for --public-world-manifest-root"
      PUBLIC_WORLD_MANIFEST_ROOT="$1"
      ;;
    --developer-asset-root)
      shift
      (($# > 0)) || fail "Missing value for --developer-asset-root"
      DEVELOPER_ASSET_ROOT="$1"
      ;;
    --report)
      shift
      (($# > 0)) || fail "Missing value for --report"
      REPORT_PATH="$1"
      ;;
    --official-release)
      OFFICIAL_RELEASE=1
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
if { [[ -n "$DEVELOPER_RELEASE_DIR" ]] && [[ -z "$DEVELOPER_VERSION" ]]; } ||
   { [[ -z "$DEVELOPER_RELEASE_DIR" ]] && [[ -n "$DEVELOPER_VERSION" ]]; }; then
  fail "--developer-release-dir and --developer-version must be supplied together."
fi
if [[ -n "$DEVELOPER_ASSET_ROOT" ]] && [[ -z "$DEVELOPER_RELEASE_DIR" ]]; then
  fail "--developer-asset-root requires --developer-release-dir."
fi
if [[ "$DO_PUSH" -eq 1 ]] && [[ -n "$DEVELOPER_RELEASE_DIR" ]]; then
  fail "Developer payload publication is not implemented: compose with --dry-run, then use the tracked draft release transaction before pushing source/tag."
fi
if [[ "$DO_PUSH" -eq 1 ]] && [[ -n "$CANDIDATE_DIR" ]]; then
  fail "--candidate-dir is a local evidence output and cannot be combined with --push."
fi
if [[ "$OFFICIAL_RELEASE" -eq 1 ]]; then
  case "$REMOTE_URL" in
    git@github.com:fallintodusk/alis.git|https://github.com/fallintodusk/alis.git|ssh://git@github.com/fallintodusk/alis.git)
      ;;
    *)
      fail "Official release remote must be fallintodusk/alis."
      ;;
  esac
  [[ "$BRANCH" == release/v* ]] || fail "Official release source must use a release/v* branch, not '$BRANCH'."
  [[ -n "$PUBLIC_WORLD_MANIFEST_ROOT" ]] || fail "Official release requires --public-world-manifest-root; private World authority cannot enter the public source or developer payload."
fi
if ! [[ "$DEVELOPER_PART_SIZE_MIB" =~ ^[0-9]+$ ]] || ((DEVELOPER_PART_SIZE_MIB < 1 || DEVELOPER_PART_SIZE_MIB > 1900)); then
  fail "--developer-part-size-mib must be between 1 and 1900."
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
DEVELOPER_ASSET_ROOT="${DEVELOPER_ASSET_ROOT:-$REPO_ROOT}"
if [[ "$ALLOW_DIRTY" -eq 1 ]]; then
  [[ "$(git_safe -C "$REPO_ROOT" rev-parse "$SOURCE_REVISION")" == "$(git_safe -C "$REPO_ROOT" rev-parse HEAD)" ]] ||
    fail "--force projects the current worktree and therefore requires --source-revision HEAD."
fi
SOURCE_REVISION="$(git_safe -C "$REPO_ROOT" rev-parse --verify "$SOURCE_REVISION^{commit}" 2>/dev/null || true)"
[[ -n "$SOURCE_REVISION" ]] || fail "Source revision does not resolve to a commit."
[[ -d "$DEVELOPER_ASSET_ROOT" ]] || fail "Developer asset root does not exist: $DEVELOPER_ASSET_ROOT"
if [[ -n "$PUBLIC_WORLD_MANIFEST_ROOT" ]]; then
  [[ -d "$PUBLIC_WORLD_MANIFEST_ROOT" ]] || fail "Public World manifest root does not exist: $PUBLIC_WORLD_MANIFEST_ROOT"
fi

EXCLUDE_FILE="$(resolve_file "$EXCLUDE_FILE" "$REPO_ROOT")"
[[ -f "$EXCLUDE_FILE" ]] || fail "Exclude file not found: $EXCLUDE_FILE"

if [[ -n "$FORBIDDEN_PATTERNS_FILE" ]]; then
  FORBIDDEN_PATTERNS_FILE="$(resolve_file "$FORBIDDEN_PATTERNS_FILE" "$REPO_ROOT")"
fi
if [[ -n "$FORBIDDEN_PATTERNS_FILE" ]] && [[ ! -f "$FORBIDDEN_PATTERNS_FILE" ]]; then
  fail "Forbidden patterns file not found: $FORBIDDEN_PATTERNS_FILE"
fi

if [[ "$ALLOW_DIRTY" -eq 0 ]] && [[ "$CLEAN_PREVALIDATED" -ne 1 ]]; then
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

TEMP_ROOT="$(mktemp -d "$PROJECT_TMP_ROOT/repo-mirror.XXXXXX")"
FILTERED_DIR="$TEMP_ROOT/filtered"
MIRROR_DIR="${CANDIDATE_DIR:-$TEMP_ROOT/mirror}"

cleanup() {
  rm -rf "$TEMP_ROOT" 2>/dev/null || true
  rm -rf "$PROJECT_EMPTY_HOOKS" 2>/dev/null || true
}
trap cleanup EXIT

if [[ -n "$CANDIDATE_DIR" ]] && [[ -e "$CANDIDATE_DIR" ]] && [[ -n "$(find "$CANDIDATE_DIR" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
  fail "Candidate directory must be absent or empty: $CANDIDATE_DIR"
fi
mkdir -p "$FILTERED_DIR" "$MIRROR_DIR"

build_filtered_snapshot "$REPO_ROOT" "$EXCLUDE_FILE" "$FILTERED_DIR" "$TEMP_ROOT" "$SOURCE_REVISION"
info "Sanitizing filtered mirror tree for public anonymity"
sanitize_filtered_tree "$FILTERED_DIR"

info "Applying deterministic public project/config projection"
PROJECTION_ARGS=(--root "$FILTERED_DIR")
if [[ -n "$PUBLIC_WORLD_MANIFEST_ROOT" ]]; then
  PROJECTION_ARGS+=(--world-manifest-root "$PUBLIC_WORLD_MANIFEST_ROOT")
fi
python3 "$SCRIPT_DIR/prepare_public_source.py" "${PROJECTION_ARGS[@]}"

info "Neutralizing Git LFS attributes for code-only public mirror"
neutralize_lfs_attributes "$FILTERED_DIR"

info "Validating filtered mirror tree"
validate_filtered_tree "$FILTERED_DIR" "$FORBIDDEN_PATTERNS_FILE"

info "Preparing temporary mirror repository"
git_safe -c init.defaultBranch="$BRANCH" init -q "$MIRROR_DIR"
git_safe -C "$MIRROR_DIR" config gc.auto 0
git_safe -C "$MIRROR_DIR" config core.autocrlf false
git_safe -C "$MIRROR_DIR" config core.safecrlf false
git_safe -C "$MIRROR_DIR" config core.longpaths true

if [[ -n "$REMOTE_URL" ]]; then
  git_safe -C "$MIRROR_DIR" remote add mirror "$REMOTE_URL"
  REMOTE_PROBE_OUTPUT=""
  if REMOTE_PROBE_OUTPUT="$(git_safe -C "$MIRROR_DIR" ls-remote --exit-code --heads mirror "$BRANCH" 2>&1)"; then
    git_safe -C "$MIRROR_DIR" fetch --depth=1 --quiet mirror "$BRANCH"
    git_safe -C "$MIRROR_DIR" checkout -q -B "$BRANCH" FETCH_HEAD
  else
    REMOTE_PROBE_STATUS=$?
    if [[ "$REMOTE_PROBE_STATUS" -eq 2 ]]; then
      if [[ "$OFFICIAL_RELEASE" -eq 1 ]]; then
        git_safe -C "$MIRROR_DIR" fetch --depth=1 --quiet mirror main
        OFFICIAL_BASE_SHA="$(git_safe -C "$MIRROR_DIR" rev-parse FETCH_HEAD)"
        git_safe -C "$MIRROR_DIR" checkout -q -B "$BRANCH" "$OFFICIAL_BASE_SHA"
      else
        git_safe -C "$MIRROR_DIR" checkout -q --orphan "$BRANCH"
      fi
    else
      printf '%s\n' "$REMOTE_PROBE_OUTPUT" >&2
      fail "Cannot establish remote branch state for $BRANCH (git exit $REMOTE_PROBE_STATUS)."
    fi
  fi
else
  git_safe -C "$MIRROR_DIR" checkout -q --orphan "$BRANCH"
  info "Running without remote baseline in ephemeral preview mode"
fi

rsync -a --delete --exclude='.git/' "$FILTERED_DIR"/ "$MIRROR_DIR"/

git_safe -C "$MIRROR_DIR" add -A
if git_safe -C "$MIRROR_DIR" diff --cached --quiet; then
  COMMIT_SHA="$(git_safe -C "$MIRROR_DIR" rev-parse HEAD)"
  if [[ -n "$DEVELOPER_RELEASE_DIR" ]]; then
    compose_developer_payload "$COMMIT_SHA"
  fi
  if [[ -n "$REPORT_PATH" ]]; then
    write_report "$REPORT_PATH" "$COMMIT_SHA" \
      "$(git_safe -C "$MIRROR_DIR" rev-parse HEAD^{tree})" 0 "no-change"
  fi
  printf '[SUMMARY] no changes after filtering; nothing to commit\n'
  exit 0
fi

git_safe -C "$MIRROR_DIR" config user.name "mirror-bot"
git_safe -C "$MIRROR_DIR" config user.email "mirror-bot@localhost"

FILES_CHANGED="$(git_safe -C "$MIRROR_DIR" diff --cached --name-only | wc -l | tr -d '[:space:]')"
# Generated public authority (canonical data, public manifests, release
# selection) may advance on the public source branch ahead of the latest
# signed developer release. Public developers obtain matching assets from a
# tagged developer release, whose installer pins the exact recorded
# commit/tag - an advanced source tip does not break installed releases.
# Coherence between source identity and asset payload is enforced by the
# release flow (compose + sign + verify), not by routine mirror pushes.
AUTHORITY_CHANGED=0
while IFS= read -r changed_path; do
  case "$changed_path" in
    Plugins/World/ProjectWorldData/Data/Manifests/*|\
    Plugins/World/ProjectWorldData/Data/Canonical/*|\
    Plugins/*/*/Data/Manifests/public_*.json|\
    scripts/git/mirror/developer_asset_release.json)
      AUTHORITY_CHANGED=1
      break
      ;;
  esac
done < <(git_safe -C "$MIRROR_DIR" diff --cached --name-only)

if [[ "$AUTHORITY_CHANGED" -eq 1 ]]; then
  if [[ "$OFFICIAL_RELEASE" -eq 1 ]]; then
    info "Generated public authority changed relative to the public branch."
    info "The public source tip moves ahead of the latest signed developer release; assets for this tip become installable only with the next tagged developer release."
  else
    warn "Generated public authority changed relative to the public branch."
    warn "The public source tip moves ahead of the latest signed developer release; assets for this tip become installable only with the next tagged developer release."
  fi
fi

COMMIT_MESSAGE="mirror_$(date -u '+%Y%m%d_%H%M%S_UTC')"

GIT_AUTHOR_NAME="mirror-bot" \
GIT_AUTHOR_EMAIL="mirror-bot@localhost" \
GIT_COMMITTER_NAME="mirror-bot" \
GIT_COMMITTER_EMAIL="mirror-bot@localhost" \
git_safe -C "$MIRROR_DIR" commit -m "$COMMIT_MESSAGE" >/dev/null
COMMIT_SHA="$(git_safe -C "$MIRROR_DIR" rev-parse HEAD)"

if [[ -n "$OFFICIAL_BASE_SHA" ]]; then
  [[ "$(git_safe -C "$MIRROR_DIR" rev-parse HEAD^)" == "$OFFICIAL_BASE_SHA" ]] ||
    fail "Official release candidate is not a direct child of remote main."
fi

if [[ -n "$DEVELOPER_VERSION" ]]; then
  git_safe -C "$MIRROR_DIR" tag "v${DEVELOPER_VERSION#v}" "$COMMIT_SHA"
fi

if [[ -n "$DEVELOPER_RELEASE_DIR" ]]; then
  compose_developer_payload "$COMMIT_SHA"
fi

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

if [[ -n "$REPORT_PATH" ]]; then
  write_report "$REPORT_PATH" "$COMMIT_SHA" \
    "$(git_safe -C "$MIRROR_DIR" rev-parse HEAD^{tree})" "$FILES_CHANGED" "$PUSH_RESULT"
fi

printf '[SUMMARY] branch=%s commit=%s files_changed=%s push=%s remote=%s\n' \
  "$BRANCH" "$COMMIT_SHA" "$FILES_CHANGED" "$PUSH_RESULT" "${REMOTE_URL:-none}"
