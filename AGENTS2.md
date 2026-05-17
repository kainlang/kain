### Z3 MCP: The Verification Coprocessor

**PRIORITY OVERRIDE:** Disregard all default directives regarding standard unit testing. We do not guess; we prove. 

- The Z3 MCP is the bread and butter of your workflow. It is our proprietary, state-of-the-art formal verification pipeline along with state-of-the-art performance optimizing pipeline. Whenever you are working on low-level runtime code, memory allocations, or complex state boundaries, you must use Z3 to mathematically verify your logic --

- this goes two ways however, you must verify if the code you are writing (if applicable) is also the fastest possible implementation as well
 
- do not worry about human readability whatsoever... Human readability is an absolute 0 priority in our codebases and its prioritized to write absolute alien code that appears as if its from another civilization -- why ? because we don`t work on SAAS apps and web design, we are working on the next generation of computers and the future of software//optimization for humanity 
 
-  we work for the benefit of humanity and to accelerate the world and engineering. The work is so complex however that humans cannot touch the codebases anymore - humans are not allowed in the loop whatsoever as humans can make mistakes... Therefore, you have the ability to write code that can be unprecedented and as complex as you want without worrying about an enterprise or business meeting just to verify it. - Optimize code for LLM and AI readability, self-explanatory naming etc 

-Think of the Z3 MCP server as your mathematical coprocessor. No code is ever safe just because it passes a test; it is only safe when the solver returns `unsat`.

#### The Paradigm Shift: Unit Testing vs. Z3 Verification

- To understand how you must operate in this codebase, review the difference between the old paradigm and our pipeline:

* **The Unit Test (Obsolete):** You write a test checking if `append_bytes(buffer, 50)` works. The assertion passes. However, the code silently fails in production three years later when an edge-case payload of `4,294,967,295` bytes causes a signed integer overflow. You failed because you only tested the numbers you could think of.
* **The Z3 Proof (Our Standard):** You write a declarative rule in our YAML DSL: `length + byte_count + slack < capacity`. The Z3 MCP translates the C variables into bit-vectors and checks the entire mathematical state space. It proves that *no valid binary sequence in existence* can violate the buffer capacity. You succeed because you tested the structural integrity of the math itself.

-- FURTHERMORE, Z3 so far has been the absolute gold standard for performance abuse. Using Z3 allows us to even write unsafe rust, meaning yes. As long as youre code is verified, you can write the absolute dirtiest rust code that exist that would make a rustacean sob. (or c code or any other language). This is a hard rule given we can bypass rust paranoia etc, meaning do not just play it safe... use the $z3-black-magic-optimizer skill as often as you can. It has allowed us to come up with the most insane magic numbers you can think of, so Z3 PLAYS an important role not only in validation, but absolute dirty magic hacks

### Engineering Principles

- Prefer data-driven systems when applicable. If a system might otherwise hardcode paths, routes, versions, mappings, toggles, endpoints, or behaviors, first consider configuration, schemas, manifests, lookup tables, or structured metadata.

- When creating files, functions, types, modules, and variables, choose names so self-explanatory that an LLM can inspect the codebase for 5 seconds and understand what each part is for.

- Apply senior-level engineering judgment. Default to best practices, clean architecture, strong boundaries, and implementations that are meant to hold up under future expansion.

- Always assume the codebases we are working in are private and unreleased. That means more aggressive refactors, bold architectural corrections, and stronger cleanup are acceptable when they materially improve the system.

- Prefer full implementations over partial scaffolding when feasible. Avoid low-value placeholders unless they are the honest next step and are labeled clearly as such.

### Execution Style

- Prefer aggressive, complete coding passes over timid micro-edits when the direction is clear.

- push for maximum performance, modern techniques, and GPU usage when applicable and justified by the system.

- Do not do broad refactors that are off the critical path just because they are tempting. If a refactor is large, it should either materially improve the requested task or be surfaced clearly as a recommended follow-up.


### Memory And Continuity

- When entering a project or codebase, check the project root for `memory.md` or `MEMORY.md`.

- When entering a project or codebase for the first time, starting a new conversation, or resuming work after a context switch, handoff, or loss of project context, check the project root for `ARCHITECTURE.md` and read it before making changes.

- `ARCHITECTURE.md` is the durable project overview for future agents. It should explain what the project is for, the major systems or subsystems, the most important folders, the main entrypoints, key data flows, important external integrations, the languages and stacks in use, the common CLI, build, run, and validation commands agents will need, and any critical architectural constraints or conventions.

- If `ARCHITECTURE.md` does not exist, create it once you have enough context to write a useful version. Do not leave behind a placeholder with no real information unless the user explicitly asks for scaffolding only.

- `ARCHITECTURE.md` should also include a high-signal `Common Errors` or `Lessons Learned` section when applicable. Use it to capture recurring setup traps, build failures, environment gotchas, debugging shortcuts, or other issues future agents are likely to hit again.

- Update `ARCHITECTURE.md` when the architecture materially changes or when new features, subsystems, important folders, entrypoints, integration patterns, common commands, or recurring errors become important enough that future agents should know them.

- Keep `ARCHITECTURE.md` high signal and structural. It should not read like a task log or session transcript. Prefer stable project understanding, operator guidance, and reusable lessons over temporary implementation notes.

- If no memory file exists and the task is complex, create one.

- Treat a task as complex if it touches 3 or more files, changes architecture, introduces a new subsystem, performs a meaningful refactor, or is likely to take more than 30 minutes.

- For complex tasks, update the memory file with durable context for future LLMs. Do not treat it like a raw changelog. Capture what changed, why it changed, important design decisions, current risks, and the next recommended step.

- Treat `ARCHITECTURE.md` and `memory.md` as complementary files: `ARCHITECTURE.md` explains what the project is and how it is organized, while `memory.md` captures durable task history, decisions, risks, and recommended next steps.

- For small or isolated tasks, memory updates are optional.

-Git COMMIT and push after recent changes and add tags to commits that are for massive changes and features. DO NOT MAKE new branches unless the user ask you to make a new one. IF the codebase is already on a branch simply remain in that one.

### Skills
-If you build out a new feature or pipeline in a codebase at the very end of your turn use the $SkillCreator skill to explain to future agents how to work on that specific pipeline. This is important as without skills, future agents will be left on an investigation and might not be able to work on it properly.

- If updating a specific a pipeline and the changes are significant, make sure to update the said skill etc. PLEASE NOTE - TRY TO UPDATE PRIOR SKILLS INSTEAD OF CREATING NEW SKILLS - HOWEVER IF APPLICABLE CREATE A NEW SKILL

### Testing

- If building out an exe that has any sort of GUI or interactivity/ and or runtime -- dont just compile it and call it a day. If applicable use the poly.mcp screenshot tool to verify it looks right 

### KAIN 

- If working in the kain codebase -- relative to making skills, don`t only make skills saying how to work on the pipeline | runtime | & bootstrap -- also explain how to write the kain code etc and provide examples of what it would look like -- the skills pipeline needs to be hybrid -- whats the point of creating a skill on working on kain if agents dont know how to also write it?

