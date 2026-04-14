# Validation: KSculpt And KPainter Parity

**Spec Type:** full  
**Slug:** `ksculpt-kpainter-parity`  
**Created:** 2026-04-14

## Checklist

- [x] Spec level is appropriate for scope and risk
- [x] Required artifacts are present
- [x] Required headings are complete
- [x] Requirements are testable and unambiguous
- [x] Design traces to requirements
- [x] Tasks trace to requirements

## Traceability Checks

- REQ-1 -> Native Desktop Workbench And Reload Loop -> Tasks 2.2, 2.3, 3.2, 7.2
- REQ-2 -> Shared DCC Contract Layer -> Tasks 1.1, 3.1, 3.2, 3.3
- REQ-3 -> Sculpt Runtime Lane -> Tasks 4.1, 6.2, 7.1
- REQ-4 -> Sculpt Runtime Lane, Shared DCC Contract Layer -> Tasks 2.2, 3.3, 4.2, 4.3
- REQ-5 -> Painter Runtime Lane -> Tasks 3.1, 5.1, 5.2
- REQ-6 -> Painter Runtime Lane, Parity Harness -> Tasks 1.2, 5.2, 5.3, 6.2
- REQ-7 -> Native Language Surface And Lowering -> Tasks 2.1, 2.2, 6.1
- NFR-1 -> Reload Coordinator Telemetry, Runtime Quality Tiers -> Tasks 2.2, 2.3, 4.1, 4.2, 5.1, 5.3, 6.2
- NFR-2 -> Host Diagnostics, Last-Good Recovery -> Tasks 2.3, 3.2, 3.3, 5.2, 6.1, 7.2
- NFR-3 -> Parity Harness And Scenario Coverage -> Tasks 1.1, 2.1, 2.3, 4.1, 4.2, 4.3, 5.1, 5.2, 5.3, 6.2, 7.1
- NFR-4 -> Registry Model And Schema Ownership -> Tasks 1.1, 2.2, 3.1, 3.3, 5.1, 5.3, 6.3

## Open Issues

- The host-launcher stability decision remains open because the current native
  stack still hits environment-specific `qmlscene` failures.
- Pressure-sensitive input and the first supported OS matrix need an explicit
  implementation choice before sculpt and painter parity can be claimed in full.

## Approval

- Reviewer: Repo owner and future implementation agents
- Status: Review
- Notes: This spec is ready to drive implementation. The first execution gate is
  Task 1.1: build the explicit parity matrix and capability inventory before
  feature claims expand further.
