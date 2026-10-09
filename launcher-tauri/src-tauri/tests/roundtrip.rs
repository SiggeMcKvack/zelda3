// Phase 2 de-risk test: exercise the full Rust -> C FFI chain end to end,
// proving the static-link + bindgen wiring works and the round-trip is lossless.
use std::path::PathBuf;
use zelda3_launcher_core::{load_config_json, save_config_json, validate_config_json};

fn repo_ini() -> String {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../zelda3.ini")
        .canonicalize()
        .unwrap()
        .to_string_lossy()
        .into_owned()
}

#[test]
fn load_returns_populated_json() {
    let json = load_config_json(&repo_ini()).expect("load_config_json returned None");
    // Sanity: the marshalled JSON has the keymap structure and real values.
    assert!(json.contains("\"kbd\""), "missing kbd object");
    assert!(json.contains("\"gamepad\""), "missing gamepad object");
    assert!(json.contains("\"controls\""), "missing controls array");
    assert!(json.contains("features0"), "missing features0");
}

#[test]
fn validate_accepts_loaded_config() {
    let json = load_config_json(&repo_ini()).unwrap();
    let res = validate_config_json(&json).expect("validate returned None");
    assert!(res.contains("\"ok\":true"), "validate not ok: {res}");
}

#[test]
fn save_then_reload_is_idempotent() {
    let json = load_config_json(&repo_ini()).unwrap();

    let out1 = std::env::temp_dir().join("zelda3_ffi_rs_1.ini");
    let out2 = std::env::temp_dir().join("zelda3_ffi_rs_2.ini");
    let out1s = out1.to_string_lossy().into_owned();
    let out2s = out2.to_string_lossy().into_owned();

    let res = save_config_json(&out1s, &json).expect("save returned None");
    assert!(res.contains("\"ok\":true"), "first save not ok: {res}");

    // Reload the written INI and save again; the two canonical files must match.
    let json2 = load_config_json(&out1s).expect("reload returned None");
    let res2 = save_config_json(&out2s, &json2).unwrap();
    assert!(res2.contains("\"ok\":true"), "second save not ok: {res2}");

    let a = std::fs::read(&out1).unwrap();
    let b = std::fs::read(&out2).unwrap();
    assert_eq!(a, b, "canonical INI differs across a load/save cycle");
    assert!(!a.is_empty(), "written INI is empty");
}

#[test]
fn concurrent_calls_do_not_corrupt() {
    // The C core uses process-global keymap state; the safe wrappers serialize
    // access. Hammer it from several threads to prove there's no heap corruption
    // (this aborts with SIGABRT if the FFI lock is ever removed).
    let ini = repo_ini();
    let handles: Vec<_> = (0..8)
        .map(|_| {
            let ini = ini.clone();
            std::thread::spawn(move || {
                for _ in 0..50 {
                    let json = load_config_json(&ini).expect("load failed");
                    let res = validate_config_json(&json).expect("validate failed");
                    assert!(res.contains("\"ok\":true"));
                }
            })
        })
        .collect();
    for h in handles {
        h.join().unwrap();
    }
}

#[test]
fn scan_msu_folder_groups_tracks_by_prefix() {
    let dir = std::env::temp_dir().join(format!("zelda3-msu-scan-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    for f in [
        "alttp_msu-1.pcm",
        "alttp_msu-2.pcm",
        "alttp_msu-2.opuz",
        "ALTTP_MSU-3.PCM",
        "other-1.opuz",
        "readme.txt",
        "alttp_msu-.pcm",
    ] {
        std::fs::write(dir.join(f), b"").unwrap();
    }
    let v: serde_json::Value = serde_json::from_str(&zelda3_launcher_core::scan_msu_folder(
        dir.to_str().unwrap(),
    ))
    .unwrap();
    std::fs::remove_dir_all(&dir).unwrap();

    assert_eq!(v["ok"], true);
    let prefixes = v["prefixes"].as_array().unwrap();
    assert_eq!(
        prefixes[0],
        serde_json::json!({ "prefix": "alttp_msu-", "pcm": 2, "opuz": 1 })
    );
    assert!(prefixes.contains(&serde_json::json!({ "prefix": "ALTTP_MSU-", "pcm": 1, "opuz": 0 })));
    assert!(prefixes.contains(&serde_json::json!({ "prefix": "other-", "pcm": 0, "opuz": 1 })));
    assert_eq!(
        prefixes.len(),
        3,
        "files without a track number are ignored"
    );
}

#[test]
fn scan_msu_folder_reports_missing_folder() {
    let v: serde_json::Value = serde_json::from_str(&zelda3_launcher_core::scan_msu_folder(
        "/nonexistent/zelda3-msu",
    ))
    .unwrap();
    assert_eq!(v["ok"], false);
}

#[test]
fn tidy_msu_pcm_only_touches_encoded_tracks() {
    for delete in [false, true] {
        let dir =
            std::env::temp_dir().join(format!("zelda3-msu-tidy-{}-{delete}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        // Track 1 encoded; track 2 failed to encode; other prefix untouched.
        for f in [
            "alttp_msu-1.pcm",
            "alttp_msu-1.opuz",
            "alttp_msu-2.pcm",
            "other-1.pcm",
            "other-1.opuz",
        ] {
            std::fs::write(dir.join(f), b"x").unwrap();
        }
        let v: serde_json::Value = serde_json::from_str(&zelda3_launcher_core::tidy_msu_pcm(
            dir.to_str().unwrap(),
            "alttp_msu-",
            delete,
        ))
        .unwrap();
        assert_eq!(v["ok"], true);
        assert_eq!(v["count"], 1);
        assert!(!dir.join("alttp_msu-1.pcm").exists());
        assert_eq!(dir.join("pcm/alttp_msu-1.pcm").exists(), !delete);
        assert!(dir.join("alttp_msu-1.opuz").exists());
        assert!(
            dir.join("alttp_msu-2.pcm").exists(),
            "unencoded track kept in place"
        );
        assert!(dir.join("other-1.pcm").exists(), "other prefix untouched");
        std::fs::remove_dir_all(&dir).unwrap();
    }
}