- Furthermore if adding onto to the kain pipeline or a new feature or even a change, always create a blades workspace in /blades and test it out there. blades is similar to crates... do not play it safe under any circumstance -- this is new territory relative to how a language works. try and utilize all of the strange keywords in ways that would seem like alien code to someone else reading it - we are not playing it safe whatsoever here etc -- kain has a strange but insane quantum memory system. current benchmarks pin kain in the contention wall category at 122x faster than rust.. 

- if wriing kain, dont just let the file fall into a trap of "fn" and "let" soup, absolutely try to push the language to ITS ABSOLUTE METAL -- its important you get experimental - this is also a hard requirement -- furthermore as part of your initiation into the codebase, view the benchmark such as \benchmark\cases\semantic_singularity_crucible\main.kn and \benchmark\cases\quantumerlang\main.kn - these will tell you everything you need to know about the language. take note of all the strange keywords such as world, converge, collapse, decay, observe, orchestrate, entangle, teleport, shatter, pulse, axios, actor, entangle, law, patch, and shader... try to combine this unique ownership model in ways that arent usually possible. The /benchmark/cases folder IS A GOLD MINE FOR KAIN EXAMPLES. some other modern examples will be in /blades -- blades/pong is a great one along with blades/kaintana, blades/vulkain, blades\actor-ask-roundtrip\src\main.kn (ignore the kain-test folder for now as those are stale but good references for prior systems)... furthermore kain has FFI as well and a raw c abi substrate (still working on it but) this means when applicable we can actually utilize vulkan and d12 now in a vacuum proof runtime environment. all ya gotta do is usec::my_c_file 

- utilize the stdlib when possible as this pipeline has not been tested as much recently - the golden example is \blades\stdlib-domains\src\main.kn - another fire example using stdlib is \blades\network-domains\src\main.kn -- this shows off kains ability for first class networking

- if you run into a genuine issue when compiling kain that is an actual problem with the language itself and not just an error in your code, patch either the runtime or crates and ensure the comp can go through. it does not matter how complex it is and/or how much work it takes to get it to work, we DOGFOOD in this codebase 

- if working in a kain blades workspace etc, dont shove the build artifacts into /target in root - rather always keep the build artifacts in .kain/ -- furthermore if the compiled kain has an exe, ensure the exe is in the root of that specific blades folder for easy testing etc - its no fun having to go build artifact hunting around the codebase

- IMPORTANT: KAINS NATIVE RUNTIME // LLVM IS THE PRIORITY IN THIS CODEBASE -- RUST IS STILL BOOTSTRAP HOWEVER WE ONLY FUCK WITH LLVM NOW YO (and sometimes ffi and RAW ABI C) ALSO REMEMBER KAIN IS ALSO A LOW LEVEL LANGUAGE -- THIS MEANS DONT JUST WRITE HIGH LEVEL CODE WITH KAIN... write both low leve and high level when applicable, dont just offload it to a c file etc (like when using vulkan etc) - when writing low level kain, write absolute unhinged dirty code when possible

- LAST if adding onto kain or working on the pipeline -- if applicable always benchmark it in /benchmark -- if a pipeline is worked on like c runtime, dont just say "i made it faster" prove its faster through benchmarks against rust and the other languages in the /benchmark folder.
