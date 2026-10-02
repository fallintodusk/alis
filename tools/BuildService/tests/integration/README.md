# Integration Tests

Container-based integration tests for Build Service publisher and manifest components.

## Purpose

Integration tests verify that Build Service components work correctly with real external services (Docker containers).

## Scope

- Publisher streaming artifacts to the CDN Control API (staging + promote)
- Publisher calling Control API endpoints
- Manifest validation against CDN schema
- Full publish workflow (artifacts + manifest upload)
- Error handling (auth failures, network errors)

## Speed Target

10-30 seconds for integration test suite (requires running containers).

## Container Requirements

Integration tests require running Docker containers:
- **Object storage (MinIO):** http://localhost:9000
- **nginx (CDN frontend + Control API proxy):** http://localhost (serves `/api` and public downloads)
  - The underlying Control API container should not be exposed directly; nginx forwards `/api/*`.

**Note:** Containers are managed by the CDN repository, not BuildService.

## Running Integration Tests

**Prerequisites:**
1. Ensure Docker containers are running
2. (Optional) Set `TEST_API_TOKEN` if nginx enforces Authorization
**Run tests:**
```bash
# Check if containers are healthy
cargo test test_containers_health -- --ignored

# Run all integration tests
cargo test --test publisher_container_test -- --ignored

# Run specific test
cargo test test_upload_via_control_api -- --ignored --nocapture

# Run full workflow test
cargo test test_full_publish_workflow -- --ignored
```

## Test Organization

Integration tests are located in `tests/integration/` directory.

```
tests/
└── integration/
    ├── README.md                          # This file
    ├── manifest_validation_test.rs        # Manifest schema validation
    └── publisher_container_test.rs        # Publisher with real containers
```

## Available Tests

### publisher_container_test.rs
- `test_containers_health` - Verify containers are accessible
- `test_publisher_initialization` - Publisher initializes against Control API
- `test_upload_via_control_api` - Upload mock artifacts
- `test_upload_manifest` - Upload manifest.json
- `test_full_publish_workflow` - Complete upload workflow
- `test_health_check` - Control API health endpoint
- `test_verify_and_promote` - Promotion workflow
- `test_invalid_credentials` - Auth error handling

### manifest_validation_test.rs
- `test_generated_manifest_validates_against_schema` - Schema compliance
- `test_manifest_urls_match_staging_directory_structure` - Path consistency

## Test Scenarios Covered

Integration tests verify:
1. **Artifact Upload:** Artifacts uploaded with correct canonical paths
2. **Manifest Upload:** manifest.json uploaded to correct location
3. **Health Checks:** Container services are accessible
4. **Auth Handling:** Invalid credentials properly rejected
5. **Full Workflow:** End-to-end publish pipeline completes successfully
6. **Schema Validation:** Generated manifests match CDN schema exactly
7. **Path Consistency:** Staging structure matches manifest URLs

## Environment Variables

Integration tests use:
- `TEST_API_URL` - Control API URL (default: http://localhost/api)
- `TEST_API_TOKEN` - Optional Control API token (if nginx enforces Authorization)

## Troubleshooting

**Tests fail with "connection refused":**
- Check containers are running: `docker ps`
- Test container health: `cargo test test_containers_health -- --ignored`
- Verify nginx endpoint: `curl http://localhost/api/health`
- Verify object storage: `curl http://localhost:9000/minio/health/live`

**Auth errors:**
- Ensure Control API token is valid / header sent
- Check CDN diagnostics via Control API

**Tests are skipped:**
- Container tests use `#[ignore]` attribute
- Must explicitly run with `--ignored` flag

## Related Documentation

- [Build Service Testing](../../docs/testing.md)
- [Tests Overview](../README.md)
