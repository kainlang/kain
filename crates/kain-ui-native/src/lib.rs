#[cfg(feature = "legacy-egui")]
mod legacy_egui;
#[cfg(not(feature = "legacy-egui"))]
mod no_egui;

#[cfg(feature = "legacy-egui")]
pub use legacy_egui::*;
#[cfg(not(feature = "legacy-egui"))]
pub use no_egui::*;
