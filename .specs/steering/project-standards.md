# Project Standards

## Code Quality

- Keep semantic ownership in Kain, typed contracts, and manifest data instead of
  leaking meaning into generated code, native hosts, or ad-hoc helper scripts.
- Prefer explicit, maintainable structure with names that another strong model
  can understand within seconds.
- Match established repository conventions before introducing new ones, but
  aggressively improve weak boundaries when they block native-authoring goals.
- Treat `.reference/` as the product-shape oracle for flagship parity work and
  current runtime/docs as the implementation truth for what Kain already owns.
- Prefer data-driven registries for workspaces, tools, brushes, shaders,
  runtime lanes, export presets, and capability flags instead of hardcoded
  switch ladders.

## Testing

- Test the highest-risk logic first: compiler lowering, runtime compatibility,
  GPU artifact generation, native host reload behavior, and artist-facing
  command flows.
- For major app or compiler work, include unit, integration, and scenario
  validation rather than relying on only one layer.
- Keep validation commands discoverable, repeatable, and runnable from the repo
  root.
- Flagship parity work should maintain an explicit acceptance matrix that maps
  reference-app capabilities to Kain-owned implementations and automated checks.

## Configuration and Secrets

- Keep secrets out of version control.
- Prefer environment-driven or configuration-driven values over hardcoded
  behavior.
- Document required configuration at the point of use.
- Generated directories, artifact roots, and host launch paths should be
  configurable and excluded from watch loops by default.

## Security and Privacy

- Validate untrusted input.
- Apply least privilege to credentials and integrations.
- Record security-relevant assumptions when they affect the design.

## Performance and Reliability

- Make expensive paths visible and measurable, especially viewport, brush, GPU,
  and hot-reload paths.
- Prefer predictable behavior, explicit degradation states, and clean rollback
  paths for risky runtime or packaging changes.
- Capture observability requirements for critical workflows such as native-ui
  dev reloads, shader/materialization failures, and host restart reasons.
- Native DCC features should fail loudly with capability reports or disabled
  states rather than silently pretending parity that does not exist.

## Documentation

- Update durable docs when architecture or operating expectations change.
- Keep feature-local rationale inside the matching `.specs/<slug>/` package.
- For large initiatives, keep `ARCHITECTURE.md` structural and keep
  implementation-program rationale in `.specs/`.
