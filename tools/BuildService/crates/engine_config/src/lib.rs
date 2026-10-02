//! engine_config - engine-root resolution from the repository SOT and the
//! source-release receipt gate.
//!
//! Consumed by: Build Service CLI and publisher receipt gate.
//!
//! ONE SOT: engine roots come from `scripts/config/ue_path.conf`
//! (+ `ue_path.local.conf` per-key override) resolved from the configured
//! project path - never from a committed toml key. Grammar and authority
//! model: see the conf header; this parser mirrors
//! `scripts/config/ue_conf.py` / `Resolve-UEConfig.ps1` and is proven
//! against the SAME shared fixture corpus.
//!
//! Source-release gate: publishing consumes only artifacts produced by a
//! gated run. `Receipt::verify` is the ONLY constructor of
//! [`VerifiedReceipt`], and the publisher's promote path requires a
//! `VerifiedReceipt` - there is no "publish existing artifact" path that
//! skips the gate.

use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use thiserror::Error;

pub const KNOWN_KEYS: [&str; 6] = [
    "UE_PATH",
    "UE_SOURCE_PATH",
    "LINUX_MULTIARCH_ROOT",
    "BUILD_TARGET",
    "BUILD_CONFIG",
    "BUILD_PLATFORM",
];

#[derive(Debug, Error)]
pub enum ConfError {
    #[error("{file}:{line}: malformed line (grammar: KEY=VALUE, no spaces around '='): {text}")]
    Malformed { file: String, line: usize, text: String },
    #[error("{file}:{line}: unknown key '{key}'")]
    UnknownKey { file: String, line: usize, key: String },
    #[error("{file}:{line}: duplicate key '{key}'")]
    DuplicateKey { file: String, line: usize, key: String },
    #[error("{file}:{line}: empty value for '{key}'")]
    EmptyValue { file: String, line: usize, key: String },
    #[error("{file}:{line}: forbidden character in value for '{key}'")]
    ForbiddenChar { file: String, line: usize, key: String },
    #[error("io error reading {file}: {source}")]
    Io { file: String, source: std::io::Error },
}

/// Strict single-file parse (grammar SOT: ue_path.conf header).
pub fn parse_conf_file(path: &Path) -> Result<BTreeMap<String, String>, ConfError> {
    let file = path.display().to_string();
    let text = std::fs::read_to_string(path).map_err(|source| ConfError::Io {
        file: file.clone(),
        source,
    })?;
    let mut values = BTreeMap::new();
    for (idx, raw) in text.lines().enumerate() {
        let line = idx + 1;
        let stripped = raw.trim_end_matches('\r');
        let trimmed = stripped.trim();
        if trimmed.is_empty() || trimmed.starts_with('#') {
            continue;
        }
        let Some(eq) = stripped.find('=') else {
            return Err(ConfError::Malformed {
                file,
                line,
                text: stripped.to_string(),
            });
        };
        let key = &stripped[..eq];
        let value = &stripped[eq + 1..];
        let key_ok = !key.is_empty()
            && key.chars().next().unwrap().is_ascii_uppercase()
            && key.chars().all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == '_');
        if !key_ok {
            return Err(ConfError::Malformed {
                file,
                line,
                text: stripped.to_string(),
            });
        }
        if !KNOWN_KEYS.contains(&key) {
            return Err(ConfError::UnknownKey {
                file,
                line,
                key: key.to_string(),
            });
        }
        if values.contains_key(key) {
            return Err(ConfError::DuplicateKey {
                file,
                line,
                key: key.to_string(),
            });
        }
        if value.trim().is_empty() {
            return Err(ConfError::EmptyValue {
                file,
                line,
                key: key.to_string(),
            });
        }
        if value.contains(['#', '$', '"', '\'']) {
            return Err(ConfError::ForbiddenChar {
                file,
                line,
                key: key.to_string(),
            });
        }
        values.insert(key.to_string(), value.trim().to_string());
    }
    Ok(values)
}

/// Key-level merge: `ue_path.local.conf` overrides `ue_path.conf`.
pub fn resolve_conf(config_dir: &Path) -> Result<BTreeMap<String, String>, ConfError> {
    let mut values = BTreeMap::new();
    for name in ["ue_path.conf", "ue_path.local.conf"] {
        let path = config_dir.join(name);
        if path.is_file() {
            values.extend(parse_conf_file(&path)?);
        }
    }
    Ok(values)
}

