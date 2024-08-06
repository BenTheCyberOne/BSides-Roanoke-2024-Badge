// Case for a pico and PN532
// mjf 6/26/2017
$fn=24;
caseH = 15;
caseW = 70;
caseL = 76;
//PN532W = 40;
//PN532L = 43;
//NanoW = 19;
//NanoL = 43;
pico_w = 24;
pico_l = 53;
NanoW = pico_w;
NanoL = pico_l;
NanoBlock = 28; 
wallThick = 2;
clearance = .5;
arbitrary = -3; // some arbitrary height above the base


//////////////// Start of Letter Module //////////////////////////////
// found on https://github.com/KitWallace/openscad/blob/master/liddedHeartBox.scad
font = "Liberation Sans";

letter_size = 5;
letter_height = 1;


module letter(l) {
	// Use linear_extrude() to make the letters 3D objects as they
	// are only 2D shapes when only using text()
	linear_extrude(height = letter_height) {
		text(l, size = letter_size, font = font, halign = "center", valign = "center", $fn = 16);
	}
}

//////////////// End of Letter Module //////////////////////////////

module post(x, y, bigPost, smallPost, h){
    difference(){
    translate([-caseL/2 + wallThick + x ,-caseW/2 +wallThick + y ,0])
        cylinder(r=bigPost, h=h);
    translate([-caseL/2 + wallThick + x ,-caseW/2 +wallThick + y ,0.1])
        cylinder(r=smallPost, h=h);
    }
    
}

module nano(Xoffset, Yoffset, Zoffset){
    difference () {
        // main block
        translate([Xoffset,Yoffset,0 + Zoffset])
            cube ([NanoBlock,NanoL,8.6], center=true);
        // cut out
        translate([Xoffset,Yoffset,2+ Zoffset])
            cube ([NanoW-2,NanoL+1.5,8], center=true);
        // nano slot
        translate([Xoffset,Yoffset,4+ Zoffset])
            cube ([NanoW-1,NanoL+1,3.6], center=true);      
        // header pin slot
        translate([Xoffset,Yoffset,5.5+ Zoffset])
            cube ([NanoW,NanoL+1,3.6], center=true);
    }     
}

module base(){
    difference(){
        translate([0,0,.5*caseH]) 
            cube([ caseL, caseW,  caseH], center = true);
        translate([0,0,.5*caseH + wallThick]) 
            cube([ caseL-2*wallThick, caseW-2*wallThick,  caseH], center = true);
        translate([caseW/2 -NanoBlock/2 -wallThick ,-caseW/2 -1 ,7.5])
            cube([9,wallThick+2,5]); //USB hole
        
    /////////Letters ///////////////////////////////////
        translate([-22, 0, .9]) rotate([0, 180, 90]) letter("P");
        translate([-22, -5, .9]) rotate([0, 180, 90]) letter("L");
        translate([-22, -10, .9]) rotate([0, 180, 90]) letter("A");
        translate([-22, -15, .9]) rotate([0, 180, 90]) letter("C");
        translate([-22, -20, .9]) rotate([0, 180, 90]) letter("E");
        translate([-15, 0, .9]) rotate([0, 180, 90]) letter("B");
        translate([-15, -5, .9]) rotate([0, 180, 90]) letter("A");
        translate([-15, -10, .9]) rotate([0, 180, 90]) letter("D");
        translate([-15, -15, .9]) rotate([0, 180, 90]) letter("G");
        translate([-15, -20, .9]) rotate([0, 180, 90]) letter("E");
        translate([-8, -3, .9]) rotate([0, 180, 90]) letter("H");
        translate([-8, -8, .9]) rotate([0, 180, 90]) letter("E");
        translate([-8, -13, .9]) rotate([0, 180, 90]) letter("R");
        translate([-8, -18, .9]) rotate([0, 180, 90]) letter("E");
    }
    //post(x = 8, y=8, bigPost = 2.5, smallPost = 1.25);
    //post(x = 36, y=33, bigPost = 2.5, smallPost = 1.25);
    post(x = 8, y=8, bigPost = 4, smallPost = 2.1, h=wallThick +3);
    post(x = 36, y=33, bigPost = 4, smallPost = 2.1, h=wallThick +3);
    post(x = 2.2, y=caseW-6.2, bigPost = 4, smallPost = 2.1, h=caseH-wallThick);
    post(x = caseL-6.2, y=caseW-6.2, bigPost = 4, smallPost = 2.1, h=caseH-wallThick);
}


module top(){
    difference() {
      union() {
          translate([0,0,caseH + arbitrary + wallThick]) 
              cube([ caseL-(2*wallThick +2*clearance), caseW-(2*wallThick + 2*clearance),  wallThick], center = true); //bottom
          // difference(){   
              translate([0,0,caseH + arbitrary  + 2* wallThick])
                  cube([ caseL, caseW, wallThick], center = true); //top
            /*  translate([0, 10, 17.4]) rotate([180, 0, 90]) letter("M");
              translate([0, 5, 16.4]) rotate([0, 00, 270]) letter("J");
              translate([0, 0, 16.4]) rotate([0, 00, 270]) letter("F");
              translate([0, -5, 16.4]) rotate([0, 00, 270]) letter("5");
              translate([0, -10, 16.4]) rotate([0, 00, 270]) letter("5");
           }*/
      }
      translate([(caseL-6.2-wallThick)/2,-(caseW-6.2-wallThick)/2,caseH + arbitrary])
        cylinder(10, r=1.6);
      translate([-(caseL-6.2-wallThick)/2,-(caseW-6.2-wallThick)/2,caseH + arbitrary])
        cylinder(10, r=1.6);
    }
    
}


base();
nano(Xoffset = caseL/2 - wallThick  - NanoBlock/2 +1 , Yoffset= -caseW/2  +NanoL/2 +wallThick , Zoffset = 4.3);

//rotate([0,0,180])
//top();

rotate([0,180,0])
	translate([0,caseL, -(caseH + arbitrary  + 2*wallThick+1)]) //dont know why 1.2 makes it on 0 plane
		top();

