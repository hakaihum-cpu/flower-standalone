import cv2, math, json, hashlib
from pathlib import Path
import numpy as np
import trimesh, vtk
from vtk.util import numpy_support
from skimage import measure
from scipy.ndimage import gaussian_filter, binary_closing, binary_opening
from PIL import Image, ImageOps, ImageEnhance, ImageDraw

OUT=Path('/mnt/data/flower3d_hull_v12c'); OUT.mkdir(parents=True,exist_ok=True)
SHEET=Image.open('/mnt/data/1000005621.png').convert('RGB')
FRONT_PATH=Path('/mnt/data/front_dir_crop.png')
NX=86; NY=86; NZ=180

# 8 unique walk cells from the locked motion sheet (one duplicate source cell omitted)
X_EDGES=[14,114,213,313,413,513,612,712,812,898]
USE=[0,1,2,3,4,6,7,8]
Y0,Y1=38,259

def segment_uniform(rgb, threshold=10):
    bgr=cv2.cvtColor(rgb,cv2.COLOR_RGB2BGR)
    lab=cv2.cvtColor(bgr,cv2.COLOR_BGR2LAB).astype(np.float32)
    border=np.concatenate([lab[:4].reshape(-1,3),lab[-4:].reshape(-1,3),lab[:,:4].reshape(-1,3),lab[:,-4:].reshape(-1,3)])
    bg=np.median(border,axis=0)
    dist=np.linalg.norm(lab-bg,axis=2)
    mask=dist>threshold
    mask[:2]=False;mask[-2:]=False;mask[:,:2]=False;mask[:,-2:]=False
    mask=binary_closing(mask,iterations=1); mask=binary_opening(mask,iterations=1)
    n,lbl,stats,cent=cv2.connectedComponentsWithStats(mask.astype(np.uint8))
    out=np.zeros_like(mask)
    for i in range(1,n):
        if stats[i,cv2.CC_STAT_AREA]>18:
            out[lbl==i]=True
    return out

def crop_by_mask(rgb,mask,pad=2):
    ys,xs=np.nonzero(mask)
    if len(xs)==0: raise RuntimeError('empty mask')
    x0=max(0,xs.min()-pad);x1=min(mask.shape[1]-1,xs.max()+pad);y0=max(0,ys.min()-pad);y1=min(mask.shape[0]-1,ys.max()+pad)
    return rgb[y0:y1+1,x0:x1+1],mask[y0:y1+1,x0:x1+1]

def prep_view(rgb,w,h,mask_override=None):
    mask=segment_uniform(rgb) if mask_override is None else mask_override.astype(bool)
    rgb,mask=crop_by_mask(rgb,mask)
    # Keep empty margin around the silhouette so marching cubes never clips
    # the head, hair or shoes at the visual-hull volume boundary.
    pad_y=max(4,int(rgb.shape[0]*0.035)); pad_x=max(4,int(rgb.shape[1]*0.055))
    border=np.median(np.concatenate([rgb[:2].reshape(-1,3),rgb[-2:].reshape(-1,3),rgb[:,:2].reshape(-1,3),rgb[:,-2:].reshape(-1,3)],axis=0),axis=0).astype(np.uint8)
    padded=np.empty((rgb.shape[0]+2*pad_y,rgb.shape[1]+2*pad_x,3),dtype=np.uint8); padded[:]=border
    padded[pad_y:pad_y+rgb.shape[0],pad_x:pad_x+rgb.shape[1]]=rgb
    pm=np.zeros((mask.shape[0]+2*pad_y,mask.shape[1]+2*pad_x),dtype=np.uint8)
    pm[pad_y:pad_y+mask.shape[0],pad_x:pad_x+mask.shape[1]]=mask.astype(np.uint8)
    im=cv2.resize(padded,(w,h),interpolation=cv2.INTER_AREA)
    mk=cv2.resize(pm,(w,h),interpolation=cv2.INTER_NEAREST)>0
    return im, mk

