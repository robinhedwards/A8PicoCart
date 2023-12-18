/* A8PicoCart shell
   (c) R.Edwards 2023
*/

include <a8pico_dimensions.scad>


module back() {

difference() {
    union() {
        difference() {
            // main shell
            union() {
                cube([cart_length, cart_width, cartb_height-rim_height]);
                // rim
                translate([0, sides_thickness/2, cartb_height-rim_height])
                    cube([cart_length-sides_thickness/2, sides_thickness/2, rim_height]);
                translate([0, cart_width-sides_thickness, cartb_height-rim_height])
                    cube([cart_length-sides_thickness/2, sides_thickness/2, rim_height]);
                translate([cart_length-sides_thickness, sides_thickness/2, cartb_height-rim_height])
                    cube([sides_thickness/2, cart_width-sides_thickness, rim_height]);
            }
            // make hollow
            translate([0, sides_thickness, front_thickness])
                cube([cart_length-sides_thickness,
                    cart_width-sides_thickness*2,
                    cartb_height-front_thickness]);
        }    
        // end
        cube([sides_thickness, cart_width, 4]);
        // pcb supports
        difference() {
            translate([17, sides_thickness, 0])
                cube([support_thickness, cart_width-sides_thickness*2, cartb_height]);
            translate([17, (cart_width-55)/2, supportb_height])
                cube([support_thickness, 55, cartb_height-supportb_height]);
        }
        translate([60, 0, 0])
            cube([support_thickness, 7, supportb_height]);
        translate([60, cart_width-7, 0])
            cube([support_thickness, 7, supportb_height]);
        // screw holes
        translate([31, (cart_width-screw_sep)/2, 0])
            difference() {
                cylinder(h = supportb_height - 1.2, r1 = 4, r2 = 2.8,  $fs = 1);
                cylinder(h = supportb_height - 1.2, r1 = 1.6, r2 = 1.6,  $fs = 1);
            }
        translate([31, cart_width-(cart_width-screw_sep)/2, 0])
            difference() {
                cylinder(h = supportb_height - 1.2, r1 = 4, r2 = 2.8,  $fs = 1);
                cylinder(h = supportb_height - 1.2, r1 = 1.6, r2 = 1.6,  $fs = 1);
            }
        // ridges
        for (i = [15: 5: cart_length-5])
            union() {
                translate([i, -ridge_width, 0])
                    cube([3, ridge_width, cartb_height-rim_height]);
                translate([i, cart_width, 0])
                    cube([3, ridge_width, cartb_height-rim_height]);
            }
    }
    // screw head
    translate([31, (cart_width-screw_sep)/2, 0])
        cylinder(h = 0.6, r1 = 3.5, r2 = 3.5,  $fs = 1);
    translate([31, (cart_width-screw_sep)/2, 0.6])
        cylinder(h = front_thickness-0.6, r1 = 3.5, r2 = 1.8,  $fs = 1);

    translate([31, cart_width-(cart_width-screw_sep)/2, 0])
        cylinder(h = 0.6, r1 = 3.5, r2 = 3.5,  $fs = 1);
    translate([31, cart_width-(cart_width-screw_sep)/2, 0.6])
        cylinder(h = front_thickness-0.6, r1 = 3.5, r2 = 1.8,  $fs = 1);
     // cutout USB slot
    translate([0, (cart_width-16)/2+0.5, front_thickness])    // pico is 0.5 mm off center
        cube([40, 16, cartb_height]);

}

}


back();
