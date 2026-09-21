#!/usr/bin/env sh
set -eu

EXPECTED_FINGERPRINT="3B9885F0C2D8D927C27FAB58F61A530034CFB5E7"
ROOT=${1:-"$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"}
if [ -f "$ROOT/SHA256SUMS.txt" ]; then
    VERIFICATION_PREFIX=""
else
    VERIFICATION_PREFIX="Verification/"
fi
MANIFEST="$ROOT/${VERIFICATION_PREFIX}SHA256SUMS.txt"
SIGNATURE="$ROOT/${VERIFICATION_PREFIX}SHA256SUMS.txt.asc"
PUBLIC_KEY="$ROOT/${VERIFICATION_PREFIX}ALIS_PUBLIC_KEY.asc"

for tool in gpg gpgv sha256sum find sort sed grep cmp mktemp mkdir chmod rm head tr; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "[FAIL] Required verification tool is missing: $tool" >&2
        exit 1
    }
done
for file in "$MANIFEST" "$SIGNATURE" "$PUBLIC_KEY"; do
    [ -f "$file" ] || {
        echo "[FAIL] Required verification file is missing: $file" >&2
        exit 1
    }
done

WORK=$(mktemp -d "${TMPDIR:-/tmp}/alis-verify.XXXXXX")
trap 'rm -rf -- "$WORK"' EXIT HUP INT TERM
GNUPGHOME="$WORK/gnupg"
mkdir "$GNUPGHOME"
chmod 700 "$GNUPGHOME"

FINGERPRINT=$(gpg --homedir "$GNUPGHOME" --batch --show-keys --with-colons "$PUBLIC_KEY" |
    sed -n 's/^fpr:::::::::\([0-9A-Fa-f]*\):$/\1/p' | head -n 1 | tr '[:lower:]' '[:upper:]')
[ "$FINGERPRINT" = "$EXPECTED_FINGERPRINT" ] || {
    echo "[FAIL] ALIS public-key fingerprint mismatch." >&2
    exit 1
}

KEYRING="$WORK/alis-public-keyring.gpg"
gpg --homedir "$GNUPGHOME" --batch --dearmor --yes --output "$KEYRING" "$PUBLIC_KEY"
gpgv --homedir "$GNUPGHOME" --keyring "$KEYRING" "$SIGNATURE" "$MANIFEST"

(
    cd "$ROOT"
    sha256sum --check "${VERIFICATION_PREFIX}SHA256SUMS.txt"
    sed 's/^[0-9a-fA-F]* [ *]//' "${VERIFICATION_PREFIX}SHA256SUMS.txt" | LC_ALL=C sort >"$WORK/expected"
    find . -type f -print | sed 's#^\./##' |
        grep -Fvx -e "${VERIFICATION_PREFIX}SHA256SUMS.txt" \
            -e "${VERIFICATION_PREFIX}SHA256SUMS.txt.asc" |
        LC_ALL=C sort >"$WORK/actual"
    cmp -s "$WORK/expected" "$WORK/actual" || {
        echo "[FAIL] Linux game inventory differs from the signed manifest." >&2
        exit 1
    }
)

echo "[OK] ALIS publisher signature, checksums, and inventory verified."
