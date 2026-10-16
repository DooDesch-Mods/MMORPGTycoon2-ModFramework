"""Own static VMB writer, based on VectorStorm's documented serializer contract.

Integer counts and string lengths use network byte order; float32 values use
the Windows engine's native little-endian representation. No game data is used.
"""
import io
import struct
from .packages import PackageError


def string(value):
    data=value.encode('utf-8')
    if len(data)>32767:
        raise PackageError('VMB string exceeds signed 16-bit length')
    return struct.pack('>h',len(data))+data


def encode(fragments, name='MT2FW'):
    output=bytearray(string('ModelV1')+string(name))
    output+=struct.pack('<10f',0,0,0,0,0,0,1,1,1,1)
    output+=struct.pack('>i',len(fragments))
    for fragment in fragments:
        rows=fragment['vertices']
        if not rows or len(rows)>65535 or len(rows)%3:
            raise PackageError('Each VMB fragment requires 1..65535 vertices in triangles')
        output+=string('Fragment')+string(fragment['material'])+string('PCNT')+struct.pack('>i',len(rows))
        for row in rows:
            if len(row)!=12:
                raise PackageError('PCNT rows require twelve float values')
            output+=struct.pack('<12f',*row)
        output+=string('IndexBuffer')+struct.pack('>i',len(rows))
        output+=struct.pack('>'+'i'*len(rows),*range(len(rows)))
    output+=struct.pack('>i',0)
    return bytes(output)


def decode(data):
    stream=io.BytesIO(data)
    def read(count):
        result=stream.read(count)
        if len(result)!=count:
            raise PackageError('Truncated VMB')
        return result
    def text():
        size=struct.unpack('>h',read(2))[0]
        if size<0:raise PackageError('Negative VMB string size')
        return read(size).decode('utf-8')
    def count():
        value=struct.unpack('>i',read(4))[0]
        if value<0 or value>1000000:raise PackageError('Invalid VMB count')
        return value
    if text()!='ModelV1':raise PackageError('Expected static ModelV1')
    name=text();pose=struct.unpack('<10f',read(40));fragments=[]
    for _ in range(count()):
        if text()!='Fragment':raise PackageError('Expected Fragment')
        material=text()
        if text()!='PCNT':raise PackageError('Expected PCNT vertices')
        vertices=[struct.unpack('<12f',read(48)) for _ in range(count())]
        if text()!='IndexBuffer':raise PackageError('Expected IndexBuffer')
        indices=[struct.unpack('>i',read(4))[0] for _ in range(count())]
        if indices!=list(range(len(vertices))):raise PackageError('Expected deindexed static geometry')
        fragments.append({'material':material,'vertices':vertices})
    if count()!=0 or stream.read(1):raise PackageError('Unexpected VMB children or trailing data')
    return {'name':name,'pose':pose,'fragments':fragments}