/// Walk up from any path inside the repo to the checkout root
/// (identified by `Alis.uproject` next to `scripts/config/`).
pub fn find_repo_root(start: &Path) -> Option<PathBuf> {
    let mut cur = Some(start);
    while let Some(dir) = cur {
        if dir.join("scripts/config/ue_path.conf").is_file()
            || dir.join("Alis.uproject").is_file()
        {
            return Some(dir.to_path_buf());
        }
        cur = dir.parent();
    }
    None
}

/// Resolve the SOURCE engine root for release packaging. Missing value
/// fails ONLY this path (per the resolution contract): the error tells
/// the caller to declare it in the conf, never to add a toml key.
pub fn resolve_source_engine_root(project_path: &Path) -> anyhow::Result<PathBuf> {
    let repo = find_repo_root(project_path)
        .ok_or_else(|| anyhow::anyhow!("repo root not found from {}", project_path.display()))?;
    let values = resolve_conf(&repo.join("scripts/config"))?;
    let root = values.get("UE_SOURCE_PATH").ok_or_else(|| {
        anyhow::anyhow!(
            "UE_SOURCE_PATH not set - declare it in scripts/config/ue_path.conf \
             (source-release packaging requires an explicit source engine root; \
             there is no discovery fallback and no toml override)"
        )
    })?;
    Ok(PathBuf::from(root))
}

fn git_out(repo: &Path, args: &[&str]) -> anyhow::Result<String> {
    let out = std::process::Command::new("git")
        .arg("-C")
        .arg(repo)
        .args(args)
        .output()?;
    Ok(String::from_utf8_lossy(&out.stdout).to_string())
}

/// Current HEAD commit of the repo.
pub fn current_commit(repo: &Path) -> anyhow::Result<String> {
    Ok(git_out(repo, &["rev-parse", "HEAD"])?.trim().to_string())
}

/// Workspace fingerprint: HEAD + staged diff + unstaged diff + all
/// untracked non-ignored files (path + content hash). Receipts are
/// issued AND verified with THIS implementation (in-toolchain), so it
/// does not need to byte-match the PowerShell orchestrator fingerprint.
pub fn workspace_fingerprint(repo: &Path) -> anyhow::Result<String> {
    let head = git_out(repo, &["rev-parse", "HEAD"])?;
    let staged = git_out(repo, &["diff", "--cached"])?;
    let unstaged = git_out(repo, &["diff"])?;
    let untracked_list = git_out(repo, &["ls-files", "--others", "--exclude-standard"])?;
    let mut untracked_lines: Vec<String> = Vec::new();
    for path in untracked_list.lines().filter(|l| !l.is_empty()) {
        let hash = git_out(repo, &["hash-object", "--", path])?;
        untracked_lines.push(format!("{}:{}", path, hash.trim()));
    }
    untracked_lines.sort();
    let mut hasher = Sha256::new();
    hasher.update(head.as_bytes());
    hasher.update(staged.as_bytes());
    hasher.update(unstaged.as_bytes());
    hasher.update(untracked_lines.join("\n").as_bytes());
    Ok(format!("{:x}", hasher.finalize()))
}

/// File name of the receipt inside a build's staging directory. The
/// receipt is excluded from its own artifact coverage (manifest-protocol
/// style: no self-hash).
pub const RECEIPT_FILE_NAME: &str = "source_release_receipt.json";

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct BuildVersion {
    #[serde(rename = "MajorVersion")]
    pub major: u32,
    #[serde(rename = "MinorVersion")]
    pub minor: u32,
    #[serde(rename = "PatchVersion")]
    pub patch: u32,
    #[serde(rename = "Changelist", default)]
    pub changelist: u64,
    #[serde(rename = "BranchName", default)]
    pub branch_name: String,
}

impl BuildVersion {
    pub fn line(&self) -> String {
        format!("{}.{}", self.major, self.minor)
    }

    pub fn build_id(&self) -> String {
        if self.branch_name.is_empty() {
            format!("UE{}-CL-{}", self.line(), self.changelist)
        } else {
            format!("{}-CL-{}", self.branch_name, self.changelist)
        }
    }
}

/// Authoritative engine version: Engine/Build/Build.version.
pub fn read_build_version(engine_root: &Path) -> anyhow::Result<BuildVersion> {
    let path = engine_root.join("Engine/Build/Build.version");
    let text = std::fs::read_to_string(&path).map_err(|e| {
        anyhow::anyhow!("not an engine root (no {}): {}", path.display(), e)
    })?;
    Ok(serde_json::from_str(&text)?)
}