def build_hull(front_rgb,side_rgb,front_mask=None):
    fimg,fm=prep_view(front_rgb,NY,NZ,front_mask)
    simg,sm=prep_view(side_rgb,NX,NZ)
    # arrays image order [z_from_top, x]; transpose then flip z axis
    fmyz=fm.T[:,::-1]
    smxz=sm.T[:,::-1]
    occ=smxz[:,None,:] & fmyz[None,:,:]
    field=gaussian_filter(occ.astype(np.float32),sigma=(1.0,1.0,.75))
    verts,faces,normals,vals=measure.marching_cubes(field,level=.44,spacing=(1/(NX-1),1/(NY-1),1/(NZ-1)))
    verts[:,0]=(verts[:,0]-.5)*.62   # front/back depth
    verts[:,1]=(verts[:,1]-.5)*.48   # body width
    verts[:,2]=verts[:,2]*1.64
    mesh=trimesh.Trimesh(vertices=verts,faces=faces,process=True)
    trimesh.smoothing.filter_taubin(mesh,lamb=.43,nu=-.46,iterations=4)
    # Remove tiny disconnected background artefacts while preserving the actor.
    components=mesh.split(only_watertight=False)
    if len(components)>1:
        largest=max(p.area for p in components)
        keep=[p for p in components if p.area >= largest*0.05]
        mesh=trimesh.util.concatenate(keep)

    # Texture the hull from the actual locked reference crops.
    fg=cv2.cvtColor(fimg,cv2.COLOR_RGB2GRAY).T[:,::-1]
    sg=cv2.cvtColor(simg,cv2.COLOR_RGB2GRAY).T[:,::-1]
    v=mesh.vertices; vn=mesh.vertex_normals
    xi=np.clip(np.round((v[:,0]/.62+.5)*(NX-1)).astype(int),0,NX-1)
    yi=np.clip(np.round((v[:,1]/.48+.5)*(NY-1)).astype(int),0,NY-1)
    zi=np.clip(np.round(v[:,2]/1.64*(NZ-1)).astype(int),0,NZ-1)
    side=sg[xi,zi].astype(float); front=fg[yi,zi].astype(float)
    ws=np.abs(vn[:,1]); wf=np.abs(vn[:,0]); denom=ws+wf+1e-6
    gray=(side*ws+front*wf)/denom
    gray=np.clip((gray-128)*1.06+128,8,245).astype(np.uint8)
    mesh.visual.vertex_colors=np.c_[gray,gray,gray,np.full(len(gray),255,np.uint8)]
    return mesh

def vtk_actor(mesh):
    pts=vtk.vtkPoints();pts.SetData(numpy_support.numpy_to_vtk(np.asarray(mesh.vertices,float),deep=True));cells=vtk.vtkCellArray()
    for f in np.asarray(mesh.faces,np.int64):
        t=vtk.vtkTriangle();[t.GetPointIds().SetId(i,int(f[i])) for i in range(3)];cells.InsertNextCell(t)
    pd=vtk.vtkPolyData();pd.SetPoints(pts);pd.SetPolys(cells)
    cols=numpy_support.numpy_to_vtk(np.asarray(mesh.visual.vertex_colors,np.uint8),deep=True,array_type=vtk.VTK_UNSIGNED_CHAR);cols.SetNumberOfComponents(4);pd.GetPointData().SetScalars(cols)
    n=vtk.vtkPolyDataNormals();n.SetInputData(pd);n.ComputePointNormalsOn();n.SplittingOff();n.ConsistencyOn();n.Update()
    mp=vtk.vtkPolyDataMapper();mp.SetInputData(n.GetOutput());mp.SetScalarModeToUsePointData();mp.SetColorModeToDirectScalars();mp.ScalarVisibilityOn()
    a=vtk.vtkActor();a.SetMapper(mp);a.GetProperty().SetInterpolationToPhong();a.GetProperty().SetAmbient(.68);a.GetProperty().SetDiffuse(.32);a.GetProperty().SetSpecular(0);return a

def render(mesh,path,azimuth_deg=0,w=384,h=640):
    # azimuth 0 = locked side view, positive pivots slightly toward front.
    r=vtk.vtkRenderer();r.SetBackground(0,0,0);r.SetBackgroundAlpha(0);r.AddActor(vtk_actor(mesh));c=r.GetActiveCamera()
    rad=math.radians(azimuth_deg)
    dist=5.2
    # base camera is (0,-dist,z), rotate around z toward +x
    c.SetPosition(dist*math.sin(rad),-dist*math.cos(rad),.82);c.SetFocalPoint(0,0,.82);c.SetViewUp(0,0,1);c.ParallelProjectionOn();c.SetParallelScale(.86);r.ResetCameraClippingRange()
    win=vtk.vtkRenderWindow();win.SetOffScreenRendering(1);win.SetAlphaBitPlanes(1);win.SetMultiSamples(8);win.AddRenderer(r);win.SetSize(w,h);win.Render()
    g=vtk.vtkWindowToImageFilter();g.SetInput(win);g.SetInputBufferTypeToRGBA();g.ReadFrontBufferOff();g.Update();wr=vtk.vtkPNGWriter();wr.SetFileName(str(path));wr.SetInputConnection(g.GetOutputPort());wr.Write()

