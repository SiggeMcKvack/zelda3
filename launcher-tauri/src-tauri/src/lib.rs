// Safe Rust wrappers over the launcher_core C FFI surface (see ffi.rs).
//
// The C side owns the zelda3.ini format; these wrappers just move strings
// across the boundary and free what the C side allocated.
pub mod commands;
mod ffi;

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_void};
use std::path::PathBuf;
use std::sync::Mutex;
use tauri::Emitter;

// The C core keeps the keyboard/gamepad mappings in process-global state
// (g_kbd_*/g_gamepad_*), so the FFI is NOT thread-safe. Tauri commands may run
// concurrently, so every call into launcher_core is serialized through this
// lock. Without it, concurrent load/save calls corrupt the heap (SIGABRT).
static FFI_LOCK: Mutex<()> = Mutex::new(());

// Convert a C string the FFI allocated into an owned String, then free it.
unsafe fn take_c_string(ptr: *mut c_char) -> Option<String> {
    if ptr.is_null() {
        return None;
    }
    let s = CStr::from_ptr(ptr).to_string_lossy().into_owned();
    ffi::LauncherFfi_FreeString(ptr);
    Some(s)
}

/// Read the INI at `ini_path` and return its configuration as a JSON string.
/// Returns `None` if the file can't be read/parsed.
pub fn load_config_json(ini_path: &str) -> Option<String> {
    let c_path = CString::new(ini_path).ok()?;
    let _guard = FFI_LOCK.lock().unwrap();
    unsafe { take_c_string(ffi::LauncherFfi_LoadConfigJson(c_path.as_ptr())) }
}

/// Apply `config_json` and write the canonical INI to `ini_path`.
/// Returns the result JSON string `{"ok":bool,"error":...}`.
pub fn save_config_json(ini_path: &str, config_json: &str) -> Option<String> {
    let c_path = CString::new(ini_path).ok()?;
    let c_json = CString::new(config_json).ok()?;
    let _guard = FFI_LOCK.lock().unwrap();
    unsafe {
        take_c_string(ffi::LauncherFfi_SaveConfigJson(
            c_path.as_ptr(),
            c_json.as_ptr(),
        ))
    }
}

/// Validate `config_json` without writing. Returns `{"ok":bool,"error":...}`.
pub fn validate_config_json(config_json: &str) -> Option<String> {
    let c_json = CString::new(config_json).ok()?;
    let _guard = FFI_LOCK.lock().unwrap();
    unsafe { take_c_string(ffi::LauncherFfi_ValidateConfigJson(c_json.as_ptr())) }
}

/// Write the default zelda3.ini to `ini_path`. Returns `{"ok":bool,"error":...}`.
pub fn reset_config_defaults(ini_path: &str) -> Option<String> {
    let c_path = CString::new(ini_path).ok()?;
    let _guard = FFI_LOCK.lock().unwrap();
    unsafe { take_c_string(ffi::LauncherFfi_ResetDefaults(c_path.as_ptr())) }
}

/// Identify a ROM. Returns `{"read":bool,"valid":bool,"lang_code":str,"lang_name":str}`.
pub fn identify_rom(rom_path: &str) -> Option<String> {
    let c_path = CString::new(rom_path).ok()?;
    let _guard = FFI_LOCK.lock().unwrap();
    unsafe { take_c_string(ffi::LauncherFfi_IdentifyRom(c_path.as_ptr())) }
}

/// Create zelda3_assets.dat. `options_json` = `{us_rom_path, output_dir, language_roms[]}`.
/// Returns `{"ok":bool,"error":...,"languages":[...]}`. Long-running (call off-thread).
pub fn create_dat(options_json: &str) -> Option<String> {
    let c_json = CString::new(options_json).ok()?;
    let _guard = FFI_LOCK.lock().unwrap();
    unsafe { take_c_string(ffi::LauncherFfi_CreateDat(c_json.as_ptr())) }
}

/// DAT presence + languages in `dir`. Returns `{"exists":bool,"languages":[...]}`.
pub fn dat_status(dir: &str) -> Option<String> {
    let c_dir = CString::new(dir).ok()?;
    let _guard = FFI_LOCK.lock().unwrap();
    unsafe { take_c_string(ffi::LauncherFfi_DatStatus(c_dir.as_ptr())) }
}

// Context handed to the C progress callback as the `user` pointer.
struct ProgressCtx {
    app: tauri::AppHandle,
}

// C-ABI trampoline: forwards encode progress to the frontend as an "opus-progress"
// event. Runs on the encoding (blocking) thread; AppHandle::emit is thread-safe.
unsafe extern "C" fn opus_progress_trampoline(
    message: *const c_char,
    fraction: f32,
    user: *mut c_void,
) {
    if user.is_null() {
        return;
    }
    let ctx = &*(user as *const ProgressCtx);
    let msg = if message.is_null() {
        String::new()
    } else {
        CStr::from_ptr(message).to_string_lossy().into_owned()
    };
    let _ = ctx.app.emit(
        "opus-progress",
        serde_json::json!({ "message": msg, "fraction": fraction }),
    );
}

