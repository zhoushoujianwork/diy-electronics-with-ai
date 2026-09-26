"""Complete, removable snap enclosure for a Tiny board -- Rev B fit assumptions.

Units: mm. Base origin: footprint centre, bottom face. Lid origin: seat plane.
The header location, PCB thickness and corner support regions are NOT measured.
See tiny-case-design.md before printing or fitting powered electronics.
"""

from build123d import (
    Align, Box, Color, Compound, Cylinder, Location, Plane, Polygon, Pos,
    RectangleRounded, RigidJoint, Rot, extrude,
)

OUTER_X = 24.0
OUTER_Y = 24.0
OUTER_RADIUS = 1.2
INNER_X = 20.6
INNER_Y = 20.6
FLOOR = 1.4
BASE_HEIGHT = 10.2
LID_THICKNESS = 1.6

PCB_X = PCB_Y = 20.0
PCB_THICKNESS_ASSUMED = 1.0
PCB_BOTTOM = 2.4
PCB_TOP = PCB_BOTTOM + PCB_THICKNESS_ASSUMED
SUPPORT_CENTRE = 9.4
SUPPORT_SIZE = 1.2
BOARD_VERTICAL_PLAY = 0.4

GUIDE_OUTER = 20.2
GUIDE_WALL = 0.7
GUIDE_DEPTH = 1.4
CLIP_WIDTH = 4.8
CLIP_THICKNESS = 0.8
CLIP_OUTER_Y = 11.0
CLIP_LENGTH = 6.0
HOOK_PROJECTION = 0.45
HOOK_RAMP_HEIGHT = 1.2
POCKET_OUTER_Y = 11.2
POCKET_WIDTH = CLIP_WIDTH + 0.6
POCKET_BOTTOM = 4.0
LATCH_WINDOW_BOTTOM = 4.0
LATCH_WINDOW_TOP = 5.7

HEADER_X_ASSUMED = 8.8
HEADER_PITCH = 2.54
HEADER_COUNT = 6
HEADER_SLOT_X = 3.6
HEADER_SLOT_Y = 16.4
SIDE_PORT_BOTTOM = 1.9
SIDE_PORT_HEIGHT = 4.2
ANTENNA_PORT_DIAMETER = 3.0
ANTENNA_PORT_Y = 0.0
ANTENNA_PORT_Z = 8.0

BASE_COLOR = Color(0.19, 0.22, 0.27)
LID_COLOR = Color(0.87, 0.89, 0.91)


def box_at(width, depth, height, x=0, y=0, z=0):
    return Pos(x, y, z) * Box(
        width, depth, height, align=(Align.CENTER, Align.CENTER, Align.MIN)
    )


def rounded_plate(width, depth, radius, height, z=0):
    return Pos(0, 0, z) * extrude(RectangleRounded(width, depth, radius), amount=height)


def antenna_bore(z, diameter=ANTENNA_PORT_DIAMETER):
    # A complete round hole in the lower shell. Verify RF plug clearance before threading.
    # This provides a cable exit only, not strain relief or a sealed bulkhead connector.
    return Pos(9.0, ANTENNA_PORT_Y, z) * Rot(0, 90, 0) * Cylinder(
        diameter / 2, 4.0, align=(Align.CENTER, Align.CENTER, Align.MIN)
    )


def make_base(header_exit="bottom"):
    if header_exit not in ("bottom", "side"):
        raise ValueError("header_exit must be bottom or side")
    base = rounded_plate(OUTER_X, OUTER_Y, OUTER_RADIUS, BASE_HEIGHT)
    base -= box_at(INNER_X, INNER_Y, BASE_HEIGHT + 1, z=FLOOR)

    # Only four PCB corner regions are contacted; actual component keepouts need checking.
    for x_sign in (-1, 1):
        for y_sign in (-1, 1):
            base += box_at(
                SUPPORT_SIZE, SUPPORT_SIZE, PCB_BOTTOM - FLOOR + 0.1,
                x=x_sign * SUPPORT_CENTRE, y=y_sign * SUPPORT_CENTRE, z=FLOOR - 0.1,
            )

    # Recesses admit the lid arms. Windows retain their shoulders and allow release.
    pocket_inner = INNER_Y / 2 - 0.1
    for side in (-1, 1):
        base -= box_at(
            POCKET_WIDTH, POCKET_OUTER_Y - pocket_inner,
            BASE_HEIGHT - POCKET_BOTTOM + 0.2,
            y=side * (POCKET_OUTER_Y + pocket_inner) / 2, z=POCKET_BOTTOM,
        )
        base -= box_at(
            POCKET_WIDTH, 3.0, LATCH_WINDOW_TOP - LATCH_WINDOW_BOTTOM,
            y=side * 11.3, z=LATCH_WINDOW_BOTTOM,
        )

    if header_exit == "bottom":
        base -= box_at(
            HEADER_SLOT_X, HEADER_SLOT_Y, FLOOR + 0.2,
            x=HEADER_X_ASSUMED, z=-0.1,
        )
    else:
        base -= box_at(
            4.0, HEADER_SLOT_Y, SIDE_PORT_HEIGHT,
            x=11.2, z=SIDE_PORT_BOTTOM,
        )

    base -= antenna_bore(ANTENNA_PORT_Z)
    base.label = f"tiny_base_{header_exit}_header_rev_b"
    base.color = BASE_COLOR
    return base