def post(path):
    im=Image.open(path).convert('RGBA');a=im.getchannel('A');gray=ImageOps.grayscale(im);gray=ImageEnhance.Contrast(gray).enhance(1.05)
    arr=np.asarray(gray).astype(np.int16);aa=np.asarray(a);rng=np.random.default_rng(20260928);arr=np.clip(arr+rng.normal(0,1.1,arr.shape),0,255).astype(np.uint8);arr[aa==0]=0
    Image.merge('RGBA',(Image.fromarray(arr),)*3+(a,)).save(path)

def main():
    front_rgb=np.array(Image.open(FRONT_PATH).convert('RGB'))
    front_mask=np.array(Image.open('/mnt/data/front_mask_clean.png').convert('L'))>127
    # remove the thin floor line left by source segmentation
    front_mask[-4:,:]=False
    walk_meshes=[]; frames=[]
    scene=trimesh.Scene()
    for out_idx,cell in enumerate(USE,1):
        side_rgb=np.array(SHEET.crop((X_EDGES[cell]+2,Y0+2,X_EDGES[cell+1]-2,Y1-2)))
        mesh=build_hull(front_rgb,side_rgb,front_mask)
        walk_meshes.append(mesh)
        mcopy=mesh.copy();mcopy.apply_translation([(out_idx-1)*.72,0,0]);scene.add_geometry(mcopy,node_name=f'walk_{out_idx:02d}',geom_name=f'walk_{out_idx:02d}')
        p=OUT/f'walk_{out_idx:02d}.png';render(mesh,p,0);post(p);frames.append(Image.open(p).convert('RGBA'))
    scene.export(OUT/'student01_hull_v12c_walk8.glb')
    frames[0].save(OUT/'student01_hull_v12c_walk.gif',save_all=True,append_images=frames[1:],duration=125,loop=0,disposal=2)

    # Static model from direction-reference side image for a small 3D parallax proof.
    side_static=np.array(Image.open('/mnt/data/side_dir_crop.png').convert('RGB'))
    neutral=build_hull(front_rgb,side_static,front_mask);trimesh.Scene(neutral).export(OUT/'student01_hull_v12c_neutral.glb')
    turn=[]
    for idx,az in enumerate([-14,-8,-3,3,8,14,8,3,-3,-8],1):
        p=OUT/f'turn_{idx:02d}.png';render(neutral,p,az);post(p);turn.append(Image.open(p).convert('RGBA'))
    turn[0].save(OUT/'student01_hull_v12c_parallax.gif',save_all=True,append_images=turn[1:],duration=110,loop=0,disposal=2)

    # Visual comparison board: reference side / rendered side / slight 3D angle.
    ref=Image.open('/mnt/data/side_dir_crop.png').convert('RGB').resize((260,550),Image.Resampling.LANCZOS)
    side=Image.open(OUT/'turn_05.png').convert('RGBA');bg=Image.new('RGBA',side.size,(150,150,150,255));bg.alpha_composite(side);side=bg.convert('RGB').resize((330,550),Image.Resampling.LANCZOS)
    angled=Image.open(OUT/'turn_10.png').convert('RGBA');bg2=Image.new('RGBA',angled.size,(150,150,150,255));bg2.alpha_composite(angled);angled=bg2.convert('RGB').resize((330,550),Image.Resampling.LANCZOS)
    board=Image.new('RGB',(920,610),(25,25,25));d=ImageDraw.Draw(board);d.text((12,12),'FLOWER student_01 — photographic 3D visual hull v12',fill='white');board.paste(ref,(10,50));board.paste(side,(285,50));board.paste(angled,(625,50));d.text((90,575),'LOCKED REF',fill='white');d.text((390,575),'3D SIDE',fill='white');d.text((735,575),'3D +14deg',fill='white');board.save(OUT/'student01_hull_v12c_review.png')

    report=[]
    for i in range(1,9):
        arr=np.asarray(Image.open(OUT/f'walk_{i:02d}.png').convert('RGBA'));mask=arr[...,3]==0;report.append({'frame':i,'transparent_rgb_max':int(arr[...,:3][mask].max()) if mask.any() else 0})
    (OUT/'report.json').write_text(json.dumps(report,indent=2))

if __name__=='__main__':main()