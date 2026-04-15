With this self hosting pipeline we should focus on the initial idea of what this language was meant to be from the start --- another core thing to keep in mind is the direction and purpose to go in. I am going to list in a numbered list the goal for this language and pipeline so that way its easy to set a roadmap // goal. This numbered list is in no particular order and will also include a wish list and rules going forward


1. this pipeline needs to focus on the native runtime and furthermore be cutting edge and next level, and also be able to compile to an executable. using rust as the runtime was a nightmare and using rust for anything leads to bloat and cross domain slop. the starting goal for this language initially was to stop domain hopping etc... while the orchestration pipelines currently present in kain are absolutely fire, a main focus on kain and primary focus is kain itself -- meaning the language is so absolutely powerful, it can do quite frankly anything it sets it mind too. the ultimate end goal is the closest possible bridge between raw thought and creativity. there should be no limits etc, furthermore things like UI should be even more expressive than react- thanks to our C RUNTIME and the plethora of UI Libraries WE HAVE already wired in the native runtime (notice how i mentioned native runtime and not rust runtime.... the rust runtime once again is deadweight and holding this back)

2. the language needs to have UI so expressive its as good as typescript if not better. --- Yes bringing up ui on point 2 is not the best idea, but gui IS extremely important AS THAT is the interaction plane between a user and what they want to achieve. without UI theres no substance, the ui should not just be a jumble of json or toml files-- it needs to be able to expressively convey UI similar to ts but backed by the c runtime

3. kain needs to have 3d capaibilities, and GPU baked in as this is greenfield architecture... things like shaders, graphics, 3d are top priority along with the ability to make 2d applications etc --- this language once again needs to feel like god mode.

4. this language needs to be BLEEDING EDGE, IM TALKING FASTEST OF THE FASTEST- INSANE HACKS ARE RECOMENDED 

5. WE MAKE NO SACRIFICES--- AND WE CODE AGGRESSIVELY. FULL IMPLEMENTATIONS ONLY AND FURTHERMORE ALWAYS MAX POWER

6. WE NEED TO THINK ABOUT HOW WE ARE GOING TO ADD SUPPORT FOR CRATES AND OTHER ECOSYSTEMS INTO THE NATIVE SELF HOSTING PIPELINE AS KAIN NEEDS TO SUPPORT A VAST ARRAY OF ECOSYSTEMS--- AT THE CORE OF IT ALL THO , EVEN WITH VAST ECOSYSTEMS ETC, THIS LANGUAGE NOT ONLY NEEDS TO HAVE ORCHESTRATION CAPABILITIES, IT ALSO NEEDS TO BE THE ONE RUNNING THE SHOW... THE ECOSYSTEMS AND LIBRARIES FROM OTHER LANGS ? yeah so the lang just needs those as tools, not some annoying toml based orchestration layer.

## Source Tree Ownership

- `src/core` is the only active hand-owned source lane right now.
- `src/.rustimport/reference` is the moved donor corpus from the earlier Rust import lane.
- `src/.rustimport/phase2` is the canonical phase2 selfhost mirror root.
- `src/.rustimport/*` and `src/.legacy` are reference-only and should not be edited by hand.

## Durable Direction

This file is the raw vision / motivation note.

For the durable selfhost execution contract, read:

- `src/SELFHOST_DIRECTION.md`

Short version:

- `KAIN.toml` should be the canonical selfhost contract
- `src/core` should own the hand-written compiler
- `.legacy` should inform stage structure, not runtime semantics
- Rust is allowed as temporary bootstrap host substrate only, not as the permanent owner of compiler passes
