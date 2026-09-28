"""Electrical intent checks; these do not replace ERC, simulation or bench tests."""
import importlib.util
import json
import unittest
from collections import Counter, defaultdict, deque
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("calculate",ROOT/"tools/calculate.py")
calc=importlib.util.module_from_spec(spec);spec.loader.exec_module(calc)
vin_spec=importlib.util.spec_from_file_location("calculate_vin",ROOT/"tools/calculate_vin.py")
vin_calc=importlib.util.module_from_spec(vin_spec);vin_spec.loader.exec_module(vin_calc)
D=json.loads((ROOT/"source/design.json").read_text())
P=json.loads((ROOT/"source/selected-parts.json").read_text())
C={c["ref"]:c for c in D["components"]}


def pin(ref,num):return C[ref]["pins"][str(num)]


def reachable(edges,start,end):
    graph=defaultdict(set)
    for a,b in edges:graph[a].add(b)
    todo=deque([start]);seen=set()
    while todo:
        n=todo.popleft()
        if n==end:return True
        if n not in seen:seen.add(n);todo.extend(graph[n]-seen)
    return False


class ElectricalIntent(unittest.TestCase):
    def test_all_library_pins_have_one_explicit_state(self):
        self.assertEqual(len(C),len(D["components"]))
        for c in C.values():
            with self.subTest(ref=c["ref"]):
                self.assertEqual(set(c["pins"]),{p["number"] for p in P[c["lcsc"]]["pins"]})
                self.assertEqual({k for k,v in c["pins"].items() if v is None},set(c["ncReasons"]))
                self.assertEqual(P[c["lcsc"]]["pinEvidence"],"official-placed-symbol-readback")

    def test_four_page_membership_and_unique_ownership(self):
        claimed=[ref for z in D["zones"] for ref in z["members"]]
        self.assertEqual(Counter(claimed),Counter(C.keys()))
        self.assertEqual(set(c["page"] for c in C.values()),{1,2,3,4})
        for z in D["zones"]:
            self.assertIn(z["core"],z["members"])

    def test_explicit_peripherals_share_actual_owner_net(self):
        for c in C.values():
            if "attachment" in c:
                a=c["attachment"]
                self.assertEqual(c["zone"],C[a["owner"]]["zone"])
                self.assertEqual(c["pins"][a["pin"]],pin(a["owner"],a["ownerPin"]))

    def test_master_off_breaks_both_body_diode_paths(self):
        # N-channel body diode goes S -> D; P-channel goes D -> S.
        car=[(pin(q,1),pin(q,5)) for q in ["Q101","Q102"]]
        bat=[(pin(q,5),pin(q,1)) for q in ["Q201","Q202"]]
        self.assertFalse(reachable(car,"VEH_FUSED","VEH_PROT"))
        self.assertFalse(reachable(car,"VEH_PROT","VEH_FUSED"))
        self.assertFalse(reachable(bat,"PACK_FUSED","BAT_SW"))
        self.assertFalse(reachable(bat,"BAT_SW","PACK_FUSED"))
        self.assertEqual(pin("SW101",3),"GND")
        self.assertEqual(pin("SW101",2),pin("U101",6))
        self.assertEqual(pin("SW101",6),pin("Q201",1))
        self.assertEqual(pin("SW101",5),pin("R201",1))

    def test_cold_start_does_not_depend_on_main_buck(self):
        self.assertEqual(pin("U103",8),"VEH_PROT")
        self.assertEqual(pin("U103",5),"VEH_PROT")
        self.assertEqual(pin("U103",1),pin("U204",3))
        self.assertEqual(pin("U204",2),pin("U205",1))
        self.assertEqual(pin("U205",5),pin("U401",1))

    def test_charger_voltage_temperature_and_default_disable(self):
        self.assertIsNone(pin("U201",12))
        self.assertIsNone(pin("U201",3));self.assertIsNone(pin("U201",4))
        self.assertEqual(pin("U201",6),"GND")
        self.assertEqual(C["R204"]["lcsc"],"C22764")
        self.assertEqual(pin("R207",1),pin("U201",22))
        self.assertEqual(pin("R207",2),pin("U201",9))
        self.assertEqual(pin("J202",1),pin("U201",11))
        result=calc.calculate()
        self.assertGreater(result["temperature_C"]["cold_stop"]["minimum_C"],0)
        self.assertLess(result["temperature_C"]["hot_stop"]["maximum_C"],45)
        self.assertGreater(result["ntc_ratio_at_0C_min"],.742)
        self.assertLess(result["ntc_ratio_at_45C_max"],.337)

    def test_separate_muxes_and_forced_pwm(self):
        self.assertNotEqual(pin("U202",2),pin("U203",2))
        for u in ["U202","U203"]:
            self.assertEqual(pin(u,3),"CAR_5V")
            self.assertEqual(pin(u,5),"CAR_5V")
            self.assertEqual(pin(u,6),"BAT_SW")
        for u in ["U301","U304"]:
            self.assertEqual(pin(u,13),pin(u,10))

    def test_host_disconnect_and_usb_adc_backfeed_boundary(self):
        self.assertEqual(C["U302"]["lcsc"],"C471046")
        self.assertEqual(pin("U303",3),pin("U303",6))
        self.assertEqual(pin("U303",1),pin("U302",9))
        self.assertEqual(pin("R413",1),"HOST_SW")
        self.assertNotEqual(pin("R413",1),"HOST_5V")
        for u in ["U307","U403"]:self.assertEqual(C[u]["lcsc"],"C5186957")

    def test_tiny_voltage_exclusive_connectors(self):
        for ref in ["J301","J302","J303"]:self.assertEqual(pin(ref,1),"HOST_5V")
        for ref in ["J304","J305","J306"]:self.assertEqual(pin(ref,1),"TINY_BAT")
        for ref in ["J309","J310","J311"]:self.assertEqual(pin(ref,1),"OUT_VIN_5V")
        for ref in ["J301","J302","J303","J304","J305","J306","J309","J310","J311"]:
            self.assertEqual(pin(ref,2),"GND")
        self.assertEqual(pin("SW301",2),"CTL_MODEM_REG_EN")
        self.assertEqual(pin("SW301",5),"CTL_MODEM_LOAD_EN")
        self.assertEqual(pin("SW301",1),pin("R308",1))
        self.assertEqual(pin("SW301",4),pin("R315",1))
        self.assertEqual(pin("SW301",3),pin("R121",1))
        self.assertEqual(pin("SW301",3),pin("R222",1))
        self.assertEqual(pin("SW301",3),pin("R329",1))
        self.assertEqual(pin("SW301",6),pin("R331",1))
        self.assertNotEqual(pin("SW301",1),pin("SW301",3))
        self.assertNotEqual(pin("SW301",4),pin("SW301",6))

    def test_regulated_vin_chain_and_off_isolation(self):
        self.assertEqual(C["U105"]["lcsc"],"C1355305")
        self.assertEqual(C["U206"]["lcsc"],"C1850341")
        self.assertEqual(C["U308"]["lcsc"],"C485916")
        self.assertEqual(C["U310"]["lcsc"],"C311983")
        self.assertEqual(C["U309"]["lcsc"],"C17294173")
        self.assertEqual(pin("U105",2),"VEH_PROT")
        self.assertEqual(pin("U206",9),"BAT_SW")
        self.assertEqual(pin("U308",7),"VIN_CAR_6V5")
        self.assertEqual(pin("U308",2),"VIN_BAT_6V5")
        self.assertEqual(pin("U308",1),pin("U310",3))
        self.assertEqual(pin("U310",2),pin("L303",1))
        self.assertEqual(pin("U310",5),pin("R329",2))
        self.assertEqual(pin("U310",4),pin("R328",1))
        self.assertEqual(pin("U310",3),pin("U308",8))
        self.assertEqual(pin("U309",5),"VIN_5V_REG")
        self.assertEqual(pin("U309",6),pin("J309",1))
        self.assertEqual(pin("U309",1),pin("R332",1))
        self.assertIsNone(pin("U309",10))
        self.assertEqual(pin("U306",1),"AON_RAW")
        self.assertEqual(pin("U306",3),"AON_RAW")
        self.assertEqual(pin("U307",6),"CTL_MODEM_LOAD_EN")
        r=vin_calc.calculate()
        self.assertLess(r["boost"]["inductor_peak_A"],r["boost"]["switch_limit_min_estimate_A"])
        self.assertLess(r["combined_battery_A"],9)
        self.assertGreater(r["mux"]["low_car_input_after_mux_V"],5.7)

    def test_high_current_caps_are_not_on_unswitched_battery(self):
        self.assertEqual(pin("C301",1),"TINY_BAT")
        self.assertEqual(pin("C301",2),"GND")
        self.assertEqual(pin("U101",13),None)
        for p in [3,4]:self.assertIsNone(pin("J201",p))

    def test_voltage_current_and_standby_allocations(self):
        r=calc.calculate();v=r["endpoint_allocations_V"]
        self.assertGreaterEqual(v["host_min"],4.75);self.assertLessEqual(v["host_max"],5.25)
        self.assertGreaterEqual(v["modem_min"],3.4);self.assertLessEqual(v["modem_max"],4.2)
        self.assertLess(r["current"]["host_A"],4);self.assertLess(r["current"]["modem_A"],4)
        self.assertLess(r["current"]["total_battery_A"],9)
        self.assertLess(r["standby_target_sum_uA"],200)
        self.assertLess(r["standby_allocation_sum_uA"],500)


if __name__=="__main__":unittest.main()
