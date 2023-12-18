use <a8pico_back_v2.scad>
use <a8pico_front.scad>

ridge_width = 0.5;
cart_width=65 - (ridge_width*2);

module logo()
{
// FIXME TBD?
}

module assembly()
{
    back();

    translate ([0,cart_width,30])
        rotate([180,0,0])
            front();
}

item = 0;

if (item == 0) {
    assembly();
}

if (item == 1) {
    front();
}

if (item == 2) {
    back();
}

if (item == 3) {
    logo();
}

if (item == 4) {
    front();
    translate([0,cart_width+10,0]) {
        back();
    }
}
