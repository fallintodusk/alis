//! JSON schema validation for manifest

use anyhow::{Context, Result};
use jsonschema::{Draft, JSONSchema};
use serde_json::Value;

/// Schema validator for manifest
pub struct SchemaValidator {
    schema: JSONSchema,
}

impl SchemaValidator {
    /// Create new validator with bootloader manifest schema
    ///
    /// The embedded schema is the local validation authority. Compatibility
    /// with the sibling CDN must be checked at that repository boundary.
    pub fn new() -> Result<Self> {
        // ALIS immutable bootloader manifest schema.
        let schema_json = serde_json::json!({
            "$schema": "http://json-schema.org/draft-07/schema#",
            "$id": "https://alis.game/schemas/bootloader-manifest.json",
            "title": "ALIS Immutable Bootloader Manifest",
            "description": "Schema for the bootloader manifest used by Launcher and Orchestrator to manage plugin updates",
            "type": "object",
            "required": ["manifest_version", "engine_build_id", "plugins"],
            "additionalProperties": false,
            "properties": {
                "manifest_version": {
                    "type": "integer",
                    "description": "Manifest schema version (currently 1)",
                    "const": 1
                },
                "engine_build_id": {
                    "type": "string",
                    "description": "Engine build identifier - all plugins must match this (e.g., 'UE5.5-CL-123456')",
                    "minLength": 1
                },
                "signed_at": {
                    "type": "string",
                    "description": "ISO 8601 timestamp when manifest was signed",
                    "format": "date-time"
                },
                "signing_key_id": {
                    "type": "string",
                    "description": "Identifier for the signing key used (e.g., 'alis-release-2025')",
                    "minLength": 1
                },
                "signature": {
                    "type": "string",
                    "description": "Base64-encoded manifest signature (verified by Launcher)",
                    "minLength": 1
                },
                "plugins": {
                    "type": "array",
                    "description": "Array of plugin update records",
                    "items": {
                        "$ref": "#/definitions/plugin"
                    }
                }
            },
            "definitions": {
                "plugin": {
                    "type": "object",
                    "required": ["uuid", "name", "version", "platform", "code", "assets"],
                    "additionalProperties": false,
                    "properties": {
                        "uuid": {
                            "type": "string",
                            "description": "Unique plugin UUID (RFC 4122 format). Used for storage paths and CDN routing. Stable across renames.",
                            "pattern": "^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$"
                        },
                        "name": {
                            "type": "string",
                            "description": "Human-readable plugin name (e.g., 'Orchestrator', 'InventoryGF'). Used for display only.",
                            "minLength": 1,
                            "pattern": "^[A-Za-z0-9_]+$"
                        },
                        "version": {
                            "type": "string",
                            "description": "Plugin version (semver or monotonic, e.g., '1.4.0' or '2025.10.31.1')",
                            "minLength": 1,
                            "pattern": "^\\d+(?:\\.\\d+)*$"
                        },
                        "module": {
                            "type": ["string", "null"],
                            "description": "UE module name to load (e.g., 'OrchestratorCore'). Null for content-only plugins.",
                            "minLength": 1
                        },
                        "platform": {
                            "type": "string",
                            "description": "Target platform for this build",
                            "enum": ["Windows", "Linux", "Mac"]
                        },
                        "code": {
                            "$ref": "#/definitions/artifact",
                            "description": "Code artifact (.uplugin + Binaries/*)"
                        },
                        "assets": {
                            "type": "array",
                            "description": "Content artifacts (IoStore files) - must contain exactly one utoc and one ucas, optionally one pak",
                            "minItems": 2,
                            "maxItems": 3,
                            "items": {
                                "$ref": "#/definitions/artifact"
                            },
                            "allOf": [
                                {
                                    "contains": {
                                        "type": "object",
                                        "required": ["role"],
                                        "properties": {
                                            "role": { "const": "utoc" }
                                        }
                                    }
                                },
                                {
                                    "contains": {
                                        "type": "object",
                                        "required": ["role"],
                                        "properties": {
                                            "role": { "const": "ucas" }
                                        }
                                    }
                                }
                            ]
                        },
                        "depends_on": {
                            "type": "array",
                            "description": "Dependency constraints (minimum version only, >=X.Y.Z)",
                            "default": [],
                            "items": {
                                "$ref": "#/definitions/dependency"
                            }
                        },
                        "channel": {
                            "type": "string",
                            "description": "Release channel",
                            "enum": ["stable", "optional", "experimental"],
                            "default": "stable"
                        },
                        "signature_thumbprint": {
                            "type": "string",
                            "description": "Authenticode certificate thumbprint (hex) for DLL validation",
                            "pattern": "^[a-fA-F0-9]+$"
                        },
                        "release_notes": {
                            "type": "string",
                            "description": "Human-readable release notes"
                        },
                        "mirrors": {
                            "type": "array",
                            "description": "Mirror URLs for download fallback",
                            "items": {
                                "type": "string",
                                "format": "uri"
                            }
                        }
                    }
                },
                "artifact": {
                    "type": "object",
                    "required": ["url", "hash", "size"],
                    "additionalProperties": false,
                    "properties": {
                        "url": {
                            "type": "string",
                            "description": "Download URL for this artifact",
                            "format": "uri"
                        },
                        "hash": {
                            "type": "string",
                            "description": "SHA-256 hash of artifact (hex string)",
                            "pattern": "^[a-fA-F0-9]{64}$"
                        },
                        "size": {
                            "type": "integer",
                            "description": "Size of artifact in bytes",
                            "minimum": 1
                        },
                        "role": {
                            "type": "string",
                            "description": "Optional role identifier for asset (e.g., 'utoc', 'ucas', 'pak')",
                            "enum": ["utoc", "ucas", "pak"]
                        }
                    }
                },
                "dependency": {
                    "type": "object",
                    "required": ["name", "version"],
                    "additionalProperties": false,
                    "properties": {
                        "name": {
                            "type": "string",
                            "description": "Dependency plugin name",
                            "minLength": 1
                        },
                        "version": {
                            "type": "string",
                            "description": "Minimum version constraint (>=X.Y.Z format)",
                            "pattern": "^>=\\d+\\.\\d+\\.\\d+$"
                        }
                    }
                }
            }
        });

        // Box and leak to get 'static reference (acceptable for singleton validator)
        let schema_json: &'static Value = Box::leak(Box::new(schema_json));

        let schema = JSONSchema::options()
            .with_draft(Draft::Draft7)
            .compile(schema_json)
            .context("Failed to compile manifest schema")?;

        Ok(Self { schema })
    }

