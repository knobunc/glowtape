// Note, this still needs cleanup.

use <bezier.scad>

$fn=60;
e=0.01;

top_len=59;
window_width=53.9;
window_elevate=1+e;

tape_width=50.3;
tape_thick=0.25;
extra_width=6;

track_thick=3;
acrylic_len=40;
front_frame=2;
illuminator_len=28 + front_frame;

track_len=illuminator_len + acrylic_len + 3;

module rounded_cube(w=20, h=10, thick=1, r=0.5) {
  r=(r <= 0) ? e : r;
  dx=w/2-r;
  dy=h/2-r;
  hull() {
    translate([+dx, -dy, 0]) cylinder(r=r, h=thick);
    translate([+dx, +dy, 0]) cylinder(r=r, h=thick);
    translate([-dx, -dy, 0]) cylinder(r=r, h=thick);
    translate([-dx, +dy, 0]) cylinder(r=r, h=thick);
  }
}

module pocket_cube(c, r=0.3) {
  cube(c);
  cylinder(r=r, h=c[2]);
  translate([c[0], 0, 0]) cylinder(r=r, h=c[2]);
  translate([c[0], c[1], 0]) cylinder(r=r, h=c[2]);
  translate([0, c[1], 0]) cylinder(r=r, h=c[2]);
}

module usb() {
  rotate([0, -90, 0]) hull() {  // USB extension
    cylinder(r=1.65, h=10);
    translate([0, 6, 0]) cylinder(r=1.65, h=10);
  }
}

module rp2040_simple() {
  translate([-50.8/2, 0, 0]) cube([50.8, 23, 4.8]);
  hull() {
    translate([-22, 8.4, 3.1]) usb();  // actual location
    translate([-22, 8.4, 5]) usb();    // punch-out help
  }
}

module rp2040_board(extra=0) {
  render() color("#ff000070") translate([-50.8/2, 0, 0]) {
    cube([50.8, 23, 1.6]);  // board thicken
    if (false) translate([2, 8.4, 3.28]) rotate([0, -90, 0]) hull() {  // USB extension
      cylinder(r=1.65, h=10);
      translate([0, 6, 0]) cylinder(r=1.65, h=10);
    }
    difference() {
      import("Feather-rp2040.stl");
      //translate([6.5, 14, 3]) cube([9, 10, 7]);  // battery connector.
    }
  }
}

module glowxel_pcb() {
  color("#0000ffff") {
    translate([-75.6, 39+14.3, 0]) import("glowxels-small.stl");
    translate([0, 14.4/2-0.2, 0]) color("red") rounded_cube(55.6, 14.4, 3);
  }
}

module encoder_board(w, h) {
  color("purple") {
    translate([-w/2, -h/2, 0]) cube([w, h, 3]);
    translate([-6.3, 0, -1.8]) cylinder(r=2.5, h=2);  // viewport
    translate([w/2-3, -h/2, -2]) cube([3, h, 3]);  // Glue joint
    translate([w/2, -(h-1)/2, 0]) cube([2, h-1, 3]);  // Cable
  }
}

module sensor_locator() {
  translate([-3, 20, 0]) children();
}

module strip_led_light() {
  encoder_board(19.1, 6.8); // IR LED illumination
}

module strip_sensor() {
  w=19.7;
  h=7.4;
  translate([-w/2, -h/2, -2.7]) pocket_cube([w, h, 3]);  // hollow
  // bottom retainer
  translate([-(w-1)/2, -(h-1)/2, -track_thick-e]) cube([w-1, h-1, track_thick]);
  translate([w/2-e, -(h-1)/2, -1.5]) cube([2, h-1, 3]);  // Cable extract
  fudge_len=tape_width/2-1;
  translate([w/2-e, -2.5/2, -1]) cube([fudge_len, 2.5, 6]);  // Cable channel
}

module battery() {
  color("silver") cube([32.5, 25.5, 5.7]);
  translate([0.5, 4, 0]) linear_extrude(height=5.7+e) text("LiPo 3.7V", size=3);
}

module window(punch=0) {
  len=acrylic_len + punch;
  color("#00ff0040") translate([-window_width/2, -len, 0]) {
    cube([window_width, len, 3+punch]);
  }
}

