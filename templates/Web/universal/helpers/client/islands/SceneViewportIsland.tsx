import { h } from "preact";
import { useEffect, useRef, useState } from "preact/hooks";
import * as THREE from "three";

import type { KainSceneDescriptor } from "../lib/kain_site_data";

type Props = {
  scene: KainSceneDescriptor | null;
};

function parseColor(value: string | null | undefined, fallback: number): number {
  if (!value) return fallback;
  try {
    return new THREE.Color(value).getHex();
  } catch {
    return fallback;
  }
}

export function SceneViewportIsland(props: Props) {
  const mountRef = useRef<HTMLDivElement | null>(null);
  const [status, setStatus] = useState("boot");

  useEffect(() => {
    const mount = mountRef.current;
    if (!mount) return;
    const width = mount.clientWidth || 640;
    const height = Math.max(280, mount.clientHeight || 360);
    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setSize(width, height);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    mount.appendChild(renderer.domElement);

    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(54, width / height, 0.1, 200);
    camera.position.set(0, 0.8, 3.1);

    const ambient = new THREE.AmbientLight(0xffffff, 0.55);
    scene.add(ambient);

    const keyLight = new THREE.DirectionalLight(0xffffff, 0.9);
    keyLight.position.set(2, 3, 4);
    scene.add(keyLight);

    const group = new THREE.Group();
    scene.add(group);

    const descriptor = props.scene;
    const layers = descriptor?.layers || [];
    const baseColor = parseColor(layers[0]?.color, 0x5ae4ff);
    const ringColor = parseColor(layers[1]?.color, 0xffd166);
    const accentColor = parseColor(layers[2]?.color, 0x8ce66f);

    const core = new THREE.Mesh(
      new THREE.IcosahedronGeometry(0.62, 2),
      new THREE.MeshStandardMaterial({ color: baseColor, metalness: 0.15, roughness: 0.25 })
    );
    group.add(core);

    const ring = new THREE.Mesh(
      new THREE.TorusGeometry(0.92, 0.12, 18, 96),
      new THREE.MeshStandardMaterial({ color: ringColor, metalness: 0.25, roughness: 0.35 })
    );
    ring.rotation.x = 0.8;
    ring.rotation.y = 0.2;
    group.add(ring);

    const accent = new THREE.Mesh(
      new THREE.SphereGeometry(0.22, 20, 18),
      new THREE.MeshStandardMaterial({ color: accentColor, metalness: 0.1, roughness: 0.2 })
    );
    accent.position.set(0.95, 0.25, 0.15);
    group.add(accent);

    let frame = 0;
    let raf = 0;
    setStatus("running");

    const tick = () => {
      frame += 1;
      core.rotation.y += 0.005;
      ring.rotation.z += 0.003;
      accent.position.y = 0.25 + Math.sin(frame * 0.03) * 0.08;
      renderer.render(scene, camera);
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);

    const handleResize = () => {
      const nextWidth = mount.clientWidth || width;
      const nextHeight = Math.max(280, mount.clientHeight || height);
      renderer.setSize(nextWidth, nextHeight);
      camera.aspect = nextWidth / nextHeight;
      camera.updateProjectionMatrix();
    };
    window.addEventListener("resize", handleResize);

    return () => {
      window.removeEventListener("resize", handleResize);
      cancelAnimationFrame(raf);
      renderer.dispose();
      mount.innerHTML = "";
      setStatus("stopped");
    };
  }, [props.scene]);

  return (
    <div class="kain-island kain-island-scene">
      <div class="kain-island-header">
        <p class="kain-island-eyebrow">Scene</p>
        <h3 class="kain-island-title">Three.js scene island</h3>
        <p class="kain-island-copy">
          This is the first real WebGL lane in the universal web pack: it renders a lightweight 3D preview from the
          manifest scene descriptor without requiring Rust.
        </p>
      </div>
      <div class="kain-scene-mount" ref={mountRef} aria-label="3D scene preview" />
      <p class="kain-island-status">{status}</p>
    </div>
  );
}
