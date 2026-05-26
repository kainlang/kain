from __future__ import annotations

import unittest
import json

import numpy as np

import pykain


class PykainNativeTests(unittest.TestCase):
    def test_native_extension_loads_and_inspects_numpy(self) -> None:
        arr = np.arange(12, dtype=np.uint8).reshape(3, 4)

        self.assertTrue(pykain.native_available())
        self.assertEqual(pykain.native_version(), "0.1.0")

        info = pykain.inspect(arr)
        info_json = json.loads(pykain.inspect_json(arr))
        self.assertTrue(info["valid"])
        self.assertTrue(info_json["valid"])
        self.assertEqual(info["backend"], "numpy")
        self.assertEqual(info["shape"], [3, 4])
        self.assertEqual(info["element_type"], "uint8")
        self.assertEqual(info["byte_length"], 12)
        self.assertTrue(info["pointer_available"])

    def test_buffer_adaptation_and_signature(self) -> None:
        arr = np.arange(8, dtype=np.uint8)

        envelope = pykain.as_buffer(arr, lane="unit")
        self.assertEqual(envelope.kind, "buffer")
        self.assertEqual(envelope.info["contract"], "kain.shared.buffer")
        self.assertEqual(envelope.labels["lane"], "unit")
        self.assertEqual(pykain.signature(arr), int(arr.sum()))
        self.assertEqual(pykain.as_bytes(arr), bytes(range(8)))
        self.assertEqual(pykain.buffer.grid_contract('{"tensor_rows":2,"tensor_cols":4}', 3), 0)
        self.assertTrue(pykain.buffer.grid_ok('{"tensor_rows":2,"tensor_cols":4}', 3))
        self.assertGreater(pykain.smoke_score(), 0)

    def test_image_and_tensor_envelopes(self) -> None:
        image = np.zeros((4, 5, 3), dtype=np.uint8)
        tensor = np.linspace(0.0, 1.0, 6, dtype=np.float32).reshape(2, 3)

        image_env = pykain.as_image(image)
        tensor_env = pykain.as_tensor(tensor)

        self.assertEqual(image_env.kind, "image")
        self.assertEqual(image_env.descriptor()["shape"], [4, 5, 3])
        self.assertEqual(tensor_env.kind, "tensor")
        self.assertEqual(tensor_env.descriptor()["shape"], [2, 3])
        self.assertEqual(pykain.image.render_contract('{"image_width":5,"image_height":4,"image_channels":3}'), 0)
        self.assertEqual(pykain.tensor.grid_contract('{"tensor_rows":2,"tensor_cols":3}', 7), 0)
        self.assertTrue(pykain.image.render_ok('{"image_width":5,"image_height":4,"image_channels":3}'))
        self.assertTrue(pykain.tensor.grid_ok('{"tensor_rows":2,"tensor_cols":3}', 7))

    def test_gpu_shader_world_actor_semantics(self) -> None:
        arr = np.arange(4, dtype=np.uint8)
        gpu = pykain.gpu.compute_buffer(arr, binding=2)
        shader = pykain.shader.compute("shader compute Demo(id: UVec3) -> Vec4: return vec4(1.0, 0.0, 0.0, 1.0)")
        world = pykain.world.ref("Authority", {"score": 42})
        link = pykain.world.entangle("Authority.score", "Mirror.score_copy")
        actor = pykain.actor.ref("Relay", actor_id=7)
        msg = pykain.actor.message(actor, "Tick", {"frame": 1})

        self.assertEqual(gpu.policy["binding"], 2)
        self.assertEqual(gpu.descriptor()["resource_kind"], "buffer")
        self.assertEqual(shader.descriptor()["stage"], "compute")
        self.assertEqual(world.descriptor()["state"]["score"], 42)
        self.assertEqual(link.descriptor()["policy"], "single_writer")
        self.assertEqual(msg["target_id"], 7)
        self.assertEqual(pykain.validate.version(), 1)


if __name__ == "__main__":
    unittest.main()
