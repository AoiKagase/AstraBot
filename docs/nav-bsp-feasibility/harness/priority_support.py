"""Priority-area hull-support and trace-window sensitivity; not movement simulation."""
import argparse
import collections
import json
from pathlib import Path
from diagnose import Nav, Bsp, PRIORITY, write_csv


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--maps',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    args=p.parse_args()
    nav=Nav((args.maps/'de_dust2.nav').read_bytes())
    bsp=Bsp((args.maps/'de_dust2.bsp').read_bytes())
    rows=[]
    for identity in PRIORITY:
        area=nav.by_id[identity]
        for ix in range(15):
            for iy in range(15):
                x=area.g[0]+(area.g[3]-area.g[0])*(.1+.8*ix/14)
                y=area.g[1]+(area.g[4]-area.g[1])*(.1+.8*iy/14)
                nz=area.z(x,y)
                row=dict(area=identity,ix=ix,iy=iy,x=x,y=y,nav_z=nz)
                for hull,height,label in ((0,0,'point'),(1,36,'standing'),(3,18,'crouching')):
                    valid=[]
                    for window in (18,36,64):
                        start=(x,y,nz+height+window)
                        end=(x,y,nz+height-128)
                        hit=bsp.scene_trace(start,end,hull)
                        status='missing' if hit is None else 'start_solid' if hit['fraction']==0 else 'resolved'
                        row[f'{label}_{window}_status']=status
                        if status=='resolved':
                            feet=hit['z']-height
                            row[f'{label}_{window}_feet_z']=feet
                            row[f'{label}_{window}_error']=nz-feet
                            row[f'{label}_{window}_normal_z']=hit['normal'][2]
                            valid.append((window,feet,hit['normal'][2]))
                    if valid:
                        # First free start minimizes the risk of selecting an upper floor.
                        w,f,n=valid[0]
                        row[f'{label}_selected_window']=w
                        row[f'{label}_support_z']=f
                        row[f'{label}_support_error']=nz-f
                        row[f'{label}_support_normal_z']=n
                        row[f'{label}_window_disagreement']=int(max(v[1] for v in valid)-min(v[1] for v in valid)>2)
                rows.append(row)
    write_csv(args.out/'priority_hull_support.csv',rows)
    result=[]
    for identity in PRIORITY:
        aa=[r for r in rows if r['area']==identity]
        record=dict(area=identity,geometry=nav.by_id[identity].g,attributes=nav.by_id[identity].attr,samples=len(aa))
        for label in ('point','standing','crouching'):
            resolved=[r for r in aa if f'{label}_support_error' in r]
            errors=[abs(r[f'{label}_support_error']) for r in resolved]
            record[label]=dict(resolved=len(resolved),max_error=max(errors) if errors else None,
                               error_gt18=sum(v>18 for v in errors),
                               window_disagreement=sum(r.get(f'{label}_window_disagreement',0) for r in aa),
                               selected_windows=dict(collections.Counter(r[f'{label}_selected_window'] for r in resolved)))
        result.append(record)
    (args.out/'priority_hull_summary.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result))


if __name__=='__main__':
    main()
