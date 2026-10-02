// Pin-based state management integration tests
//
// Tests Scanner → State → Executor workflow with pin-based incremental tracking

use anyhow::Result;
use build_service_state::{
    BuildState, EnginePin, BaseReleasePin, Unit, ScanSnapshot, ScannedUnit, BuiltUnit,
    mark_dirty_from_scan, apply_build_result, save_atomic,
};
use std::collections::{BTreeMap, BTreeSet};
use tempfile::TempDir;

#[test]
fn test_load_or_init_creates_state() -> Result<()> {
    let temp_dir = TempDir::new()?;
    let state_path = temp_dir.path().join("build_state.yaml");

    // State file doesn't exist
    assert!(!state_path.exists());

    // load_or_init should create new state
    let state = BuildState::load_or_init(
        &state_path,
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_TEST".to_string()),
        "Windows",
    )?;

    // Verify state file was created
    assert!(state_path.exists());
    assert_eq!(state.platform, "Windows");
    assert_eq!(state.engine.ue_version, "5.5.0");
    assert_eq!(state.base_release.version, "ALIS_TEST");

    Ok(())
}

#[test]
fn test_load_or_init_loads_existing() -> Result<()> {
    let temp_dir = TempDir::new()?;
    let state_path = temp_dir.path().join("build_state.yaml");

    // Create initial state
    let mut state1 = BuildState::with_pins(
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_ORIGINAL".to_string()),
        "Windows".to_string(),
    );
    save_atomic(&state_path, &mut state1)?;

    // load_or_init should load existing state (not create new)
    let state2 = BuildState::load_or_init(
        &state_path,
        EnginePin::new("different".to_string(), "different".to_string()),
        BaseReleasePin::new("DIFFERENT".to_string()),
        "Linux",
    )?;

    // Should have loaded original state
    assert_eq!(state2.engine.ue_version, "5.5.0");
    assert_eq!(state2.base_release.version, "ALIS_ORIGINAL");
    assert_eq!(state2.platform, "Windows");

    Ok(())
}

#[test]
fn test_engine_pin_change_marks_all_dirty() -> Result<()> {
    let mut state = BuildState::with_pins(
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_TEST".to_string()),
        "Windows".to_string(),
    );

    // Add units (all clean)
    let mut unit1 = Unit::new("code1".to_string(), "content1".to_string(), BTreeSet::new());
    unit1.dirty = false;
    let mut unit2 = Unit::new("code2".to_string(), "content2".to_string(), BTreeSet::new());
    unit2.dirty = false;

    state.units.insert("Plugin1".to_string(), unit1);
    state.units.insert("Plugin2".to_string(), unit2);

    // Create scan snapshot (no changes)
    let mut scan_units = BTreeMap::new();
    scan_units.insert("Plugin1".to_string(), ScannedUnit {
        code_hash: "code1".to_string(),
        content_hash: "content1".to_string(),
        deps: BTreeSet::new(),
    });
    scan_units.insert("Plugin2".to_string(), ScannedUnit {
        code_hash: "code2".to_string(),
        content_hash: "content2".to_string(),
        deps: BTreeSet::new(),
    });
    let scan = ScanSnapshot { units: scan_units };

    // Change engine pin
    let new_engine = EnginePin::new("5.5.1".to_string(), "newhash".to_string());
    let same_base = BaseReleasePin::new("ALIS_TEST".to_string());

    let dirty_count = mark_dirty_from_scan(&mut state, scan, &new_engine, &same_base)?;

    // All units should be marked dirty
    assert_eq!(dirty_count, 2);
    assert!(state.units.get("Plugin1").unwrap().dirty);
    assert!(state.units.get("Plugin2").unwrap().dirty);

    // State pins should be updated
    assert_eq!(state.engine.ue_version, "5.5.1");
    assert_eq!(state.engine.toolchain_hash, "newhash");

    Ok(())
}

