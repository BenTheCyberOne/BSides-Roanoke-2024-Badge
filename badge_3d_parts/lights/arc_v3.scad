/*
 * Draws an arc suitable for a badge.
 *
 *         \
 *  _    \  \
 * | | | |  |
 *  -    /  /
 *         /
 *  ^
 *  |
 *  Y  X->
 *
 * All units in mm.
 *
 * 2024/1/17
 * Aaron
 */


// Distance from center of badge to center of arc
r_dists = [44, 72, 100];

// Degrees of coverage
arclen=60;

// How high off the PCB?
height=8;

//Width of arc
width=6.3+2*.8;

// Wall thickness
z_wall_thickness=.0;
//z_wall_thickness=.6;
xy_wall_thickness=.8;

// render quality (16 is low, 64 is good)
fn=128;

// used to calculate the bottom portion of the knob clip
pcb_width=1.6;

// knob catch height/depth
knob_depth=.4;
knob_height=.4;
knob_width=.4;

invert=0;

// CODE 
include <BOSL/constants.scad>
use <BOSL/shapes.scad>

module lb(r_dist) {
  union() {
    difference() {
      arced_slot(d=r_dist, h=height, sd=width, sa=0, ea=arclen, $fn=fn, $fn2=fn);
      translate([0,0,z_wall_thickness]) {
        arced_slot(d=r_dist, h=height, sd=width-2*xy_wall_thickness, sa=0, ea=arclen, $fn=fn, $fn2=fn);
      }
    }
    
    
    /*for(rot=[0, arclen]) {
      rotate([0,0,rot]) {      
        // first knob
        translate([r_dist/2, 0, height*.25]) {
          color("grey")
          arced_slot(d=width-2.9*xy_wall_thickness, 
                     h=3*(pcb_width+knob_height*2+knob_depth), 
                     sd2=knob_width,
                     sd1=0,
                     sa=185+(180/arclen)*rot, 
                     ea=265+(180/arclen)*rot, 
                     $fn=fn, $fn2=fn);
                     
          //color("green")
        }
      }
    }*/
  }
}

// full render

translate([0,0,-10])
projection(cut=false) 

for(i=r_dists) {
  
  lb(i);
}

// individual render
//lb(r_dists[0]);


