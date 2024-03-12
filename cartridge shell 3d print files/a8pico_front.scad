/* A8PicoCart shell
   (c) R.Edwards 2023
*/

include <a8pico_dimensions.scad>
include <cube_round.scad>

module front_outside()
{
    // main shell
    cube_round([cart_length, cart_width, cartf_height],r=corner_r,fn=corner_fn, round_top=false);

    // ridges
    for (i = [15: 5: cart_length-5])
                translate([i, -ridge_width, corner_r])
                    cube_round([3, cart_width + 2*ridge_width,
cartb_height-rim_height-1-corner_r],r=ridge_width,round_top=false);


}

// FIXME move this to dimensions
port_cut_height=10;


module front_inside() {
    // rims
    translate([sides_thickness/2-rim_clearance,cart_width -
        sides_thickness/2+rim_clearance, cartf_height-rim_height +
        rim_height+corner_r])
        rotate([180,0,0])
        cube_round([cart_length-sides_thickness+2*rim_clearance,
            cart_width-sides_thickness+2*rim_clearance,
            rim_height+corner_r],
            r=corner_r-sides_thickness/2,fn=corner_fn,round_top=false);

    // make hollow
    translate([-1, sides_thickness, front_thickness])
        cube_round([cart_length-sides_thickness+1,
            cart_width-sides_thickness*2,
            cartf_height-front_thickness+1],
            r=corner_r-sides_thickness,fn=corner_fn,round_top=false);

    // cutout cart port
    translate([-1, (cart_width-57)/2, -1])
        cube([port_cut_height+1, 57, cartf_height+2]);

    // button
    translate([cart_length-15, 9, -1])
        cylinder(h = front_thickness+2, r1 = 2.5, r2 = 2.5,  $fs = 1); 

}

module front_screw_mount()
{
    difference() {
        cylinder(h = supportf_height + 2.5, r1 = 3.2, r2 = 2.8,  $fs = 1);
        translate([0,0,-1])
            cylinder(h = supportf_height + 2.5+2, r1 = 1.25, r2 = 1.25,  $fs = 1);
    }

}

module front_wo_logo()
{
    difference() {
        front_outside();
        front_inside();
    }    
    // pcb supports
    difference() {
        //FIXME 17 as variable
        translate([17, sides_thickness/2, sides_thickness/2])
            cube([support_thickness, cart_width-sides_thickness,
cartf_height-rim_height-sides_thickness/2]);
        translate([17-1, (cart_width-55)/2, supportf_height])
            cube([support_thickness+2, 55, cartf_height-supportf_height]);
    }

    translate([60, sides_thickness/2, sides_thickness/2])
        cube([support_thickness, 7-sides_thickness/2,
supportf_height-sides_thickness/2]);
    translate([60, cart_width-7, sides_thickness/2])
        cube([support_thickness, 7-sides_thickness/2,
supportf_height-sides_thickness/2]);

    // screw holes
    translate([31, (cart_width-screw_sep)/2, 0])
        front_screw_mount();
    translate([31, cart_width-(cart_width-screw_sep)/2, 0])
        front_screw_mount();
}


module front_logo_hole() {

    difference() {
        front_wo_logo();
        // logo space
        translate([cart_length-logo_height-2, cart_width-logo_width-3.5, -1])
            cube([logo_height, logo_width, 1+1]);
    }
}

logo_image = "a8pico_logo.png";
image_size=[363,88];

module logo()
{
    rect_line_width=0.5;
    logo_depth=0.6;
    scl = (logo_height-rect_line_width)/image_size[0];

    /* logo frame */
    difference(){
        cube([logo_height, logo_width, logo_depth+1]);
        translate([rect_line_width,rect_line_width, -1])
            cube([logo_height-2*rect_line_width, logo_width-2*rect_line_width,
                logo_depth+1+2]);
    }
    /* logo text */
    intersection(){
        cube([logo_height, logo_width, logo_depth+1]);
    //FIXME why the "-1" to shift the logo lower and to the left side??
        translate([logo_height/2+rect_line_width-0.5,logo_width/2+rect_line_width -1 ,100/2])
            scale([scl,scl,1])
            mirror([0,1,0])
            surface(file = logo_image, center = true, invert = true);
    }

}

module reset_label()
{
    font = "Liberation Mono:style=Medium";
    rpi_logo_scale = 0.22;
    rst_height = 1.6;

    mirror([0,1,0])
        linear_extrude(height = rst_height) {
            translate([38,0]) 
                text(text = str("RST"), font = font, size = 30);
            translate([0,-8]) 
                scale([rpi_logo_scale,rpi_logo_scale])
                import("raspberry-logo-raspberry-pi.svg");
        }
}

module front() {

    difference() {
        front_wo_logo();
        // logo rectangle
        translate([cart_length-logo_height-3, cart_width-logo_width-3.5, -1])
        logo();

        translate([cart_length-25, cart_width-46, -1])
        scale([0.15,0.15,1])
        reset_label();
    }
}

front();
