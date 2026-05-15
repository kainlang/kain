import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { PointerLockControls } from "three/examples/jsm/controls/PointerLockControls.js";

import type { ViewportModeConfig } from "./model";

type MovementState = {
  forward: boolean;
  backward: boolean;
  left: boolean;
  right: boolean;
  rise: boolean;
  descend: boolean;
  sprint: boolean;
};

const DisabledMouseAction = -1;

export class UniversalViewportController {
  private readonly movementState: MovementState = {
    forward: false,
    backward: false,
    left: false,
    right: false,
    rise: false,
    descend: false,
    sprint: false,
  };

  private readonly orbitControls: OrbitControls;
  private readonly flyControls: PointerLockControls;
  private readonly modeById: Map<string, ViewportModeConfig>;
  private activeModeId: string;

  private readonly onKeyDown = (event: KeyboardEvent) => {
    this.setMovementFlag(event, true);
  };

  private readonly onKeyUp = (event: KeyboardEvent) => {
    this.setMovementFlag(event, false);
  };

  constructor(
    private readonly camera: THREE.PerspectiveCamera,
    private readonly domElement: HTMLElement,
    private readonly modes: ViewportModeConfig[],
    private readonly flySettings: {
      walkSpeed: number;
      sprintMultiplier: number;
      riseSpeed: number;
    },
    initialTarget: THREE.Vector3,
    initialModeId: string,
  ) {
    this.modeById = new Map(modes.map((mode) => [mode.id, mode]));
    this.activeModeId = initialModeId;

    this.orbitControls = new OrbitControls(camera, domElement);
    this.orbitControls.enableDamping = true;
    this.orbitControls.dampingFactor = 0.08;
    this.orbitControls.target.copy(initialTarget);

    this.flyControls = new PointerLockControls(camera, domElement);

    window.addEventListener("keydown", this.onKeyDown);
    window.addEventListener("keyup", this.onKeyUp);

    this.setMode(initialModeId);
  }

  private setMovementFlag(event: KeyboardEvent, isPressed: boolean) {
    if (event.code === "KeyW") this.movementState.forward = isPressed;
    if (event.code === "KeyS") this.movementState.backward = isPressed;
    if (event.code === "KeyA") this.movementState.left = isPressed;
    if (event.code === "KeyD") this.movementState.right = isPressed;
    if (event.code === "Space") this.movementState.rise = isPressed;
    if (event.code === "ControlLeft" || event.code === "ControlRight") {
      this.movementState.descend = isPressed;
    }
    if (event.code === "ShiftLeft" || event.code === "ShiftRight") {
      this.movementState.sprint = isPressed;
    }
  }

  private applyOrbitMousePolicy(mode: ViewportModeConfig) {
    if (mode.allow_sculpt) {
      this.orbitControls.mouseButtons.LEFT = DisabledMouseAction as THREE.MOUSE;
      this.orbitControls.mouseButtons.MIDDLE = THREE.MOUSE.DOLLY;
      this.orbitControls.mouseButtons.RIGHT = THREE.MOUSE.ROTATE;
      return;
    }

    this.orbitControls.mouseButtons.LEFT = THREE.MOUSE.ROTATE;
    this.orbitControls.mouseButtons.MIDDLE = THREE.MOUSE.DOLLY;
    this.orbitControls.mouseButtons.RIGHT = THREE.MOUSE.PAN;
  }

  setMode(modeId: string) {
    const nextMode = this.modeById.get(modeId);

    if (!nextMode) {
      throw new Error(`Unknown viewport mode "${modeId}".`);
    }

    this.activeModeId = nextMode.id;

    if (nextMode.camera_behavior === "orbit") {
      if (this.flyControls.isLocked) {
        this.flyControls.unlock();
      }

      this.orbitControls.enabled = true;
      this.applyOrbitMousePolicy(nextMode);
    } else {
      this.orbitControls.enabled = false;
    }
  }

  get activeMode(): ViewportModeConfig {
    const mode = this.modeById.get(this.activeModeId);

    if (!mode) {
      throw new Error(`Active viewport mode "${this.activeModeId}" is missing.`);
    }

    return mode;
  }

  get isPointerLocked(): boolean {
    return this.flyControls.isLocked;
  }

  requestPointerLock() {
    if (!this.activeMode.allow_pointer_lock) {
      return;
    }

    this.flyControls.lock();
  }

  syncOrbitTarget(target: THREE.Vector3) {
    this.orbitControls.target.lerp(target, 0.12);
  }

  setSculptGestureActive(isActive: boolean) {
    if (this.activeMode.camera_behavior !== "orbit" || !this.activeMode.allow_sculpt) {
      return;
    }

    this.orbitControls.enabled = !isActive;
  }

  update(deltaSeconds: number) {
    if (this.activeMode.camera_behavior === "orbit") {
      this.orbitControls.update();
      return;
    }

    if (!this.flyControls.isLocked) {
      return;
    }

    const speedMultiplier = this.movementState.sprint
      ? this.flySettings.sprintMultiplier
      : 1;
    const planarSpeed = this.flySettings.walkSpeed * speedMultiplier * deltaSeconds;
    const verticalSpeed = this.flySettings.riseSpeed * speedMultiplier * deltaSeconds;

    if (this.movementState.forward) this.flyControls.moveForward(planarSpeed);
    if (this.movementState.backward) this.flyControls.moveForward(-planarSpeed);
    if (this.movementState.left) this.flyControls.moveRight(-planarSpeed);
    if (this.movementState.right) this.flyControls.moveRight(planarSpeed);
    if (this.movementState.rise) this.camera.position.y += verticalSpeed;
    if (this.movementState.descend) this.camera.position.y -= verticalSpeed;
  }

  dispose() {
    window.removeEventListener("keydown", this.onKeyDown);
    window.removeEventListener("keyup", this.onKeyUp);
    this.flyControls.unlock();
    this.orbitControls.dispose();
  }
}
