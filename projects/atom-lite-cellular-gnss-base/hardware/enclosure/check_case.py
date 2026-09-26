"""Check exported Rev B geometry and assembly clearances; no physical-fit claim."""

import json
from pathlib import Path

import trimesh
from build123d import Pos, import_step

import tiny_case as case

HERE = Path(__file__).resolve().parent
TOL = 1e-6


def overlap(a, b):
    common = a.intersect(b)
    if common is None:
        return 0.0
    return common.volume if hasattr(common, "volume") else sum(s.volume for s in common)


def valid(shape):
    return all(s.is_valid() and s.volume > 0 for s in shape.solids())


def main():
    report = {"printed_parts": {}, "assemblies": {}}
    for stem in ("tiny_case_base", "tiny_case_side_base", "tiny_case_lid"):
        shape = import_step(HERE / f"{stem}.step")
        mesh = trimesh.load_mesh(HERE / f"{stem}.stl")
        assert len(shape.solids()) == 1 and valid(shape), stem
        assert mesh.is_watertight and mesh.volume > 0, stem
        assert abs(mesh.volume - shape.volume) / shape.volume < 0.001, stem
        assert abs(shape.bounding_box().min.Z) < TOL, (stem, "print-bed placement")
        report["printed_parts"][stem] = {
            "solids": 1, "brep_valid": True, "stl_watertight": True,
            "bbox_mm": list(shape.bounding_box().size),
            "step_volume_mm3": round(shape.volume, 5),
            "stl_volume_mm3": round(float(mesh.volume), 5),
        }

    for variant, stem in (("bottom", "tiny_case"), ("side", "tiny_case_side_exit")):
        exported = import_step(HERE / f"{stem}.step")
        assert len(exported.solids()) == 2 and valid(exported), stem
        assert all(abs(a - b) < TOL for a, b in zip(exported.bounding_box().size, [24, 24, 11.8])), stem
        # Use actual exported solids for final rigid assembly collision checks.
        a, b = exported.solids()
        assert overlap(a, b) < TOL, (stem, "exported parts interfere")
        assembly = case.make_assembly(variant)
        base, lid = assembly.children
        assert abs(exported.volume - base.volume - lid.volume) < TOL
        for ref in (case.reference_board(), case.reference_module()):
            assert overlap(ref, base) < TOL and overlap(ref, lid) < TOL, (stem, ref.label)

        if variant == "bottom":
            probes = case.reference_pins()
        else:
            probes = [case.box_at(
                6.0, 0.64, 0.64, x=11.0,
                y=(i - 2.5) * 2.54, z=4.0,
            ) for i in range(6)]
        for probe in probes:
            assert overlap(probe, base) < TOL and overlap(probe, lid) < TOL, (stem, "header blocked")
        cable = case.antenna_bore(case.ANTENNA_PORT_Z, diameter=2.8)
        assert overlap(cable, base) < TOL and overlap(cable, lid) < TOL, (stem, "antenna route blocked")
        oversized = case.antenna_bore(case.ANTENNA_PORT_Z, diameter=3.2)
        assert overlap(oversized, base) > 0.01, (stem, "round hole rim missing")
        assert case.ANTENNA_PORT_Z + case.ANTENNA_PORT_DIAMETER / 2 < case.BASE_HEIGHT, (stem, "antenna opening reaches seam")
        # The round exit must have intact material above and below on the outside wall.
        for rim_z in (case.ANTENNA_PORT_Z - 1.8, case.ANTENNA_PORT_Z + 1.6):
            rim = case.box_at(0.5, 0.5, 0.2, x=11.6, z=rim_z)
            assert abs(overlap(rim, base) - rim.volume) < TOL, (stem, "outer rim interrupted")

        # A rigid upward displacement must encounter the latch shoulders.
        # This proves geometric retention only, not clip strength or fatigue life.
        catch_volume = overlap(base, Pos(0, 0, 0.6) * lid)
        assert catch_volume > 0.01, (stem, "no geometric latch retention")
        report["assemblies"][variant] = {
            "solids": 2, "bbox_mm": [24, 24, 11.8], "seated_overlap_mm3": 0,
            "six_header_probes_clear": True, "round_antenna_port_2_8_mm_probe_clear": True,
            "round_antenna_port_3_2_mm_probe_hits_base_rim": True,
            "round_antenna_port_within_base_and_outer_rims_intact": True,
            "reference_board_and_module_clear": True,
            "rigid_lift_0_6_mm_latch_interference_mm3": round(catch_volume, 5),
        }
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