    /// Validate manifest against schema
    pub fn validate(&self, manifest: &Value) -> Result<()> {
        let result = self.schema.validate(manifest);

        if let Err(errors) = result {
            let error_messages: Vec<String> = errors
                .map(|error| format!("{}: {}", error.instance_path, error))
                .collect();

            return Err(anyhow::anyhow!(
                "Manifest validation failed:\n{}",
                error_messages.join("\n")
            ));
        }

        tracing::info!("Manifest validation passed");
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_validator_creation() {
        let validator = SchemaValidator::new();
        assert!(validator.is_ok());
    }

    #[test]
    fn test_validate_valid_manifest() {
        let validator = SchemaValidator::new().unwrap();

        let manifest = serde_json::json!({
            "manifest_version": 1,
            "engine_build_id": "UE5.5-CL-123456",
            "plugins": [
                {
                    "uuid": "550e8400-e29b-41d4-a716-446655440000",
                    "name": "TestPlugin",
                    "version": "1.0.0",
                    "platform": "Windows",
                    "code": {
                        "url": "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440000/1.0.0/code.zip",
                        "hash": "a".repeat(64),
                        "size": 1024
                    },
                    "assets": [
                        {
                            "url": "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440000/1.0.0/content.utoc",
                            "hash": "b".repeat(64),
                            "size": 2048,
                            "role": "utoc"
                        },
                        {
                            "url": "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440000/1.0.0/content.ucas",
                            "hash": "c".repeat(64),
                            "size": 4096,
                            "role": "ucas"
                        }
                    ],
                    "channel": "stable"
                }
            ]
        });

        let result = validator.validate(&manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_invalid_manifest() {
        let validator = SchemaValidator::new().unwrap();

        let manifest = serde_json::json!({
            "manifest_version": 1,
            "engine_build_id": "UE5.5-CL-123456",
            "plugins": [
                {
                    "name": "TestPlugin",
                    "code_hash": "invalid",  // Old schema - missing required fields
                    "channel": "stable"
                }
            ]
        });

        let result = validator.validate(&manifest);
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_manifest_with_pak() {
        let validator = SchemaValidator::new().unwrap();

        // Manifest with 3 assets: pak + utoc + ucas
        let manifest = serde_json::json!({
            "manifest_version": 1,
            "engine_build_id": "UE5.5-CL-123456",
            "plugins": [
                {
                    "uuid": "550e8400-e29b-41d4-a716-446655440000",
                    "name": "TestPlugin",
                    "version": "1.0.0",
                    "platform": "Windows",
                    "code": {
                        "url": "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440000/1.0.0/code.zip",
                        "hash": "a".repeat(64),
                        "size": 1024
                    },
                    "assets": [
                        {
                            "url": "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440000/1.0.0/pakchunk0-Windows.pak",
                            "hash": "a".repeat(64),
                            "size": 104857600,
                            "role": "pak"
                        },
                        {
                            "url": "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440000/1.0.0/content.utoc",
                            "hash": "b".repeat(64),
                            "size": 2048,
                            "role": "utoc"
                        },
                        {
                            "url": "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440000/1.0.0/content.ucas",
                            "hash": "c".repeat(64),
                            "size": 4096,
                            "role": "ucas"
                        }
                    ],
                    "channel": "stable"
                }
            ]
        });

        let result = validator.validate(&manifest);
        assert!(result.is_ok(), "Schema should accept 3 assets with optional pak");
    }
}
