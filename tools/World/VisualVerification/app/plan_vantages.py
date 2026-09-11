"""Derive camera vantages from World Partition actor descriptors."""
import json, math, re, sys
from collections import Counter

FOV_DEG = 90.0

def parse(path):
    pat = re.compile(
        r"NativeClass:(?P<cls>\S+).*?\bName:(?P<name>\S+).*?"
        r"RuntimeBounds:IsValid=\w+, Min=\(X=(?P<x0>[-\d.]+) Y=(?P<y0>[-\d.]+) Z=(?P<z0>[-\d.]+)\), "
        r"Max=\(X=(?P<x1>[-\d.]+) Y=(?P<y1>[-\d.]+) Z=(?P<z1>[-\d.]+)\)")
    out = []
    for line in open(path, encoding="utf-8", errors="replace"):
        m = pat.search(line)
        if m:
            d = m.groupdict()
            for k in ("x0","y0","z0","x1","y1","z1"): d[k] = float(d[k])
            out.append(d)
    return out

CAPTURE_WIDTH = 1920
CAPTURE_HEIGHT = 1080


def altitude_for(span_world_x, span_world_y):
    """Altitude at which BOTH world spans fit the captured frame.

    FOV_DEG is the HORIZONTAL field of view. On a 16:9 capture the vertical
    field is materially narrower, so solving only the horizontal axis silently
    crops the taller world dimension -- which is the exact partial-framing
    failure this planner exists to prevent. For a nadir camera at yaw 0, screen
    X maps to world Y and screen Y maps to world X, so each world axis is
    solved against the field that actually covers it.
    """
    half_h = math.radians(FOV_DEG / 2.0)
    aspect = CAPTURE_WIDTH / CAPTURE_HEIGHT
    half_v = math.atan(math.tan(half_h) / aspect)
    return max(span_world_y / 2.0 / math.tan(half_h),
               span_world_x / 2.0 / math.tan(half_v))

def main(desc_path, out_path):
    rows = parse(desc_path)
    terr = [r for r in rows if "LandscapeStreamingProxy" in r["cls"]]
    water = [r for r in rows if "Water" in r["name"]]
    if not terr:
        print("FATAL: no landscape proxies", file=sys.stderr); return 2

    X0=min(r["x0"] for r in terr); X1=max(r["x1"] for r in terr)
    Y0=min(r["y0"] for r in terr); Y1=max(r["y1"] for r in terr)
    Zmin=min(r["z0"] for r in terr); Zmax=max(r["z1"] for r in terr)
    cx,cy=(X0+X1)/2,(Y0+Y1)/2
    span=max(X1-X0, Y1-Y0)

    V=[]
    def add(name,x,y,z,pitch,yaw,note):
        V.append(dict(name=name,x=round(x),y=round(y),z=round(z),
                      pitch=pitch,yaw=yaw,note=note))

    # 1. True top-down overview framing the entire territory.
    add("01_overview_topdown", cx, cy, altitude_for(X1-X0, Y1-Y0)*1.05, -90, 0,
        "entire territory, nadir")
    # 2-5. Four oblique corners looking at the centre.
    half=span/2.0
    obl_alt=altitude_for(X1-X0, Y1-Y0)*0.55
    for i,(dx,dy,yaw) in enumerate(
            [(-1,-1,45),(1,-1,135),(1,1,-135),(-1,1,-45)],start=2):
        add(f"{i:02d}_oblique_{'sw' if (dx<0 and dy<0) else 'se' if dy<0 else 'ne' if dx>0 else 'nw'}",
            cx+dx*half*1.15, cy+dy*half*1.15, obl_alt, -30, yaw,
            "oblique corner toward centre")
    # 6-8. Densest water clusters -> proves water layer is present and placed.
    BIN=200000
    bins={}
    for r in water:
        mx,my=(r["x0"]+r["x1"])/2,(r["y0"]+r["y1"])/2
        mz=(r["z0"]+r["z1"])/2
        bins.setdefault((int(mx//BIN),int(my//BIN)),[]).append((mx,my,mz))
    ranked=sorted(bins.items(), key=lambda kv:-len(kv[1]))[:3]
    for i,(k,pts) in enumerate(ranked,start=6):
        ax=sum(p[0] for p in pts)/len(pts); ay=sum(p[1] for p in pts)/len(pts)
        az=sum(p[2] for p in pts)/len(pts)
        add(f"{i:02d}_water_cluster_{len(pts)}", ax, ay-180000, az+120000, -35, 90,
            f"{len(pts)} water actors in 2km bin; standoff heuristic, not derived")
    # 9. Highest terrain, close oblique -> proves relief is real at human scale.
    hi=max(terr,key=lambda r:r["z1"])
    hx,hy=(hi["x0"]+hi["x1"])/2,(hi["y0"]+hi["y1"])/2
    add("09_relief_high", hx, hy-90000, hi["z1"]+45000, -25, 90, "highest proxy")

    # The capture route MUST render at the same width, height and field of view the altitude
    # solve above assumed. Emitting them here keeps the solve and the capture bound to one
    # authority; passing them separately would let the authored-altitude failure class back in.
    doc=dict(
        capture_width=CAPTURE_WIDTH, capture_height=CAPTURE_HEIGHT, fov_degrees=FOV_DEG,
        territory=dict(min=[X0,Y0,Zmin],max=[X1,Y1,Zmax],
                       center=[cx,cy],span_uu=span,span_m=span/100.0,
                       relief_m=(Zmax-Zmin)/100.0),
        counts=dict(landscape_proxies=len(terr),water_actors=len(water),
                    total_descs=len(rows)),
        required_overview_altitude_uu=altitude_for(X1-X0, Y1-Y0),
        vantages=V)
    json.dump(doc, open(out_path,"w"), indent=2)
    print(f"territory {span/100:.0f} m span, relief {(Zmax-Zmin)/100:.1f} m")
    print(f"proxies={len(terr)} water={len(water)} descs={len(rows)}")
    print(f"vantages={len(V)} -> {out_path}")
    for v in V:
        print(f"  {v['name']:26s} ({v['x']:>9},{v['y']:>9},{v['z']:>8}) p={v['pitch']:>4} y={v['yaw']:>5}  {v['note']}")
    return 0

if __name__=="__main__":
    sys.exit(main(sys.argv[1],sys.argv[2]))
