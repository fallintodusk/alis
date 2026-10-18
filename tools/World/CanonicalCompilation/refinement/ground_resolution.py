"""Finite per-quad sampling with shared stations, without changing the source field."""
from ..app.contracts import CompilerError


def _refuse(message):
    raise CompilerError('ground_surface_invalid',message)


def station_sets(domain,layouts,resolution,budget):
    if not isinstance(resolution,list) or len(resolution)!=len(domain):
        _refuse('Ground resolution must declare every selected quad exactly once')
    indexed={(key,x,y):bounds for bounds,(key,x,y,_) in domain.items()}
    strides={}
    for row in resolution:
        if (not isinstance(row,dict) or set(row)!={'cell_id','quad','stride'} or
            not isinstance(row['cell_id'],str) or not isinstance(row['quad'],list) or len(row['quad'])!=2 or
            any(type(v)is not int for v in row['quad']) or type(row['stride'])is not int or row['stride']<1):
            _refuse('Ground resolution needs an explicit cell, integer quad and positive integer stride')
        key=(row['cell_id'],*row['quad'])
        if key not in indexed or key in strides:
            _refuse('Ground resolution selects an unknown or duplicate quad')
        nx=layouts[indexed[key]][2];stride=row['stride']
        if nx%stride or (nx//stride-1)**2>budget:
            _refuse('Ground resolution must divide its source stations and fit the final-quad capacity')
        strides[key]=stride
    requests={};edges={}
    for bounds,(key,x,y,_) in domain.items():
        x0,y0,nx=layouts[bounds];stride=strides[key,x,y]
        requests[bounds]={(i,j) for j in range(0,nx+1,stride) for i in range(0,nx+1,stride)}
        for a,b,axis,value in (((x0,y0),(x0+nx,y0),1,0),((x0,y0+nx),(x0+nx,y0+nx),1,nx),
            ((x0,y0),(x0,y0+nx),0,0),((x0+nx,y0),(x0+nx,y0+nx),0,nx)):
            edge=tuple(sorted((a,b)))
            nodes={((x0+i,y0+j)) for i,j in requests[bounds] if (i,j)[axis]==value}
            edges.setdefault(edge,[]).append((bounds,nodes))
    for entries in edges.values():
        if len(entries)>2:_refuse('Ground resolution has nonmanifold selected edges')
        shared=set().union(*(nodes for _,nodes in entries))
        for bounds,_ in entries:
            x0,y0,_=layouts[bounds]
            requests[bounds].update((x-x0,y-y0) for x,y in shared)
    return requests
