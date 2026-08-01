/*
 * YN038 ESP32-S3 开发板卡扣式外壳（PLA，V2）
 *
 * 已实测：PCB 63.00 x 27.92 x 1.61 mm；背面排针突出 8.58 mm；
 * 正面 USB-C 最高 4.88 mm。双 USB-C 位于 y=0 的短边。
 *
 * 导出：将 part 改为 "base" 或 "lid"，F6 渲染后导出 STL。
 */

$fn = 48;

part = "assembly"; // [assembly,base,lid,collision]

pcb_w = 27.92;
pcb_l = 63.00;
pcb_t = 1.61;
module_overhang = 6.81;
module_l = 25.50;
module_w = 18.00;
module_top_h = 4.77;
pin_drop = 8.58;
assembly_top_h = 4.88; // PCB 底面到 USB-C 最高点的实测总高度

clearance_xy = 0.40;
clearance_top = 0.90;
wall = 1.60;
floor_t = 1.60;
usb_interface_h = 2.56;
usb_opening_w = 9.40;
usb_opening_h = 3.80;
usb_center_pitch = 10.47;
usb_top_bridge = 1.80;
g3_led_size = 5.00;
g3_led_clearance = 0.20;
g3_window_t = 0.80;
g3_well_inner = 5.60;
g3_well_wall = 1.00;
g3_well_depth = 2.00;
pin_rail_w = 2.40;
pin_guide_h = 5.50;

// PCB 由支柱托住，背面排针尖端距壳底保留 0.70 mm 安全余量。
board_z = floor_t + pin_drop + 0.70;
usb_center_z = board_z + assembly_top_h - usb_interface_h / 2;
usb_window_z = usb_center_z - usb_opening_h / 2;
rim_h = max(
    board_z + assembly_top_h + clearance_top,
    usb_window_z + usb_opening_h + usb_top_bridge
);

outer_w = pcb_w + 2 * (wall + clearance_xy);
outer_l = pcb_l + module_overhang + 2 * (wall + clearance_xy);

board_x = wall + clearance_xy;
board_y = wall + clearance_xy;

/* 生成闭合的底壳：四侧有壁，仅在前端精确开出两个 USB-C 窗口。 */
module base_shell() {
    difference() {
        union() {
            cube([outer_w, outer_l, floor_t]);
            translate([0, outer_l - wall, 0]) cube([outer_w, wall, rim_h]);
            cube([outer_w, wall, rim_h]);
            cube([wall, outer_l, rim_h]);
            translate([outer_w - wall, 0, 0]) cube([wall, outer_l, rim_h]);
            board_standoffs();
            pin_guide_rails();
        }
        pin_pockets();
        snap_receiver_slots();
        usb_windows();
    }
}

/* 两条低矮导轨包围针脚下段，顶部避开排针塑胶座并形成真实盲槽实体。 */
module pin_guide_rails() {
    first_pin_y = board_y + 4.83;
    last_pin_y = first_pin_y + 21 * 2.54;
    rail_y = first_pin_y - pin_rail_w / 2;
    rail_l = last_pin_y - first_pin_y + pin_rail_w;
    for (x = [board_x + 1.26, board_x + pcb_w - 1.26])
        translate([x - pin_rail_w / 2, rail_y, floor_t - 0.01])
            cube([pin_rail_w, rail_l, pin_guide_h + 0.01]);
}

/* 在四角托住 PCB，避免压到背面的排针焊脚和焊点。 */
module board_standoffs() {
    for (x = [board_x + 5.0, board_x + pcb_w - 6.6])
        for (y = [board_y + 3.0, board_y + pcb_l - 4.6])
            translate([x, y, floor_t - 0.01])
                cube([2.0, 2.0, pin_drop + 0.71]);
}