#[test]
fn test_base_pin_change_marks_all_dirty() -> Result<()> {
    let mut state = BuildState::with_pins(
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_20251120_100000".to_string()),
        "Windows".to_string(),
    );

    // Add units (all clean)
    let mut unit1 = Unit::new("code1".to_string(), "content1".to_string(), BTreeSet::new());
    unit1.dirty = false;
    state.units.insert("Plugin1".to_string(), unit1);

    // Create scan snapshot
    let mut scan_units = BTreeMap::new();
    scan_units.insert("Plugin1".to_string(), ScannedUnit {
        code_hash: "code1".to_string(),
        content_hash: "content1".to_string(),
        deps: BTreeSet::new(),
    });
    let scan = ScanSnapshot { units: scan_units };

    // Change base pin
    let same_engine = EnginePin::new("5.5.0".to_string(), "hash123".to_string());
    let new_base = BaseReleasePin::new("ALIS_20251120_120000".to_string());

    let dirty_count = mark_dirty_from_scan(&mut state, scan, &same_engine, &new_base)?;

    // All units should be marked dirty
    assert_eq!(dirty_count, 1);
    assert!(state.units.get("Plugin1").unwrap().dirty);

    // Base pin should be updated
    assert_eq!(state.base_release.version, "ALIS_20251120_120000");

    Ok(())
}

#[test]
fn test_code_hash_change_marks_unit_dirty() -> Result<()> {
    let mut state = BuildState::with_pins(
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_TEST".to_string()),
        "Windows".to_string(),
    );

    // Add units (all clean)
    let mut unit1 = Unit::new("code1".to_string(), "content1".to_string(), BTreeSet::new());
    unit1.dirty = false;
    let mut unit2 = Unit::new("code2".to_string(), "content2".to_string(), BTreeSet::new());
    unit2.dirty = false;

    state.units.insert("Plugin1".to_string(), unit1);
    state.units.insert("Plugin2".to_string(), unit2);

    // Create scan snapshot with Plugin1 code changed
    let mut scan_units = BTreeMap::new();
    scan_units.insert("Plugin1".to_string(), ScannedUnit {
        code_hash: "NEW_CODE_HASH".to_string(),  // Changed!
        content_hash: "content1".to_string(),
        deps: BTreeSet::new(),
    });
    scan_units.insert("Plugin2".to_string(), ScannedUnit {
        code_hash: "code2".to_string(),  // Unchanged
        content_hash: "content2".to_string(),
        deps: BTreeSet::new(),
    });
    let scan = ScanSnapshot { units: scan_units };

    // Same pins (no pin change)
    let same_engine = EnginePin::new("5.5.0".to_string(), "hash123".to_string());
    let same_base = BaseReleasePin::new("ALIS_TEST".to_string());

    let dirty_count = mark_dirty_from_scan(&mut state, scan, &same_engine, &same_base)?;

    // Only Plugin1 should be marked dirty
    assert_eq!(dirty_count, 1);
    assert!(state.units.get("Plugin1").unwrap().dirty);
    assert!(!state.units.get("Plugin2").unwrap().dirty);

    Ok(())
}

#[test]
fn test_content_hash_change_marks_unit_dirty() -> Result<()> {
    let mut state = BuildState::with_pins(
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_TEST".to_string()),
        "Windows".to_string(),
    );

    // Add unit (clean)
    let mut unit = Unit::new("code1".to_string(), "content1".to_string(), BTreeSet::new());
    unit.dirty = false;
    state.units.insert("Plugin1".to_string(), unit);

    // Create scan snapshot with content changed
    let mut scan_units = BTreeMap::new();
    scan_units.insert("Plugin1".to_string(), ScannedUnit {
        code_hash: "code1".to_string(),  // Unchanged
        content_hash: "NEW_CONTENT_HASH".to_string(),  // Changed!
        deps: BTreeSet::new(),
    });
    let scan = ScanSnapshot { units: scan_units };

    let same_engine = EnginePin::new("5.5.0".to_string(), "hash123".to_string());
    let same_base = BaseReleasePin::new("ALIS_TEST".to_string());

    let dirty_count = mark_dirty_from_scan(&mut state, scan, &same_engine, &same_base)?;

    // Unit should be marked dirty
    assert_eq!(dirty_count, 1);
    assert!(state.units.get("Plugin1").unwrap().dirty);

    Ok(())
}

