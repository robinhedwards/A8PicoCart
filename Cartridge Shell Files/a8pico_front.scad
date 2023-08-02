/* A8PicoCart shell
   (c) R.Edwards 2023
*/

ridge_width = 0.5;
cart_length=80;
cart_width=65 - (ridge_width*2);
cart_height=9.6;
screw_sep = 44;
front_thickness = 1.6;
sides_thickness = 1.6 - 0.05;
support_thickness = 1.2 - 0.05;
support_height = 7.6;
rim_height = 1;
logo_width = 14.5;
logo_height = 58;

difference() {
    union() {
        difference() {
            // main shell
            cube([cart_length, cart_width, cart_height]);
            // rims
            translate([0, sides_thickness/2, cart_height-rim_height])
                cube([cart_length-sides_thickness/2,
                    cart_width-sides_thickness, rim_height]);
            // make hollow
            translate([0, sides_thickness, front_thickness])
                cube([cart_length-sides_thickness,
                    cart_width-sides_thickness*2,
                    cart_height-front_thickness]);
            // cutout cart port
            translate([0, (cart_width-57)/2, 0])
                cube([10, 57, cart_height]);
            // button
            translate([cart_length-15, 9, 0])
               cylinder(h = front_thickness, r1 = 2.5, r2 = 2.5,  $fs = 1); 
        }    
        // pcb supports
        difference() {
            translate([17, 0, 0])
                cube([support_thickness, cart_width, cart_height-rim_height]);
            translate([17, (cart_width-55)/2, support_height])
                cube([support_thickness, 55, cart_height-support_height]);
        }
        translate([60, 0, 0])
            cube([support_thickness, 7, support_height]);
        translate([60, cart_width-7, 0])
            cube([support_thickness, 7, support_height]);
        // screw holes
        translate([31, (cart_width-screw_sep)/2, 0])
            difference() {
                cylinder(h = support_height + 2.5, r1 = 3.2, r2 = 2.8,  $fs = 1);
                cylinder(h = support_height + 2.5, r1 = 1.25, r2 = 1.25,  $fs = 1);
            }
        translate([31, cart_width-(cart_width-screw_sep)/2, 0])
            difference() {
                cylinder(h = support_height + 2.5, r1 = 3.2, r2 = 2.8,  $fs = 1);
                cylinder(h = support_height + 2.5, r1 = 1.25, r2 = 1.25,  $fs = 1);
            }
        // ridges
        for (i = [15: 5: cart_length-5])
            union() {
                translate([i, -ridge_width, 0])
                    cube([3, ridge_width, cart_height]);
                translate([i, cart_width, 0])
                    cube([3, ridge_width, cart_height]);
            }
    }
    // logo space
    translate([cart_length-logo_height-2, cart_width-logo_width-3.5, 0])
        cube([logo_height, logo_width, 1]);
}