/* 从实体导轨中切出两排 22 个盲槽；壳底仍保留完整的 1.59 mm 封闭层。 */
module pin_pockets() {
    pin_row_x = [board_x + 1.26, board_x + pcb_w - 1.26];
    for (x = pin_row_x)
        for (i = [0 : 21])
            translate([x - 0.85, board_y + 4.83 + i * 2.54 - 0.85, floor_t - 0.01])
                cube([1.70, 1.70, board_z - floor_t + 0.02]);
}

/* 用整板、模组凸出段和 44 根方针构造参考体，避免漏查天线端与顶面干涉。 */
module board_reference() {
    union() {
        translate([board_x, board_y, board_z])
            cube([pcb_w, pcb_l, assembly_top_h]);
        translate([
            board_x + (pcb_w - module_w) / 2,
            board_y + pcb_l - (module_l - module_overhang),
            board_z
        ]) cube([module_w, module_l, module_top_h]);
        for (x = [board_x + 1.26, board_x + pcb_w - 1.26])
            for (i = [0 : 21])
                translate([x - 0.32, board_y + 4.83 + i * 2.54 - 0.32, board_z - pin_drop])
                    cube([0.64, 0.64, pin_drop]);
    }
}

/* 前端仅保留两个 USB-C 插口；按插头外轮廓使用胶囊形圆角长孔。 */
module usb_windows() {
    for (center_x = [
        outer_w / 2 - usb_center_pitch / 2,
        outer_w / 2 + usb_center_pitch / 2
    ])
        rounded_usb_window(
            center_x - usb_opening_w / 2,
            usb_window_z,
            usb_opening_w,
            usb_opening_h
        );
}

/* 用两个等半径圆柱的凸包生成标准胶囊孔，截面为直边加左右完整半圆。 */
module rounded_usb_window(x, z, width, height) {
    radius = height / 2;
    hull() {
        translate([x + radius, -0.01, z + radius])
            rotate([-90, 0, 0]) cylinder(h = board_y + 0.02, r = radius);
        translate([x + width - radius, -0.01, z + radius])
            rotate([-90, 0, 0]) cylinder(h = board_y + 0.02, r = radius);
    }
}

/* 底壳侧壁的卡扣接收孔，和顶盖的弹性卡舌配合。 */
module snap_receiver_slots() {
    for (y = [8, outer_l - 14]) {
        translate([-0.1, y, rim_h - 4.9]) cube([wall + 0.3, 4.0, 2.0]);
        translate([outer_w - wall - 0.2, y, rim_h - 4.9])
            cube([wall + 0.3, 4.0, 2.0]);
    }
}

/* 生成封闭式半透明顶盖，G3 使用薄窗和内置光井显示状态。 */
module lid() {
    difference() {
        union() {
            translate([-0.30, 0, rim_h]) cube([outer_w + 0.60, outer_l, wall]);
            lid_skirt_and_latches();
            lid_internal_optics();
        }
        g3_window_recess();
    }
}

/* 顶盖四个长弹片扣入底壳孔位；PLA 使用宽弹片降低折断风险。 */
module lid_skirt_and_latches() {
    for (y = [8, outer_l - 14]) {
        // 局部根座覆盖整片卡舌宽度，卡舌再贯穿整个顶板厚度，避免薄边点接触。
        translate([-1.20, y - 0.60, rim_h]) cube([1.20, 5.20, wall]);
        translate([outer_w, y - 0.60, rim_h]) cube([1.20, 5.20, wall]);
        // 弹片位于底壳外侧，避免占用 PCB 两侧仅 0.40 mm 的装配间隙。
        translate([-1.15, y, rim_h - 4.8]) cube([1.10, 4.0, 4.8 + wall]);
        translate([outer_w + 0.05, y, rim_h - 4.8]) cube([1.10, 4.0, 4.8 + wall]);
        // 0.45 mm 楔形钩带导入斜坡，压盖时逐渐外张，降低 PLA 折断风险。
        hull() {
            translate([-0.10, y + 0.45, rim_h - 4.8]) cube([0.10, 3.1, 0.10]);
            translate([-0.10, y + 0.45, rim_h - 3.4]) cube([0.55, 3.1, 0.10]);
        }
        hull() {
            translate([outer_w, y + 0.45, rim_h - 4.8]) cube([0.10, 3.1, 0.10]);
            translate([outer_w - 0.45, y + 0.45, rim_h - 3.4])
                cube([0.55, 3.1, 0.10]);
        }
    }
}