#[test]
fn test_dependency_change_marks_unit_dirty() -> Result<()> {
    let mut state = BuildState::with_pins(
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_TEST".to_string()),
        "Windows".to_string(),
    );

    // Add unit with one dependency
    let mut deps = BTreeSet::new();
    deps.insert("DepA".to_string());
    let mut unit = Unit::new("code1".to_string(), "content1".to_string(), deps);
    unit.dirty = false;
    state.units.insert("Plugin1".to_string(), unit);

    // Create scan snapshot with different dependency
    let mut new_deps = BTreeSet::new();
    new_deps.insert("DepB".to_string());  // Changed!

    let mut scan_units = BTreeMap::new();
    scan_units.insert("Plugin1".to_string(), ScannedUnit {
        code_hash: "code1".to_string(),  // Unchanged
        content_hash: "content1".to_string(),  // Unchanged
        deps: new_deps,  // Changed!
    });
    let scan = ScanSnapshot { units: scan_units };

    let same_engine = EnginePin::new("5.5.0".to_string(), "hash123".to_string());
    let same_base = BaseReleasePin::new("ALIS_TEST".to_string());

    let dirty_count = mark_dirty_from_scan(&mut state, scan, &same_engine, &same_base)?;

    // Unit should be marked dirty
    assert_eq!(dirty_count, 1);
    assert!(state.units.get("Plugin1").unwrap().dirty);

    Ok(())
}

#[test]
fn test_no_changes_keeps_units_clean() -> Result<()> {
    let mut state = BuildState::with_pins(
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_TEST".to_string()),
        "Windows".to_string(),
    );

    // Add units (all clean)
    let mut unit1 = Unit::new("code1".to_string(), "content1".to_string(), BTreeSet::new());
    unit1.dirty = false;
    let mut unit2 = Unit::new("code2".to_string(), "content2".to_string(), BTreeSet::new());
    unit2.dirty = false;

    state.units.insert("Plugin1".to_string(), unit1);
    state.units.insert("Plugin2".to_string(), unit2);

    // Create scan snapshot with NO changes
    let mut scan_units = BTreeMap::new();
    scan_units.insert("Plugin1".to_string(), ScannedUnit {
        code_hash: "code1".to_string(),
        content_hash: "content1".to_string(),
        deps: BTreeSet::new(),
    });
    scan_units.insert("Plugin2".to_string(), ScannedUnit {
        code_hash: "code2".to_string(),
        content_hash: "content2".to_string(),
        deps: BTreeSet::new(),
    });
    let scan = ScanSnapshot { units: scan_units };

    let same_engine = EnginePin::new("5.5.0".to_string(), "hash123".to_string());
    let same_base = BaseReleasePin::new("ALIS_TEST".to_string());

    let dirty_count = mark_dirty_from_scan(&mut state, scan, &same_engine, &same_base)?;

    // No units should be marked dirty
    assert_eq!(dirty_count, 0);
    assert!(!state.units.get("Plugin1").unwrap().dirty);
    assert!(!state.units.get("Plugin2").unwrap().dirty);

    Ok(())
}

