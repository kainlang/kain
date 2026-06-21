#ifndef ABI_UI_LAYOUT_H
#define ABI_UI_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration (actual struct in ui_system_internal.h)
struct KainNativeUiSession;

// ── Layout engine ─────────────────────────────────────────────────────
//
// Walks the node tree and computes pixel positions (x, y, width, height)
// for every node based on parent-child relationships and style rules.
//
// Style keys consumed:
//   "layout.direction"  i64  0=horizontal, 1=vertical (default 1)
//   "padding"           f64  uniform padding on all sides
//   "padding.left"      f64  left padding
//   "padding.top"       f64  top padding
//   "padding.right"     f64  right padding
//   "padding.bottom"    f64  bottom padding
//   "spacing"           f64  gap between children (also read as "gap")
//   "width"             f64  explicit width override
//   "height"            f64  explicit height override
//
// Returns 0 on success, -1 on invalid session.

int64_t ui_layout_resolve(struct KainNativeUiSession* session);

#ifdef __cplusplus
}
#endif

#endif /* ABI_UI_LAYOUT_H */
