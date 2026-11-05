// ============================================================
// Mitat
// ============================================================
plate_w = 150;
plate_h = 90;
plate_t = 2;

box_d  = 40;   // sisätilan syvyys
wall_t =  2;   // seinämien paksuus

// ============================================================
// Napit
// ============================================================
button_d     = 25;
button_r     = button_d / 2;
button_gap   = 12;
button_count =  4;
button_y     = 8 + button_r;

buttons_total_w = button_count * button_d + (button_count - 1) * button_gap;
button_margin   = (plate_w - buttons_total_w) / 2;
button_start_x  = button_margin + button_r;

// ============================================================
// Näyttöaukko
// ============================================================
disp_w  = 44;
disp_h  = 25;
disp_x  = (plate_w - disp_w) / 2;
button_top = button_y + button_r;
disp_y  = button_top + (plate_h - button_top) / 2 - disp_h / 2;
chamfer = 1.5;

// ============================================================
// USB-C takalevyssä
// ============================================================
usb_w = 10;
usb_h =  4;
usb_x = (plate_w - usb_w) / 2;
usb_y = (plate_h - usb_h) / 2;

// ============================================================
// Akkukehys  LP704452  52 × 44 × 7 mm
// Akku tipahtaa alas avoimen etuaukon (z=0) kautta.
// Etuseinä pitää akun Z-suunnassa paikallaan.
// ============================================================
bat_l   = 52;
bat_w   = 44;
bat_h   =  7;
bat_tol =  0.5;   // ilmarako per sivu
clip_t  =  2;     // kehyksen seinämäpaksuus

// Johtojen ulosvientiaukko yhdessä sivussa
wire_slot_w =  8;  // johtoaukon leveys

// Akun sijainti kotelo-koordinaateissa (keskitetty sisätilaan)
bat_x = wall_t + ((plate_w - 2*wall_t) - bat_l) / 2;
bat_y = wall_t + ((plate_h - 2*wall_t) - bat_w) / 2;

// Suorakaiteinen kehys ilman huulia.
// Ulkomitat: (bat_l + 2*bat_tol + 2*clip_t) × (bat_w + 2*bat_tol + 2*clip_t)
// Sisämitat: (bat_l + 2*bat_tol) × (bat_w + 2*bat_tol)   → 0.5 mm ilmarako per sivu
// Korkeus = bat_h  (seinä tasainen akun etupinnan kanssa, etuseinä sulkee)
module battery_tray() {
    h  = bat_h;
    tl = bat_tol;
    z0 = box_d - h;

    translate([bat_x - tl - clip_t, bat_y - tl - clip_t, z0])
    difference() {
        cube([bat_l + 2*(tl + clip_t), bat_w + 2*(tl + clip_t), h]);
        // Akun tasku
        translate([clip_t, clip_t, -0.1])
            cube([bat_l + 2*tl, bat_w + 2*tl, h + 0.2]);
        // Johtoaukko yhdessä sivussa (bat_y-puoli, alareunan seinä)
        translate([(bat_l + 2*tl - wire_slot_w) / 2 + clip_t, -0.1, 0])
            cube([wire_slot_w, clip_t + 0.2, h]);
    }
}

// ============================================================
// Moduulit
// ============================================================

module chamfered_hole(w, h, t, c) {
    hull() {
        translate([0, 0, 0]) cube([w, h, 0.001]);
        translate([c, c, t]) cube([w - 2*c, h - 2*c, 0.001]);
    }
}

module front_panel() {
    difference() {
        cube([plate_w, plate_h, plate_t]);

        // Napit
        for (i = [0 : button_count - 1])
            translate([button_start_x + i * (button_d + button_gap), button_y, -1])
                cylinder(h = plate_t + 2, r = button_r, $fn = 64);

        // Näyttöaukko viisteineen
        translate([disp_x, disp_y, 0])
            chamfered_hole(disp_w, disp_h, plate_t, chamfer);
    }
}

module box_body() {
    union() {
        difference() {
            // Ulkokuori
            cube([plate_w, plate_h, box_d + wall_t]);
            // Sisäontelo – avoin edestä
            translate([wall_t, wall_t, 0])
                cube([plate_w - 2*wall_t, plate_h - 2*wall_t, box_d]);
            // USB-C aukko takaseinässä
            translate([usb_x, usb_y, box_d + wall_t - 0.1])
                cube([usb_w, usb_h, wall_t + 0.2]);
        }
        // Akkukehys sisäpuolella
        battery_tray();
    }
}

// ============================================================
// Renderöinti – kappaleet vierekkäin tulostusasentoa varten
// ============================================================
front_panel();

translate([plate_w + 15, 0, 0])
    box_body();
