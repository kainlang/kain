 # Kain Unreal Folder Guide / Pre public update

- This folder holds the legacy pipeline for when Kain was initially used and scaffolded as a codegen language to generate C++ plugins for UE5

## What Lives Here

- UE5 asset tooling and build helpers
- Unreal-facing shaders, configs, or bridge data

## Notes

- keep Unreal-generated build outputs out of git
- document any required engine version or plugin dependency

## /plugins

- To see examples of what the old pipeline was able to do, there is a vast amount of examples in /plugins


## Roadmap

- This pipeline helped Kain accelerate fast in its maturity as it was used to generate all in one plugins and content for UE5 including shaders, blueprints, materials, and 300+ c++ file projects all from 100-2000 lines of Kain code. This pipeline is however deprecated now as the main focus is on Kain itself. It has not been tested in a while so it is unsure whether this pipeline works as of 6/18/2026