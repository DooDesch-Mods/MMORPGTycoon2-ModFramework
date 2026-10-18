"""Keep public percentage denominators and equal-area tiles honest."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('roadmap_renderer', ROOT / 'tools/render_roadmap.py')
roadmap = importlib.util.module_from_spec(spec)
spec.loader.exec_module(roadmap)


class CoverageTests(unittest.TestCase):
    def test_partial_and_experimental_areas_are_not_counted_as_available(self):
        counts = roadmap.coverage_counts({'features': [
            {'name': 'A', 'status': 'available'}, {'name': 'B', 'status': 'partial'},
            {'name': 'C', 'status': 'experimental'}, {'name': 'D', 'status': 'planned'}]})
        self.assertEqual(counts['available'], 1)
        self.assertEqual(sum(counts.values()), 4)

    def test_empty_unknown_and_duplicate_inventories_fail(self):
        for rows in ([], [{'name': 'A', 'status': 'done-ish'}],
                     [{'name': 'A', 'status': 'available'}, {'name': 'A', 'status': 'planned'}]):
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                roadmap.coverage_counts({'features': rows})

    def test_tiles_have_equal_area_and_never_overlap_or_leave_the_container(self):
        for count in range(1, 40):
            rows = list(range(count))
            cells = roadmap.tiles(rows, 19, 23, 544, 680)
            self.assertEqual(sorted(cell[0] for cell in cells), rows)
            for index, (_, x, y, width, height) in enumerate(cells):
                self.assertGreaterEqual(x, 19)
                self.assertGreaterEqual(y, 23)
                self.assertLessEqual(x + width, 563.000001)
                self.assertLessEqual(y + height, 703.000001)
                self.assertAlmostEqual(width * height, 544 * 680 / count, places=6)
                for _, other_x, other_y, other_width, other_height in cells[index + 1:]:
                    overlap_x = min(x + width, other_x + other_width) - max(x, other_x)
                    overlap_y = min(y + height, other_y + other_height) - max(y, other_y)
                    self.assertFalse(overlap_x > .000001 and overlap_y > .000001)


if __name__ == '__main__':
    unittest.main()