module pcb_arrangement() {
  translate([0, front_frame, 0]) {
    translate([0.3, 0, 0.25]) glowxel_pcb();
    sensor_locator() translate([0, 0, +0.5]) strip_led_light();
    //sensor_locator() translate([0, 0, -tape_thick]) strip_sensor();
    translate([-2.5, 3, 4.5]) {
      rp2040_board();
      color("#ffff0070") rp2040_simple();
      translate([-10, -1, 3]) battery();
    }
  }
}

module track_bottom(l=0, r=track_len, b=-track_thick) {
  turn=b+1;
  cubic_spline(points=[  [l+0, b],   // start edge. +0=sharp edge
			 [l, b+1], [l+5, 0],

			 [r-5, 0], [r, b+1],
			 [r-0, b]
		      ],

	       cps = [   [l, b],
			 [l, turn+0.5], [l+5+2, 0],

			 [r-5+2, 0], [r, turn-0.5],
			 [r, b]
		     ],
	       extrude=tape_width,
	       show_controlpoints=false,
	       just_show_polygon=false);
}

module base_case_bottom(len=top_len, back=illuminator_len, height=2.5, extra=0) {
  translate([0, back/2, 0]) rounded_cube(len+extra, back+extra, height, r=1);
}

module base_case(len=top_len) {
  back=illuminator_len;
  render() translate([0, front_frame, 0]) difference() {
    intersection() {
      union() {
	translate([0, -front_frame, 0]) base_case_bottom(len, back);

	// Center rests
	translate([-20-1, 14.5, 0]) cube([7, 4, 5]);
	translate([+20-7-3, 14.5, 0]) cube([7, 4, 5]);

	// Side holders
	translate([-5, 0, 0]) translate([len/2-5, back-8-2, 0]) cube([5, 8, 8]);
	translate([-len/2, back-13.5-2, 0]) cube([6, 13.5, 8]);
	translate([-len/2, -front_frame, 0]) cube([1.8, illuminator_len, 8]);
	//translate([-len/2, illuminator_len-front_frame-1.5, 0]) cube([len, 1.5, 8]);
      }
      translate([0, -front_frame, 0]) base_case_bottom(len, back, 20);
    }
    translate([0, -front_frame, 0]) pcb_arrangement();
    translate([0, 6.6, -3]) {
      rounded_cube(w=54, h=9, r=2.5, thick=6);  // UV-window
    }
    translate([0, -1, 2.6]) sensor_locator() cube([60, 2, 10]);
  }
}

module ear() {
  cylinder(r=2, h=0.2);
}

module base_case_print() {
  base_case();
}

module side_wall(wall_height=4) {
  r=1.5;
  h=wall_height + track_thick;
  // The wider part for the illuminator.
  extra_wide=(top_len - tape_width)/2;

  hull() {
    translate([0, 0, -track_thick]) cube([e, track_len, h]);

    translate([r+1, r, -track_thick]) cylinder(r=r, h=h);
    translate([r, track_len-r, -track_thick]) cylinder(r=r, h=h);

    translate([r+extra_wide, acrylic_len, -track_thick]) cylinder(r=r, h=h);
    translate([r+extra_wide, track_len-r, -track_thick]) cylinder(r=r, h=h);
  }
}

module track_frame() {
  translate([-tape_width/2, -acrylic_len, 0]) {
    rotate([0, 0, 90]) rotate([90, 0, 0])  track_bottom();
  }
  translate([tape_width/2, -acrylic_len, 0]) side_wall();
  translate([-tape_width/2, -acrylic_len, 0]) scale([-1, 1, 1]) side_wall();
}

module track() {
  difference() {
    track_frame();
    translate([0, 0, tape_thick]) base_case_bottom(height=10, extra=0.4);
    translate([0, 0, window_elevate]) window(punch=1);
    sensor_locator() translate([0, front_frame, 0]) strip_sensor();
  }
}

module assembly() {
  color("yellow") track();
  translate([0, 0, tape_thick]) {
    base_case_print();
    pcb_arrangement();
  }
  translate([0, 0, window_elevate]) window();
}

assembly();
