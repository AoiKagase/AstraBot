"""Small geometric oracles, not a replacement for engine acceptance."""
import struct
import unittest

from diagnose import Area, Bsp, Nav, brush_policy, components, finding_witness


class GeometryOracles(unittest.TestCase):
    def test_bilinear_floor_and_clamping(self):
        a = Area(1, 0, (0, 0, 0, 10, 10, 30, 10, 20), [[], [], [], []])
        self.assertEqual(a.z(5, 5), 15)
        self.assertEqual(a.z(-5, 15), 20)

    def test_halfspace_signed_leaf_and_trace(self):
        # z >= 0 is empty; z < 0 is solid. Hull 0 uses leaf indices,
        # while hulls 1/3 use CONTENTS values directly.
        b = Bsp.synthetic([(0, 0, 1, 0, 2)], [(0, -1, -2)], [-1, -2], [(0, -1, -2)])
        self.assertEqual(b.contents(0, 0, (0, 0, 1)), -1)
        self.assertEqual(b.contents(0, 0, (0, 0, -1)), -2)
        self.assertEqual(b.contents(1, 0, (0, 0, -1)), -2)
        h = b.trace(0, 0, (2, 3, 10), (2, 3, -10))
        self.assertAlmostEqual(h[0], .5)
        self.assertEqual(h[1], (0, 0, 1))
        self.assertEqual(b.trace(0, 0, (0, 0, -1), (0, 0, -2))[0], 0)
        self.assertIsNone(b.trace(0, 0, (0, 0, 1), (0, 0, 2)))

    def test_distinct_standing_and_crouching_hulls(self):
        # Ceiling at 60: stand center +36 cannot fit, crouch center +18 can.
        b = Bsp.synthetic([(0, 0, 1, 24, 2), (0, 0, 1, 42, 2)],
                          [(0, -1, -1)], [-1], [(0, -2, -1), (1, -2, -1)])
        self.assertEqual(b.contents(1, 0, (0, 0, 36)), -2)
        self.assertEqual(b.contents(3, 1, (0, 0, 18)), -1)

    def test_brush_semantics(self):
        self.assertEqual(brush_policy({'classname': 'func_illusionary'}), 'ignored_noncollision')
        self.assertEqual(brush_policy({'classname': 'func_bomb_target'}), 'ignored_trigger')
        self.assertEqual(brush_policy({'classname': 'func_ladder'}), 'ladder')
        self.assertEqual(brush_policy({'classname': 'func_breakable'}), 'breakable_intact')
        self.assertEqual(brush_policy({'classname': 'func_door'}), 'door_reference_pose')
        self.assertEqual(brush_policy({'classname': 'func_wall_toggle', 'spawnflags': '1'}),
                         'toggle_initially_off')

    def test_box_top_state_and_origin(self):
        b = Bsp.synthetic([(0, 0, 1, 0, 2), (0, 0, 1, 64, 2)],
                          [(0, -1, -2), (1, -1, -2)], [-1, -2],
                          [(0, -1, -2), (1, -1, -2)])
        b.models = [(-100, -100, -100, 100, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                    (-10, -10, 0, 10, 10, 64, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0)]
        b.solids = [(0, 'world', (0, 0, 0), (0, 0, 0), {}),
                    (1, 'breakable_intact', (20, 0, 16), (0, 0, 0), {})]
        status, hit = b.floor(20, 0, 80)
        self.assertEqual(status, 'resolved')
        self.assertEqual(hit['z'], 80)
        self.assertEqual(b.floor(20, 0, 80, 'removed_open')[1]['z'], 0)
        self.assertEqual(b.floor(0, 0, 80)[1]['z'], 0)

    def test_inverse_entity_rotation(self):
        result = Bsp.local((10, 30, 5), (10, 20, 5), (0, 90, 0))
        self.assertAlmostEqual(result[0], 10)
        self.assertAlmostEqual(result[1], 0)
        self.assertAlmostEqual(result[2], 0)

    def test_directed_components_do_not_invent_reverse_edges(self):
        weak, strong = components({1: [2], 2: [1, 3], 3: [], 4: []})
        self.assertEqual(sorted(map(len, weak)), [1, 3])
        self.assertEqual(sorted(map(len, strong)), [1, 1, 2])

    def test_unresolved_only_candidate_saves_reproducible_witness(self):
        resolved = dict(status='resolved', error=1.725, x=587.5, y=-520)
        unresolved = dict(status='start_solid', x=597.5, y=-520)
        self.assertIs(finding_witness([resolved, unresolved]), unresolved)
        self.assertIsNone(finding_witness([resolved]))
        resolved['error'] = 19
        self.assertIs(finding_witness([resolved, unresolved]), resolved)

    def test_minimal_nav_and_trailing_or_missing_reference(self):
        header = struct.pack('<IIIHI', 0xfeedface, 5, 1234, 0, 1)
        area = struct.pack('<IB8f', 1, 0, 0, 0, 0, 25, 25, 0, 0, 0)
        tail = struct.pack('<4IBBIH', 0, 0, 0, 0, 0, 0, 0, 0)
        n = Nav(header + area + tail)
        self.assertEqual(len(n.areas), 1)
        self.assertEqual(n.errors, [])
        with self.assertRaises(ValueError):
            Nav(header + area + tail + b'x')
        linked = struct.pack('<I', 1) + struct.pack('<I', 999) + struct.pack('<3IBBIH', 0, 0, 0, 0, 0, 0, 0)
        self.assertIn('missing_area:1:999', Nav(header + area + linked).errors)


if __name__ == '__main__':
    unittest.main()
