import sys,os,re
from PIL import Image
gl,sw=sys.argv[1],sys.argv[2]
names={}
for l in open(os.path.join(gl,'log.txt'),errors='replace'):
    m=re.match(r'Room changed: .* -> (\S+) \(room (\d+)\)',l)
    if m: names[int(m.group(2))]=m.group(1)
res=[]
for fn in os.listdir(gl):
    m=re.match(r'shot-(\d+)\.png',fn)
    if not m: continue
    f=int(m.group(1)); swp=os.path.join(sw,f'swframe-{f}.ppm')
    if not os.path.exists(swp): continue
    a=Image.open(os.path.join(gl,fn)).convert('RGB'); b=Image.open(swp).convert('RGB')
    pa=a.load(); pb=b.load(); W,H=a.size; n=0; blue=0
    for y in range(H):
        for x in range(W):
            r1,g1,b1=pa[x,y]; r2,g2,b2=pb[x,y]
            if abs(r1-r2)+abs(g1-g2)+abs(b1-b2)>60:
                n+=1
                if b1-b2>60 and b1>r1 and b1>g1: blue+=1
    k=(f-1200)//60
    res.append((n,f,names.get(k,'?'),blue))
res.sort(reverse=True)
for n,f,name,blue in res[:int(sys.argv[3]) if len(sys.argv)>3 else 30]:
    print(f"frame {f:6d} {name:16s} diff {n:6d} blue {blue:6d}")
print('rooms compared',len(res),'| >500 px:',sum(1 for r in res if r[0]>500),'| >2000 px:',sum(1 for r in res if r[0]>2000),'| identical:',sum(1 for r in res if r[0]==0))
