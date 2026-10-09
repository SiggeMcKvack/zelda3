// Tauri commands exposed to the frontend. Each is a thin wrapper that resolves
// the per-user zelda3.ini path and calls into the launcher_core FFI (which is
// serialized behind FFI_LOCK in lib.rs).
//
// Config is passed as a JSON *string* both ways; the frontend JSON.parse/stringify
// it. This keeps the backend free of a serde_json dependency.
use crate::{
    config_path, data_dir, load_config_json, reset_config_defaults, save_config_json,
    validate_config_json,
};
use std::fs;
use tauri_plugin_dialog::DialogExt;

// Make sure the data dir exists; returns the zelda3.ini path as a String.
fn ensure_dir() -> Result<String, String> {
    let path = config_path();
    if let Some(dir) = path.parent() {
        fs::create_dir_all(dir).map_err(|e| format!("cannot create data dir: {e}"))?;
    }
    Ok(path.to_string_lossy().into_owned())
}

fn ok_or_err(result: Option<String>, what: &str) -> Result<String, String> {
    match result {
        Some(s) => Ok(s),
        None => Err(format!("{what} failed")),
    }
}

/// Load the current config as a JSON string. Creates a default INI on first run.
#[tauri::command]
pub fn load_config() -> Result<String, String> {
    let path = ensure_dir()?;
    if !std::path::Path::new(&path).exists() {
        let res = ok_or_err(reset_config_defaults(&path), "reset")?;
        if !res.contains("\"ok\":true") {
            return Err(res);
        }
    }
    ok_or_err(load_config_json(&path), "load")
}

/// Validate + write the given config JSON to the user's zelda3.ini.
/// Returns the FFI result object string `{"ok":bool,"error":...}`.
#[tauri::command]
pub fn save_config(config: String) -> Result<String, String> {
    let path = ensure_dir()?;
    ok_or_err(save_config_json(&path, &config), "save")
}

/// Validate a config JSON without writing. Returns `{"ok":bool,"error":...}`.
#[tauri::command]
pub fn validate_config(config: String) -> Result<String, String> {
    ok_or_err(validate_config_json(&config), "validate")
}

/// Reset zelda3.ini to defaults and return the fresh config JSON.
#[tauri::command]
pub fn reset_config() -> Result<String, String> {
    let path = ensure_dir()?;
    let res = ok_or_err(reset_config_defaults(&path), "reset")?;
    if !res.contains("\"ok\":true") {
        return Err(res);
    }
    ok_or_err(load_config_json(&path), "load after reset")
}

/// Launch the game (detached). Returns an error string if the binary is missing.
#[tauri::command]
pub fn launch_game() -> Result<(), String> {
    crate::launch_game()
}

// --- Assets (restool) -------------------------------------------------------

/// Open a native file dialog to pick a ROM; returns the absolute path or null.
#[tauri::command]
pub async fn pick_rom_file(app: tauri::AppHandle) -> Option<String> {
    let picked = app
        .dialog()
        .file()
        .add_filter("SNES ROM", &["sfc", "smc", "fig", "bin"])
        .blocking_pick_file();
    picked
        .and_then(|fp| fp.into_path().ok())
        .map(|p| p.to_string_lossy().into_owned())
}

/// Open a native folder dialog (for the MSU directory); returns the path or null.
#[tauri::command]
pub async fn pick_msu_folder(app: tauri::AppHandle) -> Option<String> {
    app.dialog()
        .file()
        .blocking_pick_folder()
        .and_then(|fp| fp.into_path().ok())
        .map(|p| p.to_string_lossy().into_owned())
}

/// Identify a ROM file. Returns `{"read","valid","lang_code","lang_name"}`.
#[tauri::command]
pub fn identify_rom(path: String) -> Result<String, String> {
    ok_or_err(crate::identify_rom(&path), "identify")
}

/// Encode MSU `.pcm` files in `folder` (matching `prefix`) to `.opuz`, streaming
/// progress as "opus-progress" events. Runs off the async runtime.
#[tauri::command]
pub async fn encode_opus(
    app: tauri::AppHandle,
    folder: String,
    prefix: String,
) -> Result<String, String> {
    tauri::async_runtime::spawn_blocking(move || crate::encode_msu_to_opus(&folder, &prefix, app))
        .await
        .map_err(|e| format!("task join error: {e}"))?
        .ok_or_else(|| "encode failed".to_string())
}

/// DAT presence + languages for the user's data dir.
#[tauri::command]
pub fn dat_status() -> Result<String, String> {
    let dir = data_dir().to_string_lossy().into_owned();
    ok_or_err(crate::dat_status(&dir), "dat_status")
}

/// Build zelda3_assets.dat from a US ROM (+ optional language ROMs) into the data
/// dir. Runs the multi-second compile off the async runtime so the UI stays live.
#[tauri::command]
pub async fn create_dat(us_rom_path: String, language_roms: Vec<String>) -> Result<String, String> {
    let dir = data_dir();
    fs::create_dir_all(&dir).map_err(|e| format!("cannot create data dir: {e}"))?;
    let options = serde_json::json!({
        "us_rom_path": us_rom_path,
        "output_dir": dir.to_string_lossy(),
        "language_roms": language_roms,
    })
    .to_string();

    tauri::async_runtime::spawn_blocking(move || crate::create_dat(&options))
        .await
        .map_err(|e| format!("task join error: {e}"))?
        .ok_or_else(|| "create_dat failed".to_string())
}
