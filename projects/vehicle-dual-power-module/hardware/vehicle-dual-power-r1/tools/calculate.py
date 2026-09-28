#!/usr/bin/env python3
"""Reproducible first-order calculations, not SPICE or hardware validation."""
import itertools
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def fb(vref, top, bottom, rtol, vtol):
    return {"nominal_V":vref*(1+top/bottom),
            "minimum_V":vref*(1-vtol)*(1+top*(1-rtol)/(bottom*(1+rtol))),
            "maximum_V":vref*(1+vtol)*(1+top*(1+rtol)/(bottom*(1-rtol)))}


def calculate():
    rt = {r["celsius"]:r for r in json.loads((ROOT/"source/ntc-rt.json").read_text())["rows"]}
    # 1% resistor tolerance plus 100ppm/K over a conservative 60K excursion.
    # Actual selected control switch limits declared ambient operation to -20..70C.
    tol=.016
    def ratio(t,key,at,bt):
        ntc=rt[t][key]*1000
        low=1/(1/ntc+1/(1.5e6*bt))
        return low/(9760*at+low)
    def crossing(key,at,bt,v):
        for t in range(-5,55):
            a,b=ratio(t,key,at,bt),ratio(t+1,key,at,bt)
            if a>=v>=b:return t+(a-v)/(a-b)
        raise ValueError("temperature outside retained R/T data")
    temps={}
    thresholds={"cold_stop":(.724,.733,.742),"cold_restart":(.690,.715,.740),
                "hot_stop":(.337,.342,.351),"hot_restart":(.345,.353,.362),
                "cool_half_current":(.672,.680,.690),"warm_lower_voltage":(.438,.447,.458)}
    for name,(lo,nom,hi) in thresholds.items():
        corners=[crossing(key,a,b,t) for key,a,b,t in itertools.product(
            ["min_kohm","max_kohm"],[1-tol,1+tol],[1-tol,1+tol],[lo,hi])]
        temps[name]={"minimum_C":min(corners),"nominal_C":crossing("nom_kohm",1,1,nom),"maximum_C":max(corners)}
    # 0.1% feedback resistors + 25ppm/K * 60K, VFB_PWM guaranteed +/-1%.
    host=fb(.5,91900,10000,.0025,.01)
    modem=fb(.5,100000,14700,.0025,.01)
    # Extra +/-1% is an engineering allocation for line/load regulation.
    # TI lists typical line/load values, so these endpoints are NOT guaranteed specs.
    host_min=host["minimum_V"]*.99-.024-.120-.060
    host_max=host["maximum_V"]*1.01
    modem_min=modem["minimum_V"]*.99-2*.025-2*.060
    modem_max=modem["maximum_V"]*1.01
    # Minimum BATSYS accepted under full-load: 3.40V actual; 3.45V nominal ADC threshold.
    vmux=3.40-3*.038
    eta=.85
    host_i=host["maximum_V"]/(vmux*eta)
    modem_i=modem["maximum_V"]*2/(vmux*eta)
    total_i=host_i+modem_i
    fet_pair_r=2*.013*1.6  # 1.6 temperature multiplier is a design allocation, not a max guarantee.
    # Standby: estimates/allocations, deliberately includes pull-up currents and pack protection.
    standby={
        "BQ25606_no_VBUS": {"target_uA":58,"allocation_uA":85,"basis":"datasheet typ/max, VBAT4.5V, TJ<85C"},
        "two_TPS2117": {"target_uA":2.7,"allocation_uA":9,"basis":"selected-source IQ; use hot max allowance"},
        "two_TPS63020_shutdown": {"target_uA":.2,"allocation_uA":2,"basis":"datasheet shutdown typ/max at3.6V"},
        "LM66200": {"target_uA":1.32,"allocation_uA":4.5,"basis":"datasheet IQ"},
        "TPS7A02_AON": {"target_uA":.025,"allocation_uA":1,"basis":"25nA typical; extra leakage allocation"},
        "STM32_stop_and_periodic_work": {"target_uA":3,"allocation_uA":20,"basis":"includes wake/ADC/watchdog duty cycle; firmware unimplemented"},
        "LIS2DW12_LP_12p5Hz": {"target_uA":1,"allocation_uA":10,"basis":"1uA typical, no guaranteed maximum for this mode"},
        "battery_ADC_divider": {"target_uA":14,"allocation_uA":14.3,"basis":"4.2V/300k; resistor tolerance"},
        "two_mux_status_pullups": {"target_uA":18.2,"allocation_uA":19,"basis":"2*3V/330k, battery-selected ST low"},
        "two_regulator_PG_pullups": {"target_uA":18.2,"allocation_uA":19,"basis":"2*3V/330k conservative disabled-low assumption"},
        "charger_status_pullup": {"target_uA":0,"allocation_uA":31,"basis":"allow STAT low; characterize no-VBUS state"},
        "battery_gate_bias": {"target_uA":4.2,"allocation_uA":4.3,"basis":"4.2V/1M"},
        "UART_and_other_leakage": {"target_uA":4,"allocation_uA":25,"basis":"powered-off translator/control leakage allocation"},
        "external_pack_protection": {"target_uA":15,"allocation_uA":30,"basis":"procurement requirement; pack not selected"},
        "external_wake_sensor": {"target_uA":50,"allocation_uA":100,"basis":"sensor selection budget, including low-level duty cycle"},
    }
    target=sum(x["target_uA"] for x in standby.values());alloc=sum(x["allocation_uA"] for x in standby.values())
    return dict(status="calculated-design-allocations-not-hardware-verified",temperature_C=temps,
        ntc_ratio_at_0C_min=min(ratio(0,k,a,b) for k,a,b in itertools.product(["min_kohm","max_kohm"],[1-tol,1+tol],[1-tol,1+tol])),
        ntc_ratio_at_45C_max=max(ratio(45,k,a,b) for k,a,b in itertools.product(["min_kohm","max_kohm"],[1-tol,1+tol],[1-tol,1+tol])),
        host_feedback=host,modem_feedback=modem,
        endpoint_allocations_V=dict(host_min=host_min,host_max=host_max,modem_min=modem_min,modem_max=modem_max),
        vehicle_buck=fb(.8,53600,10000,.016,.02),
        vehicle_ov=fb(1.231,249000,16900,.016,(1.267-1.231)/1.231),
        mux_priority_threshold=fb(1,330000,100000,.016,.08),
        current=dict(regulator_input_floor_V=vmux,assumed_efficiency=eta,host_A=host_i,modem_A=modem_i,
                     total_battery_A=total_i,fet_pair_R_allocation_ohm=fet_pair_r,
                     fet_pair_loss_W=total_i**2*fet_pair_r,
                     per_mux_loss_W=max(host_i,modem_i)**2*.038,
                     car_buck_full_load_A=(host["maximum_V"]+2*modem["maximum_V"])/(.85*5.0)+.85),
        charger=dict(nominal_current_A=.615,min_current_A=.516/1.01,max_current_A=.715/.99,
                     nominal_CV_V=4.208,maximum_CV_V=4.208*1.005,
                     ilim_nominal_A=478/620,ilim_min_A=459/(620*1.01),ilim_max_A=500/(620*.99)),
        transient_allocations=dict(modem_cap_min_F=470e-6*.8,
            modem_step_2A_20us_cap_droop_V=2*20e-6/(470e-6*.8),modem_step_2A_esr_drop_V=2*.015,
            modem_switch_typical_rise_s=(.38*10000+34)*3.901e-6,
            modem_bulk_typical_inrush_A=470e-6/(.38*10000+34)*1e6,
            note="20us is a design scenario; mux switch time has no guaranteed maximum. Bench verify with actual Tiny."),
        standby_uA=standby,standby_target_sum_uA=target,standby_allocation_sum_uA=alloc,
        seven_days_mAh=dict(target=target*.001*168,allocation=alloc*.001*168,acceptance_0p5mA=84),
        notes=["Efficiency, temperature multiplier, line/load allowance and sensor/pack current are assumptions requiring measurement.",
               "No continuous 2A modem guarantee below regulator input3.286V or outside verified thermal conditions.",
               "Source mux 4A rating applies separately to each rail; not the combined pack current.",
               "Seven-day calculation excludes wake/transmit events, cell self-discharge and capacity aging."])


if __name__=="__main__":
    out=calculate();(ROOT/"calculations.json").write_text(json.dumps(out,ensure_ascii=False,indent=2)+"\n")
    print(json.dumps({k:out[k] for k in ["temperature_C","endpoint_allocations_V","current",
                     "standby_target_sum_uA","standby_allocation_sum_uA","seven_days_mAh"]},indent=2))
