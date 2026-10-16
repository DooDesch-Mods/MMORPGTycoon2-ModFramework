import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from mt2fw import glb, sample, packages, vmb


class GLBTests(unittest.TestCase):
    def setUp(self):
        base=Path(__file__).resolve().parents[1]/'artifacts/glb-tests'
        base.mkdir(parents=True,exist_ok=True)
        self.root=Path(tempfile.mkdtemp(dir=base))
        self.file=self.root/'sample.glb'
        self.file.write_bytes(sample.pyramid())

    def test_static_geometry_material_and_export_contract(self):
        mesh=glb.GLB(self.file).mesh()
        self.assertEqual(mesh.triangles,6)
        self.assertEqual(mesh.bounds,[[-2.,0.,-2.],[2.,4.,2.]])
        result=glb.export(mesh,self.root/'mod','test.scenery','pyramid')
        manifest=packages.check(Path(result['directory']))['manifest']
        self.assertEqual(manifest['type'],'content')
        model=next((self.root/'mod').rglob('*.vmd')).read_text()
        self.assertNotIn(';',model)
        rows=[line.strip().split() for line in model.splitlines() if line.strip() and line.strip()[0] in '-0123456789']
        self.assertTrue(all(len(row)==12 for row in rows[:-1]))
        self.assertEqual(len(rows[-1]),18)
        binary=next((self.root/'mod').rglob('*.vmb')).read_bytes()
        decoded=vmb.decode(binary)
        self.assertEqual(vmb.encode(decoded['fragments'],decoded['name']),binary)
        self.assertEqual(len(decoded['fragments'][0]['vertices']),18)

    def test_embedded_texture_survives_conversion(self):
        self.file.write_bytes(sample.pyramid(color=(1,1,1,1),textured=True))
        mesh=glb.GLB(self.file).mesh()
        self.assertEqual(len(mesh.images),1)
        result=glb.export(mesh,self.root/'textured','test.texture')
        self.assertTrue(list(Path(result['directory']).rglob('*.png')))
        self.assertIn('texture "mt2fw/',next(Path(result['directory']).rglob('*.mat')).read_text())

    def test_node_transform_and_normal_matrix(self):
        reader=glb.GLB(self.file)
        reader.doc['nodes'][0]['translation']=[10,0,0]
        reader.doc['nodes'][0]['scale']=[2,3,2]
        mesh=reader.mesh()
        self.assertEqual(mesh.bounds,[[6.,0.,-4.],[14.,12.,4.]])
        for fragment in mesh.fragments:
            for row in fragment['vertices']:
                self.assertAlmostEqual(sum(v*v for v in row[7:10]),1)

    def test_truncation_sparse_cycles_and_animation_fail_with_cause(self):
        self.file.write_bytes(sample.pyramid()[:-2])
        with self.assertRaisesRegex(packages.PackageError,'complete GLB'):
            glb.GLB(self.file)
        self.file.write_bytes(sample.pyramid())
        for mutate,problem in ((lambda r:r.doc.update(animations=[{}]),'static'),
                               (lambda r:r.doc['accessors'][0].update(sparse={}), 'Sparse'),
                               (lambda r:r.doc['nodes'][0].update(children=[0]), 'cycle')):
            reader=glb.GLB(self.file)
            mutate(reader)
            if problem=='static':
                self.file.write_bytes(sample.encode(reader.doc,reader.binary))
                with self.assertRaisesRegex(packages.PackageError,problem):glb.GLB(self.file)
                self.file.write_bytes(sample.pyramid())
            else:
                with self.assertRaisesRegex(packages.PackageError,problem):reader.mesh()


if __name__=='__main__':
    unittest.main()
