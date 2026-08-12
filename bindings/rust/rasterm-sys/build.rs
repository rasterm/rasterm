/* SPDX-License-Identifier: Apache-2.0 */

use std::{
    env,
    path::{Path, PathBuf},
};

fn contains_library(directory: &Path) -> bool {
    [
        "rasterm.lib",
        "rastermd.lib",
        "librasterm.a",
        "rasterm-import.lib",
    ]
    .iter()
    .any(|name| directory.join(name).is_file())
}

fn local_library_directory() -> Option<PathBuf> {
    let manifest = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR")?);
    let root = manifest.ancestors().nth(3)?;
    [
        root.join("build/bindings/Release"),
        root.join("build/rasterm/Release"),
        root.join("apps/rastermUI/build/rasterm/Release"),
        root.join("apps/examples/build/rasterm/Release"),
        root.join("apps/SimpleNES/build/rasterm/Release"),
        root.join("build/ci/Release"),
        root.join("build/Release"),
        root.join("build/bindings/Debug"),
        root.join("build/rasterm/Debug"),
        root.join("apps/rastermUI/build/rasterm/Debug"),
        root.join("apps/examples/build/rasterm/Debug"),
        root.join("apps/SimpleNES/build/rasterm/Debug"),
        root.join("build/ci/Debug"),
        root.join("build/Debug"),
    ]
    .into_iter()
    .find(|directory| contains_library(directory))
}

fn main() {
    println!("cargo:rerun-if-env-changed=RASTERM_LIB_DIR");
    println!("cargo:rerun-if-env-changed=RASTERM_LIB_NAME");
    println!("cargo:rerun-if-env-changed=RASTERM_LINK_DYNAMIC");

    let directory = env::var_os("RASTERM_LIB_DIR")
        .map(PathBuf::from)
        .or_else(local_library_directory);
    if let Some(directory) = directory {
        println!("cargo:rustc-link-search=native={}", directory.display());
    } else {
        println!("cargo:warning=RASTERM_LIB_DIR is unset and no local rasterm build was found");
    }

    let dynamic = env::var_os("RASTERM_LINK_DYNAMIC").is_some();
    let default_name = if dynamic && cfg!(target_env = "msvc") {
        "rasterm-import"
    } else {
        "rasterm"
    };
    let name = env::var("RASTERM_LIB_NAME").unwrap_or_else(|_| default_name.to_owned());
    println!(
        "cargo:rustc-link-lib={}={name}",
        if dynamic { "dylib" } else { "static" }
    );
}
