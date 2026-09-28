#!/usr/bin/env python3
"""Conservative hand-design estimates for the selectable 5V/2A VIN path.

These checks bound schematic choices. They do not prove loop stability, thermal
performance, transient response or voltage at a particular external harness.
"""
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def calculate():
    car = 0.8 * (1 + 71.5 / 10)
    battery = 1.204 * (1 + 249 / 56)
    post = 0.596 * (1 + 100 / 13.3)
    output_power = post * 2
    post_efficiency = 0.90
    boost_efficiency = 0.85
    battery_min = 3.0  # BMS/firmware must stop before loaded pack voltage falls below this.
    boost_current = output_power / (post_efficiency * battery)
    battery_current = battery * boost_current / (boost_efficiency * battery_min)
    boost_frequency_min = 500_000
    inductance_min = 1.5e-6 * 0.70
    ripple = battery_min * (1 - battery_min / battery) / (inductance_min * boost_frequency_min)
    peak = battery_current + ripple / 2
    ilim_typ = 550000 / 60400
    ilim_min_estimate = ilim_typ - 1.3  # TI states up to 1.3A lower than nominal.
    mux_limit_typ = 65.2 / (30 ** 0.861)
    car_priority_falling = battery * (100 / (243 + 100)) / (100 / (200 + 100))
    mux_ov_typ = 1.06 * (1 + 71.5 / 10)
    mux_hot_drop = boost_current * 0.100  # TPS2121 125C max RON, VIN >= 5V.
    car_input_low = car * 0.97 - mux_hot_drop
    host_battery_current = 5.0 / (0.85 * battery_min)  # 5V/1A host rail, assumed 85%.
    output_hot_drop = 2 * (0.0084 + 0.030)  # load switch + allocated cable/connector loop.
    post_ref_low = 0.581 * (1 + 99 / 13.433)
    post_ref_high = 0.611 * (1 + 101 / 13.167)
    result = {
        "status": "design-estimate-only",
        "nominal_V": {"vehicle_preregulator": car, "battery_boost": battery,
                      "regulated_5v": post},
        "assumptions": {"vin_output_A": 2.0, "loaded_battery_min_V": battery_min,
                        "post_efficiency": post_efficiency, "boost_efficiency": boost_efficiency,
                        "boost_frequency_min_Hz": boost_frequency_min,
                        "boost_inductance_min_H": inductance_min,
                        "host_battery_efficiency": 0.85,
                        "cable_connector_loop_ohm": 0.030},
        "boost": {"output_A": boost_current, "battery_input_A": battery_current,
                  "inductor_ripple_A": ripple, "inductor_peak_A": peak,
                  "switch_limit_typ_A": ilim_typ,
                  "switch_limit_min_estimate_A": ilim_min_estimate,
                  "inductor_isat_A": 14.0},
        "combined_battery_A": battery_current + host_battery_current,
        "mux": {"car_priority_falling_V": car_priority_falling,
                "ov_threshold_typ_V": mux_ov_typ,
                "current_limit_typ_A": mux_limit_typ,
                "hot_drop_V": mux_hot_drop,
                "low_car_input_after_mux_V": car_input_low},
        "endpoint": {"nominal_after_hot_drop_V": post - output_hot_drop,
                     "pessimistic_low_V": post_ref_low - output_hot_drop,
                     "pessimistic_high_V": post_ref_high,
                     "strict_5v_plusminus_5pct_guaranteed": post_ref_high <= 5.25 and post_ref_low - output_hot_drop >= 4.75},
        "open_checks": ["Loaded battery voltage and pack protection trip under pulse load",
                        "TPS61088 compensation phase and gain margins with effective capacitor values",
                        "Thermal loss and transient droop during TPS2121 source transfer",
                        "5V endpoint tolerance at worst temperature, selected harness and 4G pulse"],
    }
    assert peak < ilim_min_estimate, (peak, ilim_min_estimate)
    assert ilim_typ < 9.2, ilim_typ
    assert battery_current + host_battery_current < 9.0
    assert car_input_low > 5.7
    return result


if __name__ == "__main__":
    result = calculate()
    (ROOT / "vin-calculations.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(result, ensure_ascii=False, indent=2))
