$fn = 100;
difference() {
    union() {
        cylinder(r=29.5/2, h=20);
        cylinder(r=32/2, h=2);
    }
    translate([0,0,-1]) cylinder(r=1.5, h=30);
    translate([-4.25,-10,2]) cube([9,13,30]);
    translate([-12.5,-8,2]) cube([24,3,30]);

}
