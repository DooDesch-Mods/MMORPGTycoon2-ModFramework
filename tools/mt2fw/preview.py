"""Software preview of converted static geometry and diffuse textures."""
import io
import math
from PIL import Image, ImageDraw


def render(mesh, yaw=0.7, pitch=0.35, size=440):
    image=Image.new('RGB',(size,size),(16,24,36))
    depth=[float('inf')]*(size*size)
    lower,upper=mesh.bounds
    center=[(a+b)/2 for a,b in zip(lower,upper)]
    span=max(b-a for a,b in zip(lower,upper)) or 1
    scale=size*0.68/span
    textures=[Image.open(io.BytesIO(data)).convert('RGB') for data in mesh.images]
    cosine,sine=math.cos(yaw),math.sin(yaw)
    cp,sp=math.cos(pitch),math.sin(pitch)
    def project(row):
        x,y,z=[row[i]-center[i] for i in range(3)]
        rx,rz=x*cosine+z*sine,-x*sine+z*cosine
        ry,rz=y*cp+rz*sp,-y*sp+rz*cp
        return (size/2+rx*scale,size/2-ry*scale,rz)
    for fragment in mesh.fragments:
        texture=textures[fragment['texture']] if fragment['texture'] is not None else None
        for offset in range(0,len(fragment['vertices']),3):
            vertices=fragment['vertices'][offset:offset+3]
            a,b,c=[project(row) for row in vertices]
            determinant=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(determinant)<0.0001:
                continue
            x0=max(0,int(min(a[0],b[0],c[0])));x1=min(size-1,int(max(a[0],b[0],c[0]))+1)
            y0=max(0,int(min(a[1],b[1],c[1])));y1=min(size-1,int(max(a[1],b[1],c[1]))+1)
            light=max(0.3,min(1.0,0.35+0.65*sum(vertices[0][7+i]*v for i,v in enumerate((-0.3,0.85,0.4)))))
            for y in range(y0,y1+1):
                for x in range(x0,x1+1):
                    px,py=x+0.5,y+0.5
                    u=((b[1]-c[1])*(px-c[0])+(c[0]-b[0])*(py-c[1]))/determinant
                    v=((c[1]-a[1])*(px-c[0])+(a[0]-c[0])*(py-c[1]))/determinant
                    w=1-u-v
                    if min(u,v,w)<-0.00001:
                        continue
                    z=u*a[2]+v*b[2]+w*c[2]
                    index=y*size+x
                    if z>=depth[index]:
                        continue
                    depth[index]=z
                    color=[sum(vertices[j][3+i]*weight for j,weight in enumerate((u,v,w))) for i in range(3)]
                    if texture:
                        tx=sum(vertices[j][10]*weight for j,weight in enumerate((u,v,w)))%1
                        ty=sum(vertices[j][11]*weight for j,weight in enumerate((u,v,w)))%1
                        pixel=texture.getpixel((min(texture.width-1,int(tx*texture.width)),min(texture.height-1,int(ty*texture.height))))
                        color=[color[i]*pixel[i]/255 for i in range(3)]
                    image.putpixel((x,y),tuple(max(0,min(255,round(value*light*255))) for value in color))
    draw=ImageDraw.Draw(image)
    origin=project([0,0,0])
    if 0<=origin[0]<size and 0<=origin[1]<size:
        for axis,color,label in ((0,'#ff8888','X'),(1,'#88dd99','Y'),(2,'#88bbff','Z')):
            endpoint=[0,0,0];endpoint[axis]=span*0.18;end=project(endpoint)
            draw.line((origin[0],origin[1],end[0],end[1]),fill=color,width=2)
            draw.text((end[0],end[1]),label,fill=color)
        draw.ellipse((origin[0]-3,origin[1]-3,origin[0]+3,origin[1]+3),fill='white')
    draw.text((12,size-38),'Origin: (0, 0, 0) | Y up',fill=(184,198,214))
    draw.text((12,size-23),'Bounds: '+' x '.join(f'{b-a:.3g}' for a,b in zip(lower,upper))+' m',fill=(184,198,214))
    return image