/// Encode `<prefix><N>.pcm` files in `folder` to `.opuz`, emitting "opus-progress"
/// events via `app`. Returns `{"ok":bool,"error":...,"encoded":int,"total":int}`.
/// Long-running; call off the main thread.
pub fn encode_msu_to_opus(folder: &str, prefix: &str, app: tauri::AppHandle) -> Option<String> {
    let folder_c = CString::new(folder).ok()?;
    let prefix_c = CString::new(prefix).ok()?;
    let ctx = ProgressCtx { app };
    let _guard = FFI_LOCK.lock().unwrap();
    unsafe {
        take_c_string(ffi::LauncherFfi_EncodeMsuToOpus(
            folder_c.as_ptr(),
            prefix_c.as_ptr(),
            Some(opus_progress_trampoline),
            &ctx as *const ProgressCtx as *mut c_void,
        ))
    }
}

// ----------------------------------------------------------------------------
// Per-user data directory (matches the migration plan: one location on every OS,
// so installed packages in read-only paths still work).
//   macOS:   ~/Library/Application Support/Zelda3
//   Linux:   $XDG_CONFIG_HOME/zelda3  (default ~/.config/zelda3)
//   Windows: %APPDATA%\Zelda3
// ----------------------------------------------------------------------------

/// Directory holding zelda3.ini (and, later, the DAT/saves).
pub fn data_dir() -> PathBuf {
    #[cfg(target_os = "macos")]
    {
        let home = std::env::var("HOME").unwrap_or_else(|_| ".".into());
        PathBuf::from(home).join("Library/Application Support/Zelda3")
    }
    #[cfg(target_os = "linux")]
    {
        let base = std::env::var("XDG_CONFIG_HOME")
            .ok()
            .filter(|s| !s.is_empty())
            .unwrap_or_else(|| {
                format!(
                    "{}/.config",
                    std::env::var("HOME").unwrap_or_else(|_| ".".into())
                )
            });
        PathBuf::from(base).join("zelda3")
    }
    #[cfg(target_os = "windows")]
    {
        let base = std::env::var("APPDATA").unwrap_or_else(|_| ".".into());
        PathBuf::from(base).join("Zelda3")
    }
}

/// Full path to the launcher-managed zelda3.ini.
pub fn config_path() -> PathBuf {
    data_dir().join("zelda3.ini")
}

/// Launch the game binary that ships next to the launcher, detached, with its
/// working directory set to the per-user data dir (so it reads the same
/// zelda3.ini / zelda3_assets.dat the launcher manages). setsid on Unix,
/// DETACHED_PROCESS on Windows.
pub fn launch_game() -> Result<(), String> {
    use std::process::{Command, Stdio};

    let exe = std::env::current_exe().map_err(|e| e.to_string())?;
    let dir = exe.parent().ok_or("cannot resolve launcher directory")?;

    // Bundles ship the game as the `zelda3-game` sidecar (externalBin); a dev
    // build has the plain CMake `zelda3` binary copied next to the launcher.
    let exe_suffix = std::env::consts::EXE_SUFFIX;
    let game = ["zelda3-game", "zelda3"]
        .iter()
        .map(|name| dir.join(format!("{name}{exe_suffix}")))
        .find(|p| p.exists())
        .ok_or_else(|| format!("game executable not found in {}", dir.display()))?;

    let data = data_dir();
    let _ = std::fs::create_dir_all(&data);

    let mut cmd = Command::new(&game);
    cmd.current_dir(&data)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null());

    #[cfg(unix)]
    {
        use std::os::unix::process::CommandExt;
        // New session so the game survives the launcher exiting.
        unsafe {
            cmd.pre_exec(|| {
                libc::setsid();
                Ok(())
            });
        }
    }
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        const DETACHED_PROCESS: u32 = 0x0000_0008;
        const CREATE_NEW_PROCESS_GROUP: u32 = 0x0000_0200;
        cmd.creation_flags(DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP);
    }

    cmd.spawn()
        .map_err(|e| format!("failed to launch game: {e}"))?;
    Ok(())
}

/// Tauri application entry point.
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .invoke_handler(tauri::generate_handler![
            commands::load_config,
            commands::save_config,
            commands::validate_config,
            commands::reset_config,
            commands::pick_file,
            commands::pick_msu_folder,
            commands::identify_rom,
            commands::create_dat,
            commands::dat_status,
            commands::encode_opus,
            commands::launch_game,
        ])
        .run(tauri::generate_context!())
        .expect("error while running the Zelda3 launcher");
}
