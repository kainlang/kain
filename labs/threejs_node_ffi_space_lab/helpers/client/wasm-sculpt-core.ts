import * as THREE from "three";

type SculptCoreExports = {
  memory: WebAssembly.Memory;
  alloc_f32(length: number): number;
  free_f32(pointer: number, length: number): void;
  sculpt_apply_brush(
    pointer: number,
    vertexCount: number,
    centerX: number,
    centerY: number,
    centerZ: number,
    normalX: number,
    normalY: number,
    normalZ: number,
    radius: number,
    strength: number,
    operationCode: number,
    falloffPower: number,
  ): number;
};

export type SculptBrushRequest = {
  center: THREE.Vector3;
  normal: THREE.Vector3;
  radius: number;
  strength: number;
  operationCode: number;
  falloffPower: number;
};

export class WasmSculptCore {
  private allocation: { pointer: number; length: number } | null = null;

  private constructor(private readonly exportsObject: SculptCoreExports) {}

  static async load(wasmUrl: string): Promise<WasmSculptCore> {
    const response = await fetch(wasmUrl);

    if (!response.ok) {
      throw new Error(`Failed to fetch sculpt core WASM from ${wasmUrl}.`);
    }

    const wasmBytes = await response.arrayBuffer();
    const instantiated = await WebAssembly.instantiate(wasmBytes, {});
    const exportsObject = instantiated.instance.exports as unknown as Partial<SculptCoreExports>;

    if (
      !(exportsObject.memory instanceof WebAssembly.Memory) ||
      typeof exportsObject.alloc_f32 !== "function" ||
      typeof exportsObject.free_f32 !== "function" ||
      typeof exportsObject.sculpt_apply_brush !== "function"
    ) {
      throw new Error("Loaded WASM module does not expose the expected sculpt exports.");
    }

    return new WasmSculptCore(exportsObject as SculptCoreExports);
  }

  private ensureAllocation(length: number) {
    if (this.allocation && this.allocation.length === length) {
      return this.allocation;
    }

    if (this.allocation) {
      this.exportsObject.free_f32(this.allocation.pointer, this.allocation.length);
    }

    const pointer = this.exportsObject.alloc_f32(length);

    if (!pointer) {
      throw new Error("WASM sculpt core failed to allocate brush scratch memory.");
    }

    this.allocation = { pointer, length };
    return this.allocation;
  }

  applyBrush(positionArray: Float32Array, request: SculptBrushRequest): number {
    const allocation = this.ensureAllocation(positionArray.length);
    const wasmView = new Float32Array(
      this.exportsObject.memory.buffer,
      allocation.pointer,
      allocation.length,
    );

    wasmView.set(positionArray);

    const affectedVertexCount = this.exportsObject.sculpt_apply_brush(
      allocation.pointer,
      positionArray.length / 3,
      request.center.x,
      request.center.y,
      request.center.z,
      request.normal.x,
      request.normal.y,
      request.normal.z,
      request.radius,
      request.strength,
      request.operationCode,
      request.falloffPower,
    );

    positionArray.set(
      new Float32Array(this.exportsObject.memory.buffer, allocation.pointer, allocation.length),
    );

    return affectedVertexCount;
  }

  dispose() {
    if (!this.allocation) {
      return;
    }

    this.exportsObject.free_f32(this.allocation.pointer, this.allocation.length);
    this.allocation = null;
  }
}
