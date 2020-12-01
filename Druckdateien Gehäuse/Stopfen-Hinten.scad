$fn = 100;
difference() {
    union() {
        cylinder(r=29.5/2, h=20);
        cylinder(r=32/2, h=2);
    }
    translate([0,0,-1]) cylinder(r=2, h=30);
    translate([0,-20,-1]) cube([40,40,40]);
}
