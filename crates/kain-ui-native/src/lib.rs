#[cfg(feature = "legacy-egui")]
mod legacy_egui;
#[cfg(not(feature = "legacy-egui"))]
mod no_egui;
#[cfg(not(feature = "legacy-egui"))]
mod no_egui_qt_host;
#[cfg(not(feature = "legacy-egui"))]
mod no_egui_session;

#[cfg(feature = "legacy-egui")]
pub use legacy_egui::*;
#[cfg(not(feature = "legacy-egui"))]
pub use no_egui::*;
