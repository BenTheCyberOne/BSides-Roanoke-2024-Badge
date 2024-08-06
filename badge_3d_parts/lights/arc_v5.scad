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
width=6.3+3.2; // no idea how this got here.  But it seems to work?

// Wall thickness
z_wall_thickness=1.2;
//z_wall_thickness=.6;
xy_wall_thickness=1.2;

// render quality (16 is low, 64 is good)
fn=128;

// used to calculate the bottom portion of the knob clip
pcb_width=1.6;

// knob catch height/depth
knob_height=1.6;
knob_width=5.9;

back_height=2;

m25_cs_r2=4.5/2;
m25_cs_r1=2.6/2;



invert=0;

// CODE 
include <BOSL/constants.scad>
use <BOSL/shapes.scad>


module m25_cw(h) {
  delta=(m25_cs_r2-m25_cs_r1);
  threads_h=h-delta;
  union() {
    cylinder(threads_h, r=m25_cs_r1, $fn=fn);
    translate([0,0,threads_h-.001])
      cylinder(delta, r1=m25_cs_r1, r2=m25_cs_r2, $fn=fn);
  }
}

module lb(r_dist, width=width) {
  union() {
    difference() {
      arced_slot(d=r_dist, h=height, sd=width, sa=0, ea=arclen, $fn=fn, $fn2=fn);
      translate([0,0,z_wall_thickness]) {
        arced_slot(d=r_dist, h=height, sd=width-2*xy_wall_thickness, sa=0, ea=arclen, $fn=fn, $fn2=fn);
      }
    };
    
    // Restore ends for clip
    for (rot_angle = [0, arclen]) {
        rotate([0, 0, rot_angle]) {
          translate([r_dist/2, 0, 0]) {
            cylinder(h=height, r=width/2-xy_wall_thickness, center=true, $fn=fn);
          }
          translate([r_dist/2, 0, height/2-.01]) {
            cylinder(h=(pcb_width+knob_height), r=knob_width/2, $fn=fn);
          }
        }
    }
    
    
    
    
    /*
    for(rot=[0, arclen]) {
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
module light_bar_full() {

  for(i=r_dists) {
    
    lb(i, width=knob_width);
  }
}

module backing() {
  difference() {
    union() {
      // arc_axials
      linear_extrude(back_height) {
        intersection() {  
          translate([0,0,0])
            projection(cut=false)
              light_bar_full();
              
          projection(cut=false)
            pie_slice(arclen, h=1, r=r_dists[2]/2+10);
        }
      }
      // connecting arm
      rotate([0,0,arclen/2])
        translate([r_dists[0]/2, -back_height/2, 0])
          cube([(r_dists[2]-r_dists[0])/2, back_height, back_height]);
      
    }
    // right_triangle([width, thickness, height], [orient], [align|center]);
    translate([0,0,back_height+1])
      rotate([0,90,-30])
        translate([0,0,-.1])
          right_triangle([back_height+1, r_dists[2]/2+10, back_height+1]);
    translate([0,0,back_height+1])
      rotate([0,180,-90])
        translate([-.1,0,0])
          right_triangle([back_height+1, r_dists[2]/2+10, back_height+1]);
    
    // screw hole
    translate([r_dists[2]/2, 0, -.1])
    m25_cw(back_height+.1);

  }
}


//*** back brace
backing();



//*** light bars
//light_bar_full();

//****individual render
//lb(r_dists[0]);


