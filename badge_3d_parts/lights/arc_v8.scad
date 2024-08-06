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

// downsizing backing until it works
backing_fudge = .15;

//m2.5 counter sunk head sizes
m25_cs_r2=(4.5+backing_fudge)/2;
m25_cs_r1=(2.6+backing_fudge)/2;


//m2.5 heat set insert
m25_hsi_r=(3.2+backing_fudge)/2;
m25_hsi_h=4.5+backing_fudge;



invert=0;

// CODE 
include <BOSL/constants.scad>
use <BOSL/shapes.scad>


module m25_cs(h) {
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
    difference() {
      for (rot_angle = [0, arclen]) {
          rotate([0, 0, rot_angle]) {
            translate([r_dist/2, 0, 0]) {
              cylinder(h=height, r=width/2-xy_wall_thickness, center=true, $fn=fn);
            }
            difference() {
              translate([r_dist/2, 0, height/2-.01]) {
                cylinder(h=(pcb_width+back_height), r=knob_width/2, $fn=fn);
              }
              
              // brackets
              if(rot_angle == 0) {
                translate([r_dist/2-knob_width/2, knob_width-.2, height/2+pcb_width-.2])
                  rotate([0,0,-90])
                    right_triangle([knob_width, knob_width,knob_width]);
              } else {
                translate([r_dist/2-knob_width/2, -knob_width+.2, height/2+pcb_width-.2])
                rotate([0,-90,-90])
                  right_triangle([knob_width, knob_width,knob_width]);
              }
              

            }
          }
        }
        // lop off top
        translate([r_dists[0]/2-knob_width/2, 0, height/2+knob_height-.2])
          cylinder(10,10);
        // bore hole for heat set insert
        translate([r_dists[0]/2, 0, height/2+pcb_width-.1-m25_hsi_h])
          cylinder(m25_hsi_h+2, r=m25_hsi_r, $fn=fn);
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
module light_bar_full(width=width) {

  union() {
    for(i=r_dists) {
      lb(i, width=width);
    }
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
              light_bar_full(knob_width);
              
          projection(cut=false)
            union() {
              pie_slice(arclen, h=1, r=r_dists[2]/2+10);
              translate([20,0,0])
                  cylinder(10, 5, center=true);
            }
        }
      }

      // connecting arm
      rotate([0,0,arclen/2])
        translate([r_dists[0]/2, -back_height, 0])
          cube([(r_dists[2]-r_dists[0])/2, back_height*2, back_height]);
      
    }
    color([1,0,0])
    // right_triangle([width, thickness, height], [orient], [align|center]);
    translate([0,0,back_height+1])
      rotate([0,90,-30])
        translate([.5,0,-.6])
          right_triangle([back_height+2, r_dists[2]/2+10, back_height+2]);
          
    color([1,0,0])
    translate([0,0,back_height+1])
      rotate([0,180,-90])
        translate([-.6,30,0])
          right_triangle([back_height+2, r_dists[2]/2+10, back_height+2]);
    
    // screw hole
    color([1,0,0])
    translate([r_dists[0]/2, 0, -.1])
      m25_cs(back_height+.11);

  }
}


//*** back brace
backing();



//*** light bars
//light_bar_full();

//****individual render
//lb(r_dists[0]);


