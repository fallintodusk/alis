"""Independent barycentric oracle for captured contact regressions."""


def cross(a,b,c):
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])


def height(face,point):
    a,b,c=face
    return (cross(point,b,c)*a[2]+cross(a,point,c)*b[2]+cross(a,b,point)*c[2])/cross(a,b,c)


def contains(face,point):
    sign=1 if cross(*face)>0 else -1
    return all(sign*cross(a,b,point)>=0 for a,b in zip(face,(*face[1:],face[0])))