/* 从内侧减薄 G3 区域，外表面保留 0.80 mm 封闭磨砂透光窗。 */
module g3_window_recess() {
    g3_center_x = board_x + 8.10;
    // 实测：天线侧主 PCB 边到灯珠左沿 29.83 mm；换算到 USB 端中心为 30.67 mm。
    g3_center_y = board_y + 30.67;
    g3_hole = g3_led_size + g3_led_clearance;

    translate([
        g3_center_x - g3_hole / 2,
        g3_center_y - g3_hole / 2,
        rim_h - 0.1
    ]) cube([g3_hole, g3_hole, wall - g3_window_t + 0.1]);
}

/* 光井减少 G3 视差；加厚顶和围墙抑制 PWR/TX/RX 红光在半透明顶盖内扩散。 */
module lid_internal_optics() {
    g3_center_x = board_x + 8.10;
    g3_center_y = board_y + 30.67;
    g3_well_outer = g3_well_inner + 2 * g3_well_wall;
    pwr_x = board_x + 4.40;
    pwr_y = board_y + 10.80;
    pwr_w = 19.20;
    pwr_l = 5.70;
    pwr_roof_extra = 1.50;
    pwr_wall_depth = 2.20;
    pwr_wall_t = 0.80;

    difference() {
        translate([
            g3_center_x - g3_well_outer / 2,
            g3_center_y - g3_well_outer / 2,
            rim_h - g3_well_depth
        ]) cube([g3_well_outer, g3_well_outer, g3_well_depth + 0.01]);
        translate([
            g3_center_x - g3_well_inner / 2,
            g3_center_y - g3_well_inner / 2,
            rim_h - g3_well_depth - 0.01
        ]) cube([g3_well_inner, g3_well_inner, g3_well_depth + 0.03]);
    }

    translate([pwr_x, pwr_y, rim_h - pwr_roof_extra])
        cube([pwr_w, pwr_l, pwr_roof_extra + 0.01]);
    translate([pwr_x, pwr_y, rim_h - pwr_wall_depth])
        cube([pwr_w, pwr_wall_t, pwr_wall_depth + 0.01]);
    translate([pwr_x, pwr_y + pwr_l - pwr_wall_t, rim_h - pwr_wall_depth])
        cube([pwr_w, pwr_wall_t, pwr_wall_depth + 0.01]);
    translate([pwr_x, pwr_y, rim_h - pwr_wall_depth])
        cube([pwr_wall_t, pwr_l, pwr_wall_depth + 0.01]);
    translate([pwr_x + pwr_w - pwr_wall_t, pwr_y, rim_h - pwr_wall_depth])
        cube([pwr_wall_t, pwr_l, pwr_wall_depth + 0.01]);
}

/* 以不同颜色展示合盖后的相对位置，供 OpenSCAD 预览和尺寸检查。 */
module show_assembly() {
    color("dimgray") base_shell();
    color("gainsboro") lid();
}

/* 同时检查板卡、底壳与顶盖的实体重叠；正确设计应导出为空几何。 */
module collision_check() {
    union() {
        intersection() {
            base_shell();
            translate([0, 0, 0.05]) lid();
        }
        intersection() {
            base_shell();
            translate([0, 0, 0.05]) board_reference();
        }
        intersection() {
            translate([0, 0, 0.05]) lid();
            board_reference();
        }
    }
}

if (part == "base")
    base_shell();
else if (part == "lid")
    // 顶盖导出时翻转，外表面贴打印平台，卡扣朝上且无需支撑。
    translate([0, outer_l, rim_h + wall]) rotate([180, 0, 0]) lid();
else if (part == "collision")
    collision_check();
else
    show_assembly();
