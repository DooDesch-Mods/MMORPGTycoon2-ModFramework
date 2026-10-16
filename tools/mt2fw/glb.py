"""Read static GLB 2.0 and emit additive VectorStorm text-model content."""
from __future__ import annotations
from dataclasses import dataclass
import io
import hashlib
import json
import math
from pathlib import Path
import struct
from .packages import PackageError, ID, atomic_json

IDENTITY = (1.,0.,0.,0., 0.,1.,0.,0., 0.,0.,1.,0., 0.,0.,0.,1.)
COMPONENTS = {5120: ('b',1), 5121: ('B',1), 5122: ('h',2), 5123: ('H',2), 5125: ('I',4), 5126: ('f',4)}
WIDTHS = {'SCALAR':1, 'VEC2':2, 'VEC3':3, 'VEC4':4, 'MAT4':16}


def multiply(a, b):
    return tuple(sum(a[k*4+r]*b[c*4+k] for k in range(4)) for c in range(4) for r in range(4))


def transform(matrix, position):
    return tuple(sum(matrix[c*4+r]*position[c] for c in range(3))+matrix[12+r] for r in range(3))


def unit(value):
    length = math.sqrt(sum(x*x for x in value))
    if length < 1e-12:
        raise PackageError('Degenerate normal or triangle')
    return tuple(x/length for x in value)


