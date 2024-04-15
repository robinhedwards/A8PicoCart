
module cube_round(dim=[1,1,1],r=0.3, fn = 16, round_top = true)
{
    assert(dim[0]>=2*r);
    assert(dim[1]>=2*r);
    assert(dim[2]>=2*r);
    echo ("round cube",dim,r,fn);
    hull() {
        translate([r,r,r])
        sphere(r=r, $fn=fn);
        translate([r,dim[1]-r,r])
        sphere(r=r, $fn=fn);
        translate([dim[0]-r,r,r])
        sphere(r=r, $fn=fn);
        translate([dim[0]-r,dim[1]-r,r])
        sphere(r=r, $fn=fn);
if (round_top == true) {
        translate([r,r,dim[2]-r])
        sphere(r=r, $fn=fn);
        translate([r,dim[1]-r,dim[2]-r])
        sphere(r=r, $fn=fn);
        translate([dim[0]-r,r,dim[2]-r])
        sphere(r=r, $fn=fn);
        translate([dim[0]-r,dim[1]-r,dim[2]-r])
        sphere(r=r, $fn=fn);
    } else {
        translate([r,r,dim[2]-r])
        cylinder(r=r,h=r, $fn=fn);
        translate([r,dim[1]-r,dim[2]-r])
        cylinder(r=r,h=r, $fn=fn);
        translate([dim[0]-r,r,dim[2]-r])
        cylinder(r=r,h=r, $fn=fn);
        translate([dim[0]-r,dim[1]-r,dim[2]-r])
        cylinder(r=r,h=r, $fn=fn);
    }

    }
}

