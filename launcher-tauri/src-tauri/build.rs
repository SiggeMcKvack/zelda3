// Build script for the Tauri launcher backend.
//
// 1. Drives CMake to build the GTK-free `launcher_core_lib` static library
//    (and its restool_lib dependency) out of the parent C/CMake project.
// 2. Emits the link directives so cargo links those static libs plus the
//    dynamic deps they need (libyaml, libm).
// 3. Runs bindgen over launcher_ffi.h to generate the Rust FFI bindings.
use std::env;
use std::path::{Path, PathBuf};

fn main() {
    // Generate the Tauri context (reads tauri.conf.json, capabilities, frontend).
    tauri_build::build();

    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let repo_root = manifest.join("../..").canonicalize().unwrap();

    // On macOS, make Homebrew's pkg-config metadata (libyaml, etc.) visible.
    // `brew --prefix` is /opt/homebrew on Apple Silicon, /usr/local on Intel.
    if cfg!(target_os = "macos") {
        if let Some(brew) = brew_prefix() {
            let prev = env::var("PKG_CONFIG_PATH").unwrap_or_default();
            env::set_var("PKG_CONFIG_PATH", format!("{brew}/lib/pkgconfig:{prev}"));
            // Prefer Homebrew LLVM's libclang for bindgen if present.
            let brew_llvm = format!("{brew}/opt/llvm/lib");
            if env::var_os("LIBCLANG_PATH").is_none()
                && PathBuf::from(&brew_llvm).join("libclang.dylib").exists()
            {
                env::set_var("LIBCLANG_PATH", brew_llvm);
            }
        }
    }

    // --- 1. Build launcher_core_lib via CMake ---------------------------------
    // CI builds the whole C project once (with its own toolchain/vcpkg setup)
    // and points ZELDA3_CMAKE_BUILD_DIR at it; dev builds drive CMake here.
    println!("cargo:rerun-if-env-changed=ZELDA3_CMAKE_BUILD_DIR");
    let build = match env::var_os("ZELDA3_CMAKE_BUILD_DIR") {
        Some(dir) => PathBuf::from(dir),
        None => cmake::Config::new(&repo_root)
            .build_target("launcher_core_lib")
            .build()
            .join("build"),
    };

    // --- 2. Link directives ---------------------------------------------------
    link_search(&build.join("src/launcher_core"));
    link_search(&build.join("src/restool"));
    println!("cargo:rustc-link-lib=static=launcher_core_lib");
    println!("cargo:rustc-link-lib=static=restool_lib");

    // restool_lib's transitive deps (it links ${YAML_LIBRARIES} + m).
    pkg_config::Config::new()
        .probe("yaml-0.1")
        .expect("libyaml (yaml-0.1) not found via pkg-config");
    if !cfg!(target_os = "windows") {
        println!("cargo:rustc-link-lib=dylib=m");
    }

    // Optional opus encoder: linked only when CMake actually built it (i.e. the
    // launcher_core lib was compiled with HAVE_OPUS_ENCODER). Matches the C side.
    let opus_dir = build.join("src/opus_encoder");
    let opus_built = [
        "libopus_encoder_lib.a",
        "opus_encoder_lib.lib",
        "Release/opus_encoder_lib.lib",
        "Debug/opus_encoder_lib.lib",
    ]
    .iter()
    .any(|f| opus_dir.join(f).exists());
    if opus_built {
        link_search(&opus_dir);
        println!("cargo:rustc-link-lib=static=opus_encoder_lib");
        pkg_config::Config::new()
            .probe("opus")
            .expect("libopus not found via pkg-config");
        pkg_config::Config::new()
            .probe("samplerate")
            .expect("libsamplerate not found via pkg-config");
    }

    // --- 3. Generate FFI bindings ---------------------------------------------
    let header = repo_root.join("src/launcher_core/launcher_ffi.h");
    let bindings = bindgen::Builder::default()
        .header(header.to_string_lossy())
        .allowlist_function("LauncherFfi_.*")
        .generate()
        .expect("bindgen failed to generate launcher_ffi bindings");
    bindings
        .write_to_file(PathBuf::from(env::var("OUT_DIR").unwrap()).join("bindings.rs"))
        .expect("failed to write bindings.rs");

    println!("cargo:rerun-if-changed={}", header.display());
    println!(
        "cargo:rerun-if-changed={}",
        repo_root.join("src/launcher_core/launcher_ffi.c").display()
    );
}

/// Add a native link search path, plus MSVC's per-config output subdirs.
fn link_search(dir: &Path) {
    for d in [dir.to_path_buf(), dir.join("Release"), dir.join("Debug")] {
        if d.exists() {
            println!("cargo:rustc-link-search=native={}", d.display());
        }
    }
}

fn brew_prefix() -> Option<String> {
    let out = std::process::Command::new("brew")
        .arg("--prefix")
        .output()
        .ok()?;
    out.status
        .success()
        .then(|| String::from_utf8_lossy(&out.stdout).trim().to_string())
}
