"""tools/stereo_eye_sites.py: every reader of the eye's camera, position or layer is classified, and every routed
site still routes eye 1's value (the second eye's replay)."""
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import stereo_eye_sites  # noqa: E402

READER = """static void build(real *out)
{
	out[0] = global_window_parameters.camera.position.x;
}
"""


class StereoEyeSitesTests(unittest.TestCase):
    def tree(self, text):
        temp = tempfile.TemporaryDirectory(prefix="stereo-eye-sites-")
        self.addCleanup(temp.cleanup)
        root = pathlib.Path(temp.name)
        (root / "source").mkdir()
        (root / "port/linux/game").mkdir(parents=True)
        (root / "source/site.c").write_text(text)
        return root

    def test_the_tree_is_classified(self):
        problems, _ = stereo_eye_sites.check()
        self.assertEqual(problems, [])

    def test_a_new_reader_fails(self):
        problems, _ = stereo_eye_sites.check(self.tree(READER), {})
        self.assertEqual(len(problems), 1)
        self.assertIn("build reads the eye's camera", problems[0])

    def test_a_routed_site_that_stops_routing_fails(self):
        problems, _ = stereo_eye_sites.check(self.tree(READER), {("source/site.c", "build"): ("routed", "a block")})
        self.assertEqual(len(problems), 1)
        self.assertIn("calls neither", problems[0])

    def test_a_per_eye_helper_needs_a_routed_caller(self):
        caller = "void store(void)\n{\n\treal out[4];\n\n\tbuild(out);\n}\n"
        sites = {("source/site.c", "build"): ("per-eye", "a block", ["store"])}
        problems, _ = stereo_eye_sites.check(self.tree(READER + caller), sites)
        self.assertEqual(len(problems), 1)
        self.assertIn("does not route eye 1", problems[0])
        routed = caller.replace("\tbuild(out);\n", "\tbuild(out);\n\tif (rasterizer_other_eye_begin())\n\t\tbuild(out);\n")
        problems, _ = stereo_eye_sites.check(self.tree(READER + routed), sites)
        self.assertEqual(problems, [])

    def test_an_open_site_fails_only_when_strict(self):
        sites = {("source/site.c", "build"): ("open", "not yet")}
        self.assertEqual(stereo_eye_sites.check(self.tree(READER), sites)[0], [])
        self.assertEqual(len(stereo_eye_sites.check(self.tree(READER), sites, strict=True)[0]), 1)


if __name__ == "__main__":
    unittest.main()
