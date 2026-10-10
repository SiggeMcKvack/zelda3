// Tauri commands exposed to the frontend. Each is a thin wrapper that resolves
// the per-user zelda3.ini path and calls into the launcher_core FFI (which is
// serialized behind CONFIG_LOCK / ASSET_LOCK in lib.rs).
//
// Sync Tauri commands run on the main thread, so anything that calls into the
// FFI is async and runs on a blocking worker: a lock held by a long encode or
// DAT build must never stall the UI thread.
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

/// Run `f` on a blocking worker thread (off both the UI thread and the async runtime).
async fn blocking<T: Send + 'static>(f: impl FnOnce() -> T + Send + 'static) -> Result<T, String> {
    tauri::async_runtime::spawn_blocking(f)
        .await
        .map_err(|e| format!("task join error: {e}"))
}

// Write the default INI to `path` and return the freshly loaded config JSON.
fn write_defaults_and_load(path: &str) -> Result<String, String> {
    let res = ok_or_err(reset_config_defaults(path), "reset")?;
    if !res.contains("\"ok\":true") {
        return Err(res);
    }
    ok_or_err(load_config_json(path), "load after reset")
}

/// Load the current config as a JSON string. Creates a default INI on first run.
#[tauri::command]
pub async fn load_config() -> Result<String, String> {
    let path = ensure_dir()?;
    blocking(move || {
        if !std::path::Path::new(&path).exists() {
            return write_defaults_and_load(&path);
        }
        ok_or_err(load_config_json(&path), "load")
    })
    .await?
}

/// Validate + write the given config JSON to the user's zelda3.ini.
/// Returns the FFI result object string `{"ok":bool,"error":...}`.
#[tauri::command]
pub async fn save_config(config: String) -> Result<String, String> {
    let path = ensure_dir()?;
    blocking(move || ok_or_err(save_config_json(&path, &config), "save")).await?
}

/// Validate a config JSON without writing. Returns `{"ok":bool,"error":...}`.
#[tauri::command]
pub async fn validate_config(config: String) -> Result<String, String> {
    blocking(move || ok_or_err(validate_config_json(&config), "validate")).await?
}

/// After confirmation, reset zelda3.ini to defaults and return the fresh config
/// JSON; returns null if the user cancelled.
#[tauri::command]
pub async fn reset_config(app: tauri::AppHandle) -> Result<Option<String>, String> {
    use tauri_plugin_dialog::{MessageDialogButtons, MessageDialogKind};
    let confirmed = app
        .dialog()
        .message("Replace zelda3.ini with the default settings? Your current settings and key bindings will be lost.")
        .title("Reset to defaults")
        .kind(MessageDialogKind::Warning)
        .buttons(MessageDialogButtons::OkCancelCustom("Reset".into(), "Cancel".into()))
        .blocking_show();
    if !confirmed {
        return Ok(None);
    }
    let path = ensure_dir()?;
    blocking(move || write_defaults_and_load(&path)).await?.map(Some)
}

/// Launch the game (detached). Returns an error string if the binary is missing.
#[tauri::command]
pub fn launch_game() -> Result<(), String> {
    crate::launch_game()
}

// --- Assets (restool) -------------------------------------------------------

/// Open a native file dialog filtered to `extensions` (labelled `filter_name`);
/// returns the absolute path or null.
#[tauri::command]
pub async fn pick_file(
    app: tauri::AppHandle,
    filter_name: String,
    extensions: Vec<String>,
) -> Option<String> {
    let exts: Vec<&str> = extensions.iter().map(String::as_str).collect();
    let picked = app
        .dialog()
        .file()
        .add_filter(filter_name, &exts)
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

/// Count MSU tracks per filename prefix in `folder` (relative = data dir).
#[tauri::command]
pub fn scan_msu_folder(folder: String) -> String {
    crate::scan_msu_folder(&folder)
}

/// Identify a ROM file or `.ips` translation patch (applied to `base_rom`).
/// Returns `{"read","valid","lang_code","lang_name"}`.
#[tauri::command]
pub async fn identify_rom(path: String, base_rom: Option<String>) -> Result<String, String> {
    blocking(move || ok_or_err(crate::identify_rom(&path, base_rom.as_deref()), "identify")).await?
}

/// Encode MSU `.pcm` files in `folder` (matching `prefix`; relative = data dir)
/// to `.opuz`, streaming progress as "opus-progress" events. Runs off the async runtime.
#[tauri::command]
pub async fn encode_opus(
    app: tauri::AppHandle,
    folder: String,
    prefix: String,
) -> Result<String, String> {
    let folder = crate::resolve_data_path(&folder)
        .to_string_lossy()
        .into_owned();
    blocking(move || ok_or_err(crate::encode_msu_to_opus(&folder, &prefix, app), "encode")).await?
}

/// After a successful encode, ask whether to keep the source `.pcm` tracks
/// (moved to a `pcm` subfolder) or delete them. Cancel / closing the dialog
/// leaves them in place. Returns `{"action":"moved"|"deleted"|"none",...}`.
#[tauri::command]
pub async fn finish_opus_encode(
    app: tauri::AppHandle,
    folder: String,
    prefix: String,
    encoded: u32,
) -> Result<String, String> {
    use tauri_plugin_dialog::{MessageDialogButtons, MessageDialogResult};
    const KEEP: &str = "Keep (move to pcm/)";
    const DELETE: &str = "Delete";
    let choice = app
        .dialog()
        .message(format!(
            "{encoded} tracks were encoded to OPUZ.\n\n\
             Keep the original PCM files in a \"pcm\" subfolder, or delete them permanently?"
        ))
        .title("Encoding complete")
        .buttons(MessageDialogButtons::YesNoCancelCustom(
            KEEP.into(),
            DELETE.into(),
            "Cancel".into(),
        ))
        .blocking_show_with_result();
    // Backends report custom buttons either by label or as Yes/No.
    let delete = match choice {
        MessageDialogResult::Yes => false,
        MessageDialogResult::No => true,
        MessageDialogResult::Custom(label) if label == KEEP => false,
        MessageDialogResult::Custom(label) if label == DELETE => true,
        _ => return Ok(r#"{"action":"none"}"#.into()),
    };
    let action = if delete { "deleted" } else { "moved" };
    let result = crate::tidy_msu_pcm(&folder, &prefix, delete);
    Ok(format!(r#"{{"action":"{action}","result":{result}}}"#))
}

/// DAT presence + languages for the user's data dir.
#[tauri::command]
pub async fn dat_status() -> Result<String, String> {
    let dir = data_dir().to_string_lossy().into_owned();
    blocking(move || ok_or_err(crate::dat_status(&dir), "dat_status")).await?
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

    blocking(move || ok_or_err(crate::create_dat(&options), "create_dat")).await?
}
