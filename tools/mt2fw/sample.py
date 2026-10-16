"""Small authored assets used by the scenery template and converter acceptance."""
import io
import json
import struct


def encode(document, binary):
    text=json.dumps(document,separators=(',',':')).encode('utf-8')
    text+=b' '*((-len(text))%4)
    binary+=b'\0'*((-len(binary))%4)
    chunks=struct.pack('<II',len(text),0x4e4f534a)+text+struct.pack('<II',len(binary),0x004e4942)+binary
    return struct.pack('<III',0x46546c67,2,12+len(chunks))+chunks


def pyramid(color=(0.2,0.7,1.0,1.0), textured=False):
    positions=[(0,4,0),(-2,0,-2),(2,0,-2),(2,0,2),(-2,0,2)]
    indices=[0,2,1,0,3,2,0,4,3,0,1,4,1,2,3,1,3,4]
    uv=[(0.5,0),(0,1),(1,1),(1,0),(0,0)]
    points=b''.join(struct.pack('<fff',*p) for p in positions)
    index_bytes=struct.pack('<'+'H'*len(indices),*indices)
    uv_bytes=b''.join(struct.pack('<ff',*row) for row in uv)
    binary=points+index_bytes+uv_bytes
    document={
        'asset':{'version':'2.0','generator':'DooDesch MT2 Framework sample'},
        'buffers':[{'byteLength':len(binary)}],
        'bufferViews':[{'buffer':0,'byteOffset':0,'byteLength':len(points)},
                       {'buffer':0,'byteOffset':len(points),'byteLength':len(index_bytes)},
                       {'buffer':0,'byteOffset':len(points)+len(index_bytes),'byteLength':len(uv_bytes)}],
        'accessors':[{'bufferView':0,'componentType':5126,'count':5,'type':'VEC3'},
                     {'bufferView':1,'componentType':5123,'count':len(indices),'type':'SCALAR'},
                     {'bufferView':2,'componentType':5126,'count':5,'type':'VEC2'}],
        'materials':[{'pbrMetallicRoughness':{'baseColorFactor':list(color),'metallicFactor':0,'roughnessFactor':1},'doubleSided':True}],
        'meshes':[{'primitives':[{'attributes':{'POSITION':0,'TEXCOORD_0':2},'indices':1,'material':0}]}],
        'nodes':[{'mesh':0}],'scenes':[{'nodes':[0]}],'scene':0,
    }
    if textured:
        from PIL import Image
        image=Image.new('RGBA',(8,8),(245,190,45,255))
        for y in range(8):
            for x in range(8):
                if (x//2+y//2)%2:image.putpixel((x,y),(35,100,230,255))
        stream=io.BytesIO();image.save(stream,format='PNG');data=stream.getvalue()
        document['bufferViews'].append({'buffer':0,'byteOffset':len(binary),'byteLength':len(data)})
        document['images']=[{'bufferView':3,'mimeType':'image/png'}]
        document['textures']=[{'source':0}]
        document['materials'][0]['pbrMetallicRoughness']['baseColorTexture']={'index':0}
        binary+=data
        document['buffers'][0]['byteLength']=len(binary)
    return encode(document,binary)
