#!/usr/bin/env bash
# Complete local CDN test with Build Service
set -e

echo "=== Build Service Local CDN Test ==="
echo ""

# Build ID for this test
BUILD_ID="test-$(date +%Y%m%d_%H%M%S)"
PLUGIN_NAME="ProjectMenuCore"
PLUGIN_UUID="550e8400-e29b-41d4-a716-446655440000"
PLUGIN_VERSION="1.0.0"

echo "Test Configuration:"
echo "  Build ID: $BUILD_ID"
echo "  Plugin: $PLUGIN_NAME"
echo "  UUID: $PLUGIN_UUID"
echo "  Version: $PLUGIN_VERSION"
echo ""

# Create mock staging artifacts (simulates UE build output)
echo "[1/4] Creating mock staging artifacts..."
STAGING_DIR="staging/$BUILD_ID/units/$PLUGIN_UUID/$PLUGIN_VERSION"
mkdir -p "$STAGING_DIR"

# Create mock code.zip
echo "Mock DLL binary content" > "$STAGING_DIR/code.zip"
dd if=/dev/urandom of="$STAGING_DIR/code.zip" bs=1024 count=512 2>/dev/null

# Create mock IoStore files
dd if=/dev/urandom of="$STAGING_DIR/content.utoc" bs=1024 count=4 2>/dev/null
dd if=/dev/urandom of="$STAGING_DIR/content.ucas" bs=1024 count=2048 2>/dev/null

echo "✓ Created artifacts:"
ls -lh "$STAGING_DIR"
echo ""

# Create test manifest
echo "[2/4] Creating test manifest..."
cat > "manifest_test.json" << MANIFESTEOF
{
  "manifest_version": 1,
  "engine_build_id": "$BUILD_ID",
  "plugins": [
    {
      "uuid": "$PLUGIN_UUID",
      "name": "$PLUGIN_NAME",
      "version": "$PLUGIN_VERSION",
      "platform": "Windows",
      "code": {
        "url": "https://cdn.alis.game/units/$PLUGIN_UUID/$PLUGIN_VERSION/code.zip",
        "hash": "$(sha256sum "$STAGING_DIR/code.zip" | awk '{print $1}')",
        "size": $(stat -c%s "$STAGING_DIR/code.zip" 2>/dev/null || stat -f%z "$STAGING_DIR/code.zip")
      },
      "assets": [
        {
          "url": "https://cdn.alis.game/units/$PLUGIN_UUID/$PLUGIN_VERSION/content.utoc",
          "hash": "$(sha256sum "$STAGING_DIR/content.utoc" | awk '{print $1}')",
          "size": $(stat -c%s "$STAGING_DIR/content.utoc" 2>/dev/null || stat -f%z "$STAGING_DIR/content.utoc"),
          "role": "utoc"
        },
        {
          "url": "https://cdn.alis.game/units/$PLUGIN_UUID/$PLUGIN_VERSION/content.ucas",
          "hash": "$(sha256sum "$STAGING_DIR/content.ucas" | awk '{print $1}')",
          "size": $(stat -c%s "$STAGING_DIR/content.ucas" 2>/dev/null || stat -f%z "$STAGING_DIR/content.ucas"),
          "role": "ucas"
        }
      ],
      "depends_on": [],
      "channel": "dev"
    }
  ]
}
MANIFESTEOF

echo "✓ Manifest created: manifest_test.json"
echo ""

# Display what we created
echo "[3/4] Test Artifacts Summary:"
echo "Staging directory structure:"
tree -h "$STAGING_DIR" 2>/dev/null || find "$STAGING_DIR" -type f -exec ls -lh {} \;
echo ""
echo "Manifest preview:"
cat manifest_test.json | head -30
echo ""

# Verify binary
echo "[4/4] Verifying Build Service binary..."
./target/release/build_service.exe --version
echo ""

echo "=== Mock Artifacts Ready ==="
echo ""
echo "Next steps to test with local MinIO:"
echo "1. Set credentials: export S3_ACCESS_KEY=minioadmin S3_SECRET_KEY=minioadmin"
echo "2. Run publish: ./target/release/build_service.exe -c config/local.toml publish --build-id $BUILD_ID --stage"
echo "3. Check MinIO console: http://localhost:9001"
echo ""
echo "Build ID: $BUILD_ID"
