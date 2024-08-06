/*
 * PN532 Support
 * 
 */
 
 include <BOSL/shapes.scad>;
  include <BOSL/constants.scad>;
 
// sourced from https://www.elechouse.com/elechouse/images/product/PN532_module_V3/PN532_shematic_drowing.pdf
x = 42.7;
y = 40.4;

z = 10;
 
  
hole_size = 3;
hole_centers = [
 [x-7.5, 7.6],
 [7.5, y-7.6]];

header_y_len = 2.8;
header_x_len = 2.54*(8+1)+.3;

header_centerx = x - (15.1-2.54/2 + header_x_len/2);
header_centery = 6.52;

union() {
  difference() {
    cuboid(
      [x,y,z], fillet=5,
      edges=EDGE_FR_RT+EDGE_BK_RT+EDGE_BK_LF+EDGE_FR_LF, 
      $fn=256
      );
      
    translate([header_centerx-x/2, header_centery-y/2, 0]) {
      cube([header_x_len, header_y_len, z+10], center=true);
    }
  }
  
  for (hc = hole_centers) {
    translate([hc[0]-x/2, hc[1]-y/2, z/2]) {
      sphere(hole_size/2, $fn=128);
    }
  }
}