def make_lid():
    lid = rounded_plate(OUTER_X, OUTER_Y, OUTER_RADIUS, LID_THICKNESS)
    guide = box_at(GUIDE_OUTER, GUIDE_OUTER, GUIDE_DEPTH + 0.1, z=-GUIDE_DEPTH)
    guide -= box_at(
        GUIDE_OUTER - 2 * GUIDE_WALL, GUIDE_OUTER - 2 * GUIDE_WALL,
        GUIDE_DEPTH + 0.3, z=-GUIDE_DEPTH - 0.1,
    )
    # Keep the flex arms free: a 0.1 mm gap to a continuous guide would fuse in FDM.
    for side in (-1, 1):
        guide -= box_at(
            POCKET_WIDTH + 0.8, 2.0, GUIDE_DEPTH + 0.3,
            y=side * 10.0, z=-GUIDE_DEPTH - 0.1,
        )
    lid += guide

    stop_bottom = PCB_TOP + BOARD_VERTICAL_PLAY - BASE_HEIGHT
    for x_sign in (-1, 1):
        for y_sign in (-1, 1):
            lid += box_at(
                SUPPORT_SIZE, SUPPORT_SIZE, -stop_bottom + 0.1,
                x=x_sign * SUPPORT_CENTRE, y=y_sign * SUPPORT_CENTRE, z=stop_bottom,
            )

    for side in (-1, 1):
        lid += box_at(
            CLIP_WIDTH, CLIP_THICKNESS, CLIP_LENGTH + 0.1,
            y=side * (CLIP_OUTER_Y - CLIP_THICKNESS / 2), z=-CLIP_LENGTH,
        )
        # Lead-in ramp at the free end; flat upper shoulder catches the base window.
        shoulder_z = -CLIP_LENGTH + HOOK_RAMP_HEIGHT
        ramp = Plane.YZ * Polygon(
            (side * (CLIP_OUTER_Y - 0.03), -CLIP_LENGTH),
            (side * (CLIP_OUTER_Y + HOOK_PROJECTION), shoulder_z),
            (side * (CLIP_OUTER_Y - 0.03), shoulder_z),
            align=None,
        )
        lid += Pos(-CLIP_WIDTH / 2, 0, 0) * extrude(ramp, amount=CLIP_WIDTH, dir=(1, 0, 0))
        # Root reinforcement remains above the assumed module envelope.
        root_inner = CLIP_OUTER_Y - CLIP_THICKNESS
        root = Plane.YZ * Polygon(
            (side * (root_inner - 0.4), 0.05),
            (side * (root_inner + 0.03), 0.05),
            (side * (root_inner + 0.03), -0.8),
            align=None,
        )
        lid += Pos(-CLIP_WIDTH / 2, 0, 0) * extrude(root, amount=CLIP_WIDTH, dir=(1, 0, 0))

    # A shallow orientation mark points toward the header side without a brand logo.
    mark = Pos(0, 0, LID_THICKNESS - 0.25) * Polygon(
        (7.5, -1.3), (9.3, 0), (7.5, 1.3), align=None,
    )
    lid -= extrude(mark, amount=0.35)
    # Clear only the low edge of the inner guide; the exterior lid and seam stay whole.
    lid -= antenna_bore(ANTENNA_PORT_Z - BASE_HEIGHT)
    lid.label = "tiny_snap_lid_rev_b"
    lid.color = LID_COLOR
    return lid


def make_assembly(header_exit="bottom"):
    base = make_base(header_exit)
    lid = make_lid()
    RigidJoint("lid_seat", base, Location((0, 0, BASE_HEIGHT)))
    RigidJoint("underside", lid, Location((0, 0, 0)))
    base.joints["lid_seat"].connect_to(lid.joints["underside"])
    return Compound(label=f"tiny_snap_case_{header_exit}_rev_b", children=[base, lid])


def make_print_lid():
    lid = Pos(0, 0, LID_THICKNESS) * Rot(180, 0, 0) * make_lid()
    lid.label = "tiny_lid_print_outer_face_down"
    return lid


def reference_board():
    board = box_at(PCB_X, PCB_Y, PCB_THICKNESS_ASSUMED, z=PCB_BOTTOM)
    board.label = "REFERENCE_ONLY_unmeasured_pcb_20x20x1"
    board.color = Color(0.12, 0.44, 0.31)
    return board


def reference_module():
    module = box_at(16.0, 16.0, 5.0, x=-1.0, z=PCB_TOP)
    module.label = "REFERENCE_ONLY_unmeasured_module_envelope"
    module.color = Color(0.62, 0.65, 0.68)
    return module


def reference_pins():
    pins = []
    for index in range(HEADER_COUNT):
        pin = box_at(
            0.64, 0.64, 6.0,
            x=HEADER_X_ASSUMED, y=(index - (HEADER_COUNT - 1) / 2) * HEADER_PITCH,
            z=PCB_BOTTOM - 6.0,
        )
        pin.label = f"REFERENCE_ONLY_pin_{index + 1}_unmeasured_position"
        pin.color = Color(0.77, 0.62, 0.25)
        pins.append(pin)
    return pins


def make_exploded():
    base = make_base()
    lid = Pos(0, 0, BASE_HEIGHT + 12.0) * make_lid()
    return Compound(
        label="tiny_case_exploded_with_assumed_reference_geometry",
        children=[base, reference_board(), reference_module(), *reference_pins(), lid],
    )


def gen_step():
    return make_assembly()
