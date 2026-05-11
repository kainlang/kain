#[cfg(not(feature = "legacy-egui"))]
mod app;
#[cfg(not(feature = "legacy-egui"))]
mod qt_host;
#[cfg(not(feature = "legacy-egui"))]
mod session;

#[cfg(feature = "legacy-egui")]
#[path = "archive/legacy_egui.rs"]
mod legacy_egui;

#[cfg(not(feature = "legacy-egui"))]
pub use app::*;
#[cfg(feature = "legacy-egui")]
pub use legacy_egui::*;
#[cfg(not(feature = "legacy-egui"))]
pub use session::*;