/// Engine-identity gate for source release: the source engine's
/// Major.Minor must equal the project's EngineAssociation. Runs BEFORE
/// any expensive cook (code-enforced release freeze).
pub fn check_source_engine_identity(
    source_root: &Path,
    uproject_path: &Path,
) -> anyhow::Result<BuildVersion> {
    let version = read_build_version(source_root)?;
    let uproject = std::fs::read_to_string(uproject_path)?;
    let assoc = uproject
        .split("\"EngineAssociation\"")
        .nth(1)
        .and_then(|rest| rest.split('"').nth(1))
        .ok_or_else(|| anyhow::anyhow!("no EngineAssociation in {}", uproject_path.display()))?
        .to_string();
    if assoc != version.line() {
        anyhow::bail!(
            "source engine line {} does not match project EngineAssociation {} \
             - source release stays FROZEN until the source engine is re-homed \
             (Major.Minor hard fail; see docs/ue_engine/version_update.md)",
            version.line(),
            assoc
        );
    }
    Ok(version)
}

/// Source-release receipt: binds published artifacts to the exact gated
/// inputs. Any input change invalidates it.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Receipt {
    pub repo_commit: String,
    pub worktree_fingerprint: String,
    pub uproject_sha256: String,
    pub source_root: String,
    pub source_build_version: BuildVersion,
    pub package_config: String,
    /// artifact file name -> sha256
    pub artifact_hashes: BTreeMap<String, String>,
}

/// Proof that a [`Receipt`] was verified against the CURRENT inputs.
/// Private field = only [`Receipt::verify`] can construct it; the
/// publisher's promote path requires it, making the gate non-bypassable.
pub struct VerifiedReceipt {
    receipt: Receipt,
    _sealed: (),
}

impl VerifiedReceipt {
    pub fn receipt(&self) -> &Receipt {
        &self.receipt
    }
}

pub fn sha256_file(path: &Path) -> anyhow::Result<String> {
    let bytes = std::fs::read(path)?;
    let mut hasher = Sha256::new();
    hasher.update(&bytes);
    Ok(format!("{:x}", hasher.finalize()))
}

impl Receipt {
    /// Issue a receipt for a gate run that just passed.
    pub fn issue(
        repo_commit: String,
        worktree_fingerprint: String,
        uproject_path: &Path,
        source_root: &Path,
        package_config: String,
        artifacts: &[PathBuf],
    ) -> anyhow::Result<Receipt> {
        let mut artifact_hashes = BTreeMap::new();
        for a in artifacts {
            let name = a
                .file_name()
                .ok_or_else(|| anyhow::anyhow!("artifact without file name: {}", a.display()))?
                .to_string_lossy()
                .to_string();
            artifact_hashes.insert(name, sha256_file(a)?);
        }
        Ok(Receipt {
            repo_commit,
            worktree_fingerprint,
            uproject_sha256: sha256_file(uproject_path)?,
            source_build_version: read_build_version(source_root)?,
            source_root: source_root.display().to_string(),
            package_config,
            artifact_hashes,
        })
    }