def cross(a,b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def normal(matrix, value):
    a,b,c = matrix[0:3],matrix[4:7],matrix[8:11]
    bc,ca,ab = cross(b,c),cross(c,a),cross(a,b)
    determinant = sum(a[i]*bc[i] for i in range(3))
    if abs(determinant)<1e-12:
        raise PackageError('Node transform has zero scale')
    return unit(tuple((bc[i]*value[0]+ca[i]*value[1]+ab[i]*value[2])/determinant for i in range(3)))


def node_matrix(node):
    if 'matrix' in node:
        result = tuple(node['matrix'])
        if len(result)!=16:
            raise PackageError('Node matrix must contain 16 numbers')
    else:
        translation = node.get('translation',[0,0,0])
        scale = node.get('scale',[1,1,1])
        rotation = node.get('rotation',[0,0,0,1])
        if len(translation)!=3 or len(scale)!=3 or len(rotation)!=4:
            raise PackageError('Invalid node transform shape')
        x,y,z,w = rotation
        if abs(sum(v*v for v in rotation)-1)>0.001:
            raise PackageError('Node quaternion must be normalized')
        result = (1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y),0,
                  2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x),0,
                  2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y),0,
                  *translation,1)
        result = tuple(result[i]*scale[i//4] if i<12 else result[i] for i in range(16))
    if any(not isinstance(v,(int,float)) or isinstance(v,bool) or not math.isfinite(v) for v in result):
        raise PackageError('Transform requires finite numeric values')
    return result


@dataclass
class Mesh:
    fragments: list
    images: list
    warnings: list

    @property
    def triangles(self):
        return sum(len(f['vertices'])//3 for f in self.fragments)

    @property
    def bounds(self):
        rows=[v[:3] for f in self.fragments for v in f['vertices']]
        return [[min(v[i] for v in rows) for i in range(3)], [max(v[i] for v in rows) for i in range(3)]]


class GLB:
    def __init__(self, path: Path):
        if path.stat().st_size>128*1024*1024:
            raise PackageError('GLB exceeds the 128 MiB import budget')
        data=path.read_bytes()
        if len(data)<20 or struct.unpack_from('<III',data,0)!=(0x46546c67,2,len(data)):
            raise PackageError('Expected a complete GLB 2.0 file')
        chunks=[]
        offset=12
        while offset<len(data):
            if offset+8>len(data):
                raise PackageError('Truncated GLB chunk header')
            length,kind=struct.unpack_from('<II',data,offset)
            if length%4 or offset+8+length>len(data):
                raise PackageError('Invalid GLB chunk length or alignment')
            chunks.append((kind,data[offset+8:offset+8+length]))
            offset+=8+length
        if not chunks or chunks[0][0]!=0x4e4f534a or len(chunks)!=2 or chunks[1][0]!=0x004e4942:
            raise PackageError('GLB must contain one JSON chunk and one embedded BIN chunk')
        self.doc=json.loads(chunks[0][1].decode('utf-8').rstrip('\x00 \r\n\t'))
        self.binary=chunks[1][1]
        if self.doc.get('asset',{}).get('version')!='2.0':
            raise PackageError('glTF asset version must be 2.0')
        if self.doc.get('extensionsRequired'):
            raise PackageError('Required glTF extensions are unsupported: '+', '.join(self.doc['extensionsRequired']))
        if self.doc.get('animations') or self.doc.get('skins'):
            raise PackageError('Version 1 imports static scenery; remove animations and skins before export')
        buffers=self.doc.get('buffers',[])
        if len(buffers)!=1 or 'uri' in buffers[0] or buffers[0]['byteLength']>len(self.binary):
            raise PackageError('GLB requires one embedded buffer with a valid byte length')

    def view(self, index):
        try:
            if type(index) is not int or index<0 or index>=len(self.doc.get('bufferViews',[])):
                raise PackageError('Invalid buffer view index')
            value=self.doc['bufferViews'][index]
            offset=value.get('byteOffset',0);length=value['byteLength']
            if value.get('buffer',0)!=0 or not isinstance(offset,int) or not isinstance(length,int) or offset<0 or length<0 or offset+length>len(self.binary):
                raise PackageError('Buffer view is outside the embedded BIN chunk')
            return self.binary[offset:offset+length],value
        except (IndexError,KeyError,TypeError) as error:
            raise PackageError('Invalid buffer view: '+str(index)) from error

    def accessor(self,index,expected=None):
        try:
            if type(index) is not int or index<0 or index>=len(self.doc.get('accessors',[])):
                raise PackageError('Invalid accessor index')
            item=self.doc['accessors'][index]
            if 'sparse' in item:
                raise PackageError('Sparse accessors are unsupported; export dense mesh attributes')
            code,size=COMPONENTS[item['componentType']]
            width=WIDTHS[item['type']]
            if expected is not None and width not in expected:
                raise PackageError('Accessor has an unexpected vector width')
            data,view=self.view(item['bufferView'])
            offset=item.get('byteOffset',0);stride=view.get('byteStride',size*width);count=item['count']
            if not isinstance(count,int) or count<0 or count>1000000 or offset<0 or stride<size*width or stride%size or (count and offset+(count-1)*stride+size*width>len(data)):
                raise PackageError('Accessor is outside its buffer view or exceeds import limits')
            rows=[struct.unpack_from('<'+code*width,data,offset+i*stride) for i in range(count)]
            if item.get('normalized') and code!='f':
                maximum={5120:127,5121:255,5122:32767,5123:65535}.get(item['componentType'])
                if maximum is None:
                    raise PackageError('Unsupported normalized component type')
                rows=[tuple(max(-1,value/maximum) for value in row) for row in rows]
            if any(not math.isfinite(value) for row in rows for value in row):
                raise PackageError('Mesh contains non-finite attributes')
            return rows
        except (IndexError,KeyError,TypeError) as error:
            raise PackageError('Invalid accessor: '+str(index)) from error

    def image(self,index):
        try:
            if type(index) is not int or index<0 or index>=len(self.doc.get('images',[])):
                raise PackageError('Invalid image index')
            image=self.doc['images'][index]
            if 'uri' in image:
                raise PackageError('Images must be embedded in the GLB')
            data,_=self.view(image['bufferView'])
            mime=image['mimeType']
            if mime not in ('image/png','image/jpeg'):
                raise PackageError('Only embedded PNG and JPEG textures are supported')
            from PIL import Image
            with Image.open(io.BytesIO(data)) as decoded:
                if decoded.width>8192 or decoded.height>8192 or decoded.width*decoded.height>32000000:
                    raise PackageError('Texture exceeds the 8192-pixel edge or 32-megapixel budget')
                output=io.BytesIO();decoded.convert('RGBA').save(output,format='PNG')
            return output.getvalue()
        except (IndexError,KeyError,TypeError) as error:
            raise PackageError('Invalid embedded image: '+str(index)) from error

    def mesh(self, scale=1.0):
        if not isinstance(scale,(int,float)) or not math.isfinite(scale) or scale<=0:
            raise PackageError('Import scale must be finite and positive')
        doc=self.doc;fragments=[];images=[];warnings=[];visiting=set();visited=set()
        scenes=doc.get('scenes',[])
        if scenes:
            try:roots=scenes[doc.get('scene',0)]['nodes']
            except (IndexError,KeyError,TypeError) as error:raise PackageError('Invalid default scene') from error
        else:
            children={child for node in doc.get('nodes',[]) for child in node.get('children',[])}
            roots=[i for i in range(len(doc.get('nodes',[]))) if i not in children]
        def visit(index,parent,depth=0):
            if depth>64 or index in visiting or index in visited:
                raise PackageError('Node graph contains a cycle, duplicate parent or excessive depth')
            if not isinstance(index,int) or index<0 or index>=len(doc.get('nodes',[])):
                raise PackageError('Node index is outside the scene')
            visiting.add(index);visited.add(index);node=doc['nodes'][index]
            if 'skin' in node or node.get('weights'):
                raise PackageError('Skinned and morphing nodes are unsupported')
            matrix=multiply(parent,node_matrix(node))
            if 'mesh' in node:
                try:primitives=doc['meshes'][node['mesh']]['primitives']
                except (IndexError,KeyError,TypeError) as error:raise PackageError('Invalid mesh reference') from error
                for primitive in primitives:
                    if primitive.get('mode',4)!=4 or primitive.get('targets') or primitive.get('extensions'):
                        raise PackageError('Only uncompressed static triangle primitives are supported')
                    attributes=primitive['attributes']
                    extra=set(attributes)-{'POSITION','NORMAL','TEXCOORD_0','COLOR_0','TANGENT'}
                    if extra:
                        raise PackageError('Unsupported mesh attributes: '+', '.join(sorted(extra)))
                    if self.doc['accessors'][attributes['POSITION']]['componentType']!=5126:
                        raise PackageError('POSITION must use float components')
                    positions=self.accessor(attributes['POSITION'],{3})
                    normals=self.accessor(attributes['NORMAL'],{3}) if 'NORMAL' in attributes else None
                    uv=self.accessor(attributes['TEXCOORD_0'],{2}) if 'TEXCOORD_0' in attributes else [(0,0)]*len(positions)
                    colors=self.accessor(attributes['COLOR_0'],{3,4}) if 'COLOR_0' in attributes else [(1,1,1,1)]*len(positions)
                    if any(len(values)!=len(positions) for values in [uv,colors,*([normals] if normals else [])]):
                        raise PackageError('Vertex attribute counts differ')
                    if 'indices' in primitive and self.doc['accessors'][primitive['indices']]['componentType'] not in (5121,5123,5125):
                        raise PackageError('Triangle indices require unsigned integer components')
                    indices=[int(row[0]) for row in self.accessor(primitive['indices'],{1})] if 'indices' in primitive else list(range(len(positions)))
                    if len(indices)%3 or any(i<0 or i>=len(positions) for i in indices):
                        raise PackageError('Triangle indices are invalid')
                    material=doc.get('materials',[])[primitive['material']] if 'material' in primitive else {}
                    unsupported=set(material)-{'name','pbrMetallicRoughness','doubleSided','alphaMode','alphaCutoff','extras'}
                    if unsupported or material.get('alphaMode','OPAQUE')!='OPAQUE':
                        raise PackageError('Unsupported material feature: '+', '.join(sorted(unsupported or {material.get('alphaMode')})))
                    pbr=material.get('pbrMetallicRoughness',{})
                    factor=pbr.get('baseColorFactor',[1,1,1,1])
                    if len(factor)!=4 or any(not isinstance(x,(int,float)) or not math.isfinite(x) or x<0 or x>1 for x in factor):
                        raise PackageError('Invalid baseColorFactor')
                    if pbr.get('metallicRoughnessTexture'):
                        raise PackageError('Metallic/roughness texture is unsupported by the static MT2 material mapping')
                    texture=None
                    if 'baseColorTexture' in pbr:
                        info=pbr['baseColorTexture']
                        if info.get('texCoord',0)!=0 or info.get('extensions'):
                            raise PackageError('Only untransformed TEXCOORD_0 textures are supported')
                        try:source=doc['textures'][info['index']]['source']
                        except (IndexError,KeyError,TypeError) as error:raise PackageError('Invalid base-color texture') from error
                        data=self.image(source)
                        if data not in images:images.append(data)
                        texture=images.index(data)
                    if pbr.get('metallicFactor',1)!=0 or pbr.get('roughnessFactor',1)!=1:
                        if 'Metallic/roughness values are mapped to MT2 diffuse shading' not in warnings:
                            warnings.append('Metallic/roughness values are mapped to MT2 diffuse shading')
                    vertices=[]
                    determinant=sum(matrix[i]*cross(matrix[4:7],matrix[8:11])[i] for i in range(3))
                    for start in range(0,len(indices),3):
                        triangle=indices[start:start+3]
                        if determinant<0:triangle[1],triangle[2]=triangle[2],triangle[1]
                        points=[tuple(v*scale for v in transform(matrix,positions[i])) for i in triangle]
                        face=unit(cross(tuple(points[1][i]-points[0][i] for i in range(3)),tuple(points[2][i]-points[0][i] for i in range(3))))
                        for corner,vertex_index in enumerate(triangle):
                            n=normal(matrix,normals[vertex_index]) if normals else face
                            color=tuple(colors[vertex_index])+((1,) if len(colors[vertex_index])==3 else ())
                            vertices.append((*points[corner],*(factor[j]*color[j] for j in range(4)),*n,*uv[vertex_index]))
                        if len(vertices)==65535:
                            fragments.append({'vertices':vertices,'texture':texture,'double_sided':material.get('doubleSided',False)})
                            vertices=[]
                    if vertices:fragments.append({'vertices':vertices,'texture':texture,'double_sided':material.get('doubleSided',False)})
            for child in node.get('children',[]):visit(child,matrix,depth+1)
            visiting.remove(index)
        for index in roots:visit(index,IDENTITY)
        result=Mesh(fragments,images,warnings)
        if not fragments or result.triangles>250000:
            raise PackageError('Import requires 1..250000 nondegenerate triangles')
        return result


def export(mesh: Mesh, destination: Path, identifier: str, item='scenery'):
    if not ID.fullmatch(identifier) or not item or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789_-' for c in item):
        raise PackageError('Content requires a namespaced mod ID and a lowercase item name')
    if destination.exists():
        raise PackageError('Select an unused scenery package directory')
    namespace=identifier.replace('.','_')+'_'+hashlib.sha256(identifier.encode('utf-8')).hexdigest()[:12]+'_'+item
    variant='mt2fw_'+namespace
    assets=destination/'assets'
    models=assets/'mt2fw'/namespace
    materials=assets/'materials'
    models.mkdir(parents=True);materials.mkdir()
    for index,data in enumerate(mesh.images):(models/f'texture-{index}.png').write_bytes(data)
    lines=['Model','{']
    material_files=[]
    binary_fragments=[]
    for index,fragment in enumerate(mesh.fragments):
        material='mt2fw_'+namespace+'_'+str(index)
        material_file=materials/(material+'.mat');material_files.append(material_file)
        binary_fragments.append({'material':material,'vertices':fragment['vertices']})
        values=['Material','{','    mode lit','    color 1 1 1 1','    zread true','    zwrite true',
                '    culling '+('none' if fragment['double_sided'] else 'back')]
        if fragment['texture'] is not None:values.append(f'    texture "mt2fw/{namespace}/texture-{fragment["texture"]}.png"')
        material_file.write_text('\n'.join(values+['}','']),encoding='utf-8')
        lines+=['    Fragment','    {',f'        Material "{material}"','        PCNTBuffer','        {']
        lines+=['            '+' '.join(format(value,'.9g') for value in row) for row in fragment['vertices']]
        lines+=['        }','        IndexBuffer','        {','            '+' '.join(map(str,range(len(fragment['vertices'])))),
                '        }','        DisplayList','        {','            BindBuffer 0','            TriangleListBuffer 1','        }','    }']
    model=models/'model.vmd';model.write_text('\n'.join(lines+['}','']),encoding='utf-8')
    from . import vmb
    compiled=models/'model.vmb'
    compiled.write_bytes(vmb.encode(binary_fragments,namespace))
    # Decode the emitted payload before any game load; the checked round trip retains float32 bytes.
    decoded=vmb.decode(compiled.read_bytes())
    if vmb.encode(decoded['fragments'],decoded['name'])!=compiled.read_bytes():
        raise PackageError('Generated VMB did not survive its binary round trip')
    variants=assets/'buildings/scenery';variants.mkdir(parents=True)
    (variants/(variant+'.variant')).write_text('\n'.join(['mmoBuildingVariant','{',f'    name "{variant}"',
        f'    modelFile "mt2fw/{namespace}/model.vmb"','    dockPosition 0 0 0','    dockEntry 0 0 0','    pad','    {','    }','}','']),encoding='utf-8')
    display_name=identifier.rsplit('.',1)[-1].replace('_',' ').title()+' '+item.replace('_',' ').title()
    for language in ('english','german'):
        locale=assets/'i18n'/language;locale.mkdir(parents=True)
        (locale/('mt2fw_'+namespace+'.vrt')).write_text('building_variant_scenery_'+variant+'_displayname '+json.dumps(display_name)+'\n',encoding='utf-8')
    atomic_json(destination/'mod.json',{'id':identifier,'name':identifier.rsplit('.',1)[-1].replace('_',' ').title()+' '+item.replace('_',' ').title(),'version':'1.0.0','api_version':'1.0.0',
        'type':'content','entry':'content.json','capabilities':['content'],'multiplayer':'singleplayer'})
    definitions=[{'id':identifier+':'+item,'name':item.replace('_',' ').title(),
        'variant':variant,'model':compiled.relative_to(destination).as_posix(),'material':material_files[0].relative_to(destination).as_posix()}]
    atomic_json(destination/'content.json',definitions)
    (destination/'definitions.lua').write_text('return {\n'+''.join('  { '+', '.join(key+' = '+json.dumps(value,ensure_ascii=False) for key,value in row.items())+' },\n' for row in definitions)+'}\n',encoding='utf-8')
    atomic_json(destination/'import-report.json',{'triangles':mesh.triangles,'bounds':mesh.bounds,'warnings':mesh.warnings,
        'format':'VectorStorm ModelV1 binary for building catalog; editable VMD retained alongside it'})
    return {'directory':str(destination),'id':identifier+':'+item,'variant':variant,'triangles':mesh.triangles,'warnings':mesh.warnings}
