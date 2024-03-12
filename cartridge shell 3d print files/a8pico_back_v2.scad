/* A8PicoCart shell
   (c) R.Edwards 2023
*/

include <a8pico_dimensions.scad>
use <cube_round.scad>

module back_outside()
{
    cube_round([cart_length, cart_width, cartb_height-rim_height], corner_r,
        fn=corner_fn, round_top=false);

    // rim
    translate([sides_thickness/2+rim_clearance, sides_thickness/2+rim_clearance, 0])
        cube_round([cart_length-sides_thickness-2*rim_clearance, cart_width-
            2*sides_thickness/2 - 2*rim_clearance,
            cartb_height], corner_r-sides_thickness/2,
            fn=corner_fn, round_top=false);

    // ridges
    for (i = [15: 5: cart_length-5])
        translate([i, -ridge_width, corner_r])
            cube_round([3, cart_width + 2*ridge_width,
                cartb_height-rim_height-corner_r],r=ridge_width,fn=corner_fn,round_top=false);

}


module back_inside()
{
    //FIXME  name the "4"
    // upper part, whole lenght
    translate([-1, sides_thickness, front_thickness+4])
        cube_round([cart_length-sides_thickness+1,
            cart_width-sides_thickness*2,
            cartb_height-front_thickness],
            r = corner_r-sides_thickness, fn = corner_fn,
            round_top=false);

    //inside part without the wall on the header end
    translate([sides_thickness, sides_thickness, front_thickness])
        cube_round([cart_length-sides_thickness*2,
            cart_width-sides_thickness*2,
            cartb_height-front_thickness],
            r = corner_r-sides_thickness, fn = corner_fn,
            round_top=false);
    // cutout USB slot
    translate([-1, (cart_width-16)/2+0.5, front_thickness])    // pico is 0.5 mm off center
        cube([40+1, 16, cartb_height]);


}

module screw_hole()
{
    translate([0, 0, -1])
        cylinder(h = supportb_height - 1.2 + 2, r = 1.6,  $fs = 1);
    translate([0, 0, -1])
        cylinder(h = 1.6, r = 3.5, $fs = 1);
    translate([0, 0, 0.6])
        cylinder(h = front_thickness-0.6, r1 = 3.5, r2 = 1.8,  $fs = 1);
}

module pcb_support()
{
        difference() {
            translate([17, sides_thickness, sides_thickness/2])
                cube([support_thickness, cart_width-sides_thickness*2,
cartb_height-sides_thickness/2]);
            translate([17-1, (cart_width-55)/2, supportb_height])
                cube([support_thickness+2, 55, cartb_height-supportb_height+1]);

               // cutout USB slot
            translate([17-1, (cart_width-16)/2+0.5, front_thickness])    // pico is 0.5 mm off center
                 cube([support_thickness+2, 16, cartb_height]);
        }

        translate([60, sides_thickness, sides_thickness])
            cube([support_thickness, 7-sides_thickness,
                    supportb_height-sides_thickness]);
        translate([60, cart_width-7, sides_thickness])
            cube([support_thickness, 7-sides_thickness/2,
supportb_height-sides_thickness]);



}

module back() {

difference() {
    union() {
        difference() {
            // main shell
            back_outside();
                        // make hollow
            back_inside();
       } 
   
        // pcb supports
        pcb_support();

        // screw stud
        translate([31, (cart_width-screw_sep)/2, 0])
                cylinder(h = supportb_height - 1.2, r1 = 4, r2 = 2.8,  $fs = 1);
        translate([31, cart_width-(cart_width-screw_sep)/2, 0])
                cylinder(h = supportb_height - 1.2, r1 = 4, r2 = 2.8,  $fs = 1);
    }


    // screw holes
    translate([31, (cart_width-screw_sep)/2, 0])
        screw_hole();

    translate([31, cart_width-(cart_width-screw_sep)/2, 0])
        screw_hole();

}

}


back();