    /// Verify against CURRENT inputs; the only path to a
    /// [`VerifiedReceipt`]. Fails on any drift.
    pub fn verify(
        self,
        current_commit: &str,
        current_worktree_fingerprint: &str,
        uproject_path: &Path,
        artifacts: &[PathBuf],
    ) -> anyhow::Result<VerifiedReceipt> {
        if self.repo_commit != current_commit {
            anyhow::bail!(
                "receipt repo commit {} != current {} - rerun the source gate",
                self.repo_commit,
                current_commit
            );
        }
        if self.worktree_fingerprint != current_worktree_fingerprint {
            anyhow::bail!("worktree changed since the source gate - rerun it");
        }
        let uproj = sha256_file(uproject_path)?;
        if self.uproject_sha256 != uproj {
            anyhow::bail!("Alis.uproject changed since the source gate - rerun it");
        }
        for a in artifacts {
            let name = a
                .file_name()
                .ok_or_else(|| anyhow::anyhow!("artifact without file name: {}", a.display()))?
                .to_string_lossy()
                .to_string();
            let expected = self.artifact_hashes.get(&name).ok_or_else(|| {
                anyhow::anyhow!("artifact '{}' is not covered by the receipt", name)
            })?;
            let actual = sha256_file(a)?;
            if &actual != expected {
                anyhow::bail!(
                    "artifact '{}' hash mismatch (receipt {}, actual {})",
                    name,
                    expected,
                    actual
                );
            }
        }
        Ok(VerifiedReceipt {
            receipt: self,
            _sealed: (),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    /// Shared fixture corpus (same cases as ps1/bat/py/sh runners).
    fn fixtures_dir() -> PathBuf {
        let mut p = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        for _ in 0..4 {
            p = p.parent().unwrap().to_path_buf();
        }
        p.join("scripts/config/test/fixtures")
    }

    #[test]
    fn shared_fixture_corpus_conformance() {
        let root = fixtures_dir();
        assert!(root.is_dir(), "fixture corpus missing: {}", root.display());
        let mut ran = 0;
        for entry in fs::read_dir(&root).unwrap() {
            let case = entry.unwrap().path();
            if !case.is_dir() {
                continue;
            }
            let expected = fs::read_to_string(case.join("expected.txt")).unwrap();
            let result = resolve_conf(&case);
            if expected.trim_start().starts_with("ERROR") {
                assert!(result.is_err(), "case {:?} should error", case);
            } else {
                let values = result.unwrap_or_else(|e| panic!("case {:?}: {}", case, e));
                for line in expected.lines() {
                    if let Some((k, v)) = line.split_once('=') {
                        assert_eq!(
                            values.get(k).map(String::as_str),
                            Some(v),
                            "case {:?} key {}",
                            case,
                            k
                        );
                    }
                }
            }
            ran += 1;
        }
        assert!(ran >= 10, "expected the full corpus, ran {}", ran);
    }

    fn fake_engine(dir: &Path, major: u32, minor: u32, patch: u32) {
        let build = dir.join("Engine/Build");
        fs::create_dir_all(&build).unwrap();
        fs::write(
            build.join("Build.version"),
            format!(
                "{{\"MajorVersion\":{},\"MinorVersion\":{},\"PatchVersion\":{},\"Changelist\":123,\"BranchName\":\"++UE{}+Release-{}.{}\"}}",
                major, minor, patch, major, major, minor
            ),
        )
        .unwrap();
    }

    #[test]
    fn source_identity_gate_blocks_line_mismatch() {
        let tmp = tempfile::tempdir().unwrap();
        let eng = tmp.path().join("eng");
        fake_engine(&eng, 5, 8, 1);
        let uproject = tmp.path().join("Alis.uproject");
        fs::write(&uproject, "{ \"EngineAssociation\": \"5.7\" }").unwrap();
        let err = check_source_engine_identity(&eng, &uproject).unwrap_err();
        assert!(err.to_string().contains("FROZEN"), "{}", err);

        fs::write(&uproject, "{ \"EngineAssociation\": \"5.8\" }").unwrap();
        let v = check_source_engine_identity(&eng, &uproject).unwrap();
        assert_eq!(v.line(), "5.8");
        assert_eq!(v.build_id(), "++UE5+Release-5.8-CL-123");
    }

    #[test]
    fn missing_source_path_fails_only_with_clear_message() {
        let tmp = tempfile::tempdir().unwrap();
        let cfg = tmp.path().join("scripts/config");
        fs::create_dir_all(&cfg).unwrap();
        fs::write(cfg.join("ue_path.conf"), "UE_PATH=C:/TestEngine/UE_X\n").unwrap();
        fs::write(tmp.path().join("Alis.uproject"), "{}").unwrap();
        let err = resolve_source_engine_root(tmp.path()).unwrap_err();
        assert!(err.to_string().contains("UE_SOURCE_PATH not set"));
        assert!(err.to_string().contains("no toml override"));
    }

    #[test]
    fn receipt_issue_verify_and_drift_detection() {
        let tmp = tempfile::tempdir().unwrap();
        let eng = tmp.path().join("eng");
        fake_engine(&eng, 5, 8, 1);
        let uproject = tmp.path().join("Alis.uproject");
        fs::write(&uproject, "{ \"EngineAssociation\": \"5.8\" }").unwrap();
        let art = tmp.path().join("ALIS_Win64.zip.001");
        fs::write(&art, b"payload").unwrap();

        let receipt = Receipt::issue(
            "abc123".into(),
            "fp-1".into(),
            &uproject,
            &eng,
            "Shipping/Win64".into(),
            &[art.clone()],
        )
        .unwrap();

        // Happy path -> VerifiedReceipt (sole constructor)
        let verified = receipt
            .clone()
            .verify("abc123", "fp-1", &uproject, &[art.clone()])
            .unwrap();
        assert_eq!(verified.receipt().package_config, "Shipping/Win64");

        // Any input drift invalidates
        assert!(receipt
            .clone()
            .verify("other", "fp-1", &uproject, &[art.clone()])
            .is_err());
        assert!(receipt
            .clone()
            .verify("abc123", "fp-CHANGED", &uproject, &[art.clone()])
            .is_err());
        fs::write(&art, b"tampered").unwrap();
        assert!(receipt.verify("abc123", "fp-1", &uproject, &[art]).is_err());
    }
}