#[test]
fn test_apply_build_result_clears_dirty() -> Result<()> {
    let mut state = BuildState::with_pins(
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_TEST".to_string()),
        "Windows".to_string(),
    );

    // Add dirty units
    let mut unit1 = Unit::new("code1".to_string(), "content1".to_string(), BTreeSet::new());
    unit1.dirty = true;
    let mut unit2 = Unit::new("code2".to_string(), "content2".to_string(), BTreeSet::new());
    unit2.dirty = true;

    state.units.insert("Plugin1".to_string(), unit1);
    state.units.insert("Plugin2".to_string(), unit2);

    // Apply build result for Plugin1 only
    let results = vec![
        BuiltUnit {
            plugin_name: "Plugin1".to_string(),
            published_version: "ALIS_TEST".to_string(),
            code_hash: Some("code1".to_string()),
            content_hash: Some("content1".to_string()),
        },
    ];

    let cleared_count = apply_build_result(&mut state, results)?;

    // Only Plugin1 should be cleared
    assert_eq!(cleared_count, 1);
    assert!(!state.units.get("Plugin1").unwrap().dirty);
    assert!(state.units.get("Plugin2").unwrap().dirty);  // Still dirty

    Ok(())
}

#[test]
fn test_apply_build_result_updates_hashes() -> Result<()> {
    let mut state = BuildState::with_pins(
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_TEST".to_string()),
        "Windows".to_string(),
    );

    // Add dirty unit
    let mut unit = Unit::new("old_code".to_string(), "old_content".to_string(), BTreeSet::new());
    unit.dirty = true;
    state.units.insert("Plugin1".to_string(), unit);

    // Apply build result with new hashes
    let results = vec![
        BuiltUnit {
            plugin_name: "Plugin1".to_string(),
            published_version: "ALIS_20251120_120000".to_string(),
            code_hash: Some("new_code_hash".to_string()),
            content_hash: Some("new_content_hash".to_string()),
        },
    ];

    apply_build_result(&mut state, results)?;

    // Hashes should be updated
    let unit = state.units.get("Plugin1").unwrap();
    assert_eq!(unit.code_hash, "new_code_hash");
    assert_eq!(unit.content_hash, "new_content_hash");
    assert_eq!(unit.last_published_version, Some("ALIS_20251120_120000".to_string()));
    assert!(unit.last_built_at.is_some());

    Ok(())
}

#[test]
fn test_atomic_save_creates_lock_file() -> Result<()> {
    let temp_dir = TempDir::new()?;
    let state_path = temp_dir.path().join("build_state.yaml");

    let mut state = BuildState::with_pins(
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_TEST".to_string()),
        "Windows".to_string(),
    );

    save_atomic(&state_path, &mut state)?;

    // Lock file should exist
    let lock_path = temp_dir.path().join(".state.lock");
    assert!(lock_path.exists());

    // State file should exist
    assert!(state_path.exists());

    Ok(())
}

#[test]
fn test_atomic_save_windows_safe_overwrite() -> Result<()> {
    let temp_dir = TempDir::new()?;
    let state_path = temp_dir.path().join("build_state.yaml");

    // Save initial state
    let mut state1 = BuildState::with_pins(
        EnginePin::new("5.5.0".to_string(), "hash123".to_string()),
        BaseReleasePin::new("ALIS_TEST".to_string()),
        "Windows".to_string(),
    );
    state1.units.insert("Plugin1".to_string(), Unit::new("hash1".to_string(), "content1".to_string(), BTreeSet::new()));
    save_atomic(&state_path, &mut state1)?;

    // Overwrite with different state
    let mut state2 = BuildState::with_pins(
        EnginePin::new("5.5.1".to_string(), "newhash".to_string()),
        BaseReleasePin::new("ALIS_NEW".to_string()),
        "Windows".to_string(),
    );
    state2.units.insert("Plugin2".to_string(), Unit::new("hash2".to_string(), "content2".to_string(), BTreeSet::new()));
    save_atomic(&state_path, &mut state2)?;

    // Load and verify overwrite succeeded
    let loaded = BuildState::load(&state_path)?;
    assert_eq!(loaded.engine.ue_version, "5.5.1");
    assert_eq!(loaded.base_release.version, "ALIS_NEW");
    assert!(loaded.units.contains_key("Plugin2"));
    assert!(!loaded.units.contains_key("Plugin1"));

    Ok(())
}
