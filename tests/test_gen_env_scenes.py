"""Run with python -B -m unittest discover -s tests -p test_gen_env_scenes.py."""

import importlib.util
import io
import struct
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

import numpy as np


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "gen_env_scenes.py"
SPEC = importlib.util.spec_from_file_location("gen_env_scenes", SCRIPT)
scenes = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(scenes)


def model(blob, count=2, stride=None, view_offset=0, acc_offset=0,
          view_length=None):
    view = {"buffer": 0, "byteOffset": view_offset,
            "byteLength": len(blob) - view_offset if view_length is None else view_length}
    if stride is not None:
        view["byteStride"] = stride
    return {
        "asset": {"version": "2.0"}, "scene": 0,
        "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
        "accessors": [{"bufferView": 0, "componentType": scenes.FLOAT,
                       "type": "VEC3", "count": count, "byteOffset": acc_offset}],
        "bufferViews": [view], "buffers": [{"byteLength": len(blob)}],
    }


class PositionBoundsTests(unittest.TestCase):
    points = np.array([[-1, 2, 3], [4, -5, 6]], dtype="<f4")

    def assert_bounds(self, src, blob):
        lo, hi = scenes.Builder()._scene_bbox(src, blob)
        np.testing.assert_array_equal(lo, self.points.min(axis=0))
        np.testing.assert_array_equal(hi, self.points.max(axis=0))

    def assert_rejected_before_decode(self, src, blob):
        # Invalid metadata must fail before any native array reads the payload.
        with patch.object(scenes.np, "ndarray", side_effect=AssertionError("decoded")):
            with self.assertRaises(ValueError):
                scenes.Builder()._scene_bbox(src, blob)

    def test_packed_positions(self):
        blob = self.points.tobytes()
        for stride in (None, 12):
            with self.subTest(stride=stride):
                self.assert_bounds(model(blob, stride=stride), blob)

    def test_interleaved_positions_without_trailing_padding(self):
        # Stride is 24, but the last vertex needs only its 12 POSITION bytes.
        blob = self.points[0].tobytes() + b"\xff" * 12 + self.points[1].tobytes()
        self.assert_bounds(model(blob, stride=24), blob)

    def test_nonzero_view_and_accessor_offsets(self):
        blob = b"\xff" * 12 + self.points[0].tobytes() + b"\xff" * 4
        blob += self.points[1].tobytes() + b"\xff" * 8
        src = model(blob, stride=16, view_offset=4, acc_offset=8, view_length=36)
        self.assert_bounds(src, blob)

    def test_glb_padding_outside_declared_buffer(self):
        blob = self.points.tobytes() + b"\x00" * 3
        src = model(blob, view_length=24)
        src["buffers"][0]["byteLength"] = 24
        self.assert_bounds(src, blob)

    def test_single_vertex_needs_no_stride_padding(self):
        blob = self.points[0].tobytes()
        lo, hi = scenes.Builder()._scene_bbox(model(blob, count=1, stride=252), blob)
        np.testing.assert_array_equal(lo, self.points[0])
        np.testing.assert_array_equal(hi, self.points[0])

    def test_original_short_stride_payload(self):
        blob = struct.pack("<f", 1)
        self.assert_rejected_before_decode(model(blob, count=1, stride=4), blob)

    def test_invalid_metadata(self):
        blob = self.points.tobytes()
        cases = [
            ("accessors", "componentType", 5123),
            ("accessors", "type", "VEC2"),
            ("accessors", "sparse", {}),
            ("accessors", "bufferView", -1),
            ("accessors", "bufferView", 1),
            ("accessors", "bufferView", True),
            ("accessors", "bufferView", None),
            ("bufferViews", "buffer", -1),
            ("bufferViews", "buffer", 1),
            ("bufferViews", "buffer", False),
            ("buffers", "uri", "external.bin"),
            ("buffers", "byteLength", 20),
            ("buffers", "byteLength", -1),
        ]
        for group, field, values in (
            ("accessors", "count", [0, -1, 3, 1.5, True, None, 10**100]),
            ("accessors", "byteOffset", [-4, 2, 4, 1.5, True, None]),
            ("bufferViews", "byteOffset", [-4, 2, 4, 1.5, True, None]),
            ("bufferViews", "byteLength", [-1, 0, 20, 28, 1.5, True, None]),
            ("bufferViews", "byteStride", [-12, 0, 4, 8, 13, 256, 1.5, True, None]),
        ):
            cases.extend((group, field, value) for value in values)
        for group, field, value in cases:
            with self.subTest(group=group, field=field, value=value):
                src = model(blob)
                src[group][0][field] = value
                self.assert_rejected_before_decode(src, blob)

    def test_truncated_bin_and_final_vertex(self):
        blob = self.points.tobytes()
        self.assert_rejected_before_decode(model(blob), blob[:-4])
        src = model(blob)
        src["bufferViews"][0]["byteLength"] = 20
        # Extra actual BIN bytes must not permit crossing the declared view.
        self.assert_rejected_before_decode(src, blob + b"\x00" * 32)
        src = model(blob)
        src["buffers"][0]["byteLength"] = 20
        self.assert_rejected_before_decode(src, blob)

    def test_bounds_metadata_fast_path_is_preserved(self):
        src = model(b"")
        acc = src["accessors"][0]
        acc.update(min=self.points.min(axis=0).tolist(),
                   max=self.points.max(axis=0).tolist())
        # Existing bounds-only paths need no binary decoder, including sparse
        # and extension-supplied positions. Keep their previous behavior.
        acc.pop("bufferView")
        acc["sparse"] = {}
        with patch.object(scenes.np, "ndarray", side_effect=AssertionError("decoded")):
            self.assert_bounds(src, b"")

    def test_incomplete_bounds_metadata_still_validates_payload(self):
        blob = struct.pack("<f", 1)
        for field in ("min", "max"):
            with self.subTest(field=field):
                src = model(blob, count=1, stride=4)
                src["accessors"][0][field] = [1, 1, 1]
                self.assert_rejected_before_decode(src, blob)

    def test_glb_import_rejects_original_payload(self):
        blob = struct.pack("<f", 1)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.glb"
            scenes.save_glb(path, model(blob, count=1, stride=4), blob)
            with patch.object(scenes.np, "ndarray", side_effect=AssertionError("decoded")):
                with self.assertRaises(ValueError):
                    scenes.Builder().merge_model(path, "Bad", 5, "xz", (0, 0), 0, 0)

    def test_valid_glb_import_and_output_transform(self):
        blob = self.points[0].tobytes() + b"\xff" * 12 + self.points[1].tobytes()
        with tempfile.TemporaryDirectory() as directory, redirect_stdout(io.StringIO()):
            path = Path(directory) / "valid.glb"
            out = Path(directory) / "merged.glb"
            scenes.save_glb(path, model(blob, stride=24), blob)
            builder = scenes.Builder()
            builder.merge_model(path, "Valid", 5, "xz", (10, 20), 0, 2)
            builder.save(out)
            src, _ = scenes.load_glb(out)
            wrapper = src["nodes"][-1]
            np.testing.assert_allclose(wrapper["scale"], [1, 1, 1])
            np.testing.assert_allclose(wrapper["translation"], [8.5, 7, 15.5])


if __name__ == "__main__":
    unittest.main()
