use <bezier.scad>

$fn=60;
e=0.01;

top_len=59;
window_width=54;
window_elevate=1+e;
bottom_thick=3;
tape_width=50.3;
tape_thick=0.25;
extra_width=6;

track_thick=2.5;
acrylic_len=40;
front_frame=2;
illuminator_len=30 + front_frame;

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

module rp2040_simple() {
  translate([-51/2, 0, 0]) cube([51, 23, 1.8]);
  translate([-22, 8.4, 3.28]) rotate([0, -90, 0]) hull() {  // USB extension
    cylinder(r=1.65, h=10);
    translate([0, 6, 0]) cylinder(r=1.65, h=10);
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
  w=19.4;
  h=7.2;
  translate([-w/2, -h/2, -2]) cube([w, h, 3]);
  translate([-(w-1)/2, -(h-1)/2, -track_thick-e]) cube([w-1, h-1, track_thick]);
  translate([w/2-e, -(h-1)/2, -1]) cube([2, h-1, 3]);  // Cable
  fudge_len=tape_width/2-1;
  translate([w/2-e, -2.5/2, -1]) cube([fudge_len, 2.5, 6]);  // Cable
}

//strip_sensor();
module encoder() {
  comp_height=2;  // height of components.
  bottom_offset=bottom_thick - comp_height;

  w=19.4;
  h=7.2;
  translate([-(w-2)/2, -(h-2)/2, -e]) {    // resting
    cube([w-2, h-2, bottom_thick+e+10]);
  }

  // Sensor
  translate([0, 0, bottom_offset]) {
    translate([-w/2, -h/2, 0]) cube([w, h, comp_height+e]);
  }

  // LED light
  w2=19;
  h2=6.8;
  if (true) translate([0, 0, 0]) {
      translate([-w2/2, -h2/2, bottom_thick]) cube([w2, h2, comp_height]);
    }
}

module track(bottom_thick=bottom_thick, wall_height=3) {
  total_height=bottom_thick + wall_height;
  difference() {
    w = tape_width + extra_width;
    translate([-w/2, 0, 0]) cube([w, track_len, total_height]);
    translate([-tape_width/2, -e, bottom_thick]) cube([tape_width, track_len+2*e, total_height]);
  }
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
    translate([-2.5, 3, 6.5]) rotate([-10, 0, 0]) {
      rp2040_board();
      color("#ffff0070") rp2040_simple();
      translate([-10, -1, 3]) battery();
    }
  }
}

if (false) {
  difference() {
    translate([0, -25, 0]) track();
    translate([15, 21, 0]) encoder();
  }
  translate([0, 0, bottom_thick]) pcb_arrangement();
  translate([0, -30, bottom_thick+0.5]) window();
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
	translate([-20-1, 14.5, 0]) cube([7, 8, 4.5]);
	translate([+20-7-3, 14.5, 0]) cube([7, 8, 4.5]);

	// Side holders
	translate([-5, 0, 0]) translate([len/2-6, back-10-2, 0]) cube([6, 10, 4]);
	union() {
	  translate([-len/2, back-12-2, 0]) cube([6, 12, 4]);
	  //translate([-len/2, 13, 0]) cube([1.5, 7, 6]);
	}
      }
      translate([0, -front_frame, 0]) base_case_bottom(len, back, 20);
    }
    translate([0, -front_frame, 0]) pcb_arrangement();
    translate([0, 6.6, -3]) rounded_cube(w=54, h=9, r=2.5, thick=6);
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
    translate([0, 0, tape_thick]) base_case_bottom(height=10, extra=0.3);
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
//base_case_print();
//track();
//pcb_arrangement();

//translate([-tape_width/2, 0, 0]) side_wall();
//track2();
//track_bottom();
//track();
//glowxel_pcb();

//translate([0, 0, -3.6]) encoder();
//strip_led_light();
