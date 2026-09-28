#!/usr/bin/env python3
"""Generate the electrical source and BOM. Does not mutate the EDA project.

Every physical library pin needs an explicit net or documented NC. Geometry is
measured separately; this file must never manufacture measured symbol geometry.
"""
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARTS = json.loads((ROOT / "source/parts-lock.json").read_text())
PROJECT = json.loads((ROOT / "eda-project.json").read_text())
components, zones = [], []
current_zone = None


def zone(page, name, title, core):
    global current_zone
    current_zone = dict(id=name, page=page, title=title, core=core, members=[])
    zones.append(current_zone)


def add(ref, code, mapping, role, attach=None, nc=None):
    """Numeric pin map: None means NC and requires a reason in nc."""
    part = PARTS[code]
    mapping = {str(k): v for k, v in mapping.items()}
    nc = {str(k): v for k, v in (nc or {}).items()}
    expected = {p["number"] for p in part["pins"]}
    assert set(mapping) == expected, (ref, expected - set(mapping), set(mapping) - expected)
    assert {k for k, v in mapping.items() if v is None} == set(nc), ref
    assert not any(c["ref"] == ref for c in components), ref
    c = dict(id="cmp-" + ref, ref=ref, page=current_zone["page"], zone=current_zone["id"],
             lcsc=code, role=role, pins=mapping, ncReasons=nc)
    if attach:
        c["attachment"] = dict(pin=str(attach[0]), owner=attach[1], ownerPin=str(attach[2]))
    components.append(c)
    current_zone["members"].append(ref)
    return ref


RVALUES = {"0": "C21189", "100": "C22775", "1k": "C21190", "10k": "C25804",
           "100k": "C25803", "1M": "C22935", "53.6k": "C23074", "243k": "C23351",
           "16.9k": "C25954", "1.1k": "C22764", "620": "C23220", "330k": "C23137",
           "4.7k": "C23162", "470k": "C23178", "22k": "C31850", "200k": "C25811",
           "249k": "C22918", "9.76k": "C23128", "1.5M": "C4172", "750_1206": "C17985",
           "100k_0.1": "C122538", "90.9k_0.1": "C728600", "1k_0.1": "C110776",
           "10k_0.1": "C95204", "14.7k_0.1": "C705725"}
CVALUES = {"100n": "C1591", "1u": "C15849", "4.7u": "C19666", "22u": "C12891",
           "2.2u100V": "C86054", "4.7n": "C1621", "47p": "C94904", "47n": "C1622",
           "10n": "C1589", "1n": "C1588", "10u50V": "C440198", "470u6.3V": "C128503"}


def r(ref, value, a, b, role, attach=None):
    return add(ref, RVALUES[value], {1:a, 2:b}, role, attach)


def c(ref, value, a, b="GND", role="decoupling", attach=None):
    return add(ref, CVALUES[value], {1:a, 2:b}, role, attach)


def nmos(ref, code, drain, gate, source, role, attach=None):
    names = {p["number"]: p["name"] for p in PARTS[code]["pins"]}
    return add(ref, code, {k:{"D":drain,"G":gate,"S":source}[v] for k,v in names.items()}, role, attach)


# PAGE 1 — vehicle supply, independent cold-start regulator and master pole A.
zone(1, "vehicle-protection", "VEHICLE INPUT / REVERSE AND OV", "U101")
add("J101", "C474882", {1:"VEH_IN",2:"GND",3:"ACC_IN"}, "vehicle input: power / return / ACC")
add("F101", "C48467", {1:"VEH_IN",2:"VEH_FUSED"}, "5A input fuse; upstream vehicle fuse also required")
add("D101", "C1982094", {1:"VEH_FUSED",2:"GND"}, "bidirectional 33V TVS after input fuse")
nmos("Q101", "C114200", "VEH_CD", "VEH_DGATE", "VEH_FUSED", "100V ideal-diode MOSFET")
nmos("Q102", "C114200", "VEH_CD", "VEH_HGATE", "VEH_PROT", "100V disconnect MOSFET, common drain")
add("U101", "C3215600", {1:"VEH_DGATE",2:"VEH_FUSED",3:"VEH_FUSED",4:"VEH_SENSE",
    5:"VEH_OV",6:"MASTER_CAR_EN",7:"GND",8:"VEH_HGATE",9:"VEH_PROT",10:"VEH_CD",
    11:"VEH_CP",12:"VEH_CD",13:None}, "LM74800 protection controller",
    nc={13:"Datasheet RTN exposed pad must float; no ground copper connection."})
c("C101", "2.2u100V", "VEH_CD", attach=(1,"U101",10))
c("C102", "100n", "VEH_CP", "VEH_CD", "charge pump capacitor, 50V differential rating", (1,"U101",11))
r("R101", "249k", "VEH_SENSE", "VEH_OV", "overvoltage upper divider", (1,"U101",4))
r("R102", "16.9k", "VEH_OV", "GND", "overvoltage lower divider", (1,"R101",2))

zone(1, "master-control", "DPDT MASTER / LOW CURRENT ONLY", "SW101")
add("SW101", "C225088", {1:"MASTER_CLAMP",2:"MASTER_CAR_EN",3:"GND",
    4:"GND",5:"BAT_GATE_CTL",6:"BAT_CS"}, "ON: 1-2 and 4-5; OFF: 2-3 and 5-6")
r("R103", "22k", "VEH_FUSED", "MASTER_RMID", "master control current limiting stage 1", (2,"R104",1))
r("R104", "22k", "MASTER_RMID", "MASTER_CLAMP", "master control current limiting stage 2", (2,"SW101",1))
add("D103", "C213116", {1:"MASTER_CLAMP",2:"GND"}, "5.1V zener protects 6V slide contacts", (1,"SW101",1))
r("R105", "100k", "MASTER_CAR_EN", "GND", "vehicle master default off", (1,"SW101",2))
c("C103", "10n", "MASTER_CLAMP", attach=(1,"SW101",1))

zone(1, "vehicle-buck", "VEHICLE 5.088V / 4A DESIGN LOAD", "U102")
add("U102", "C1355305", {1:"CAR_BOOT",2:"VEH_PROT",3:"CAR_BUCK_EN",4:"CAR_RT",5:"CAR_FB",
    6:"CAR_COMP",7:"GND",8:"CAR_SW",9:"GND"}, "vehicle step-down regulator")
for k in range(104,108): c("C"+str(k), "2.2u100V", "VEH_PROT", attach=(1,"U102",2) if k==104 else (1,"C"+str(k-1),1))
c("C108", "100n", "CAR_BOOT", "CAR_SW", "bootstrap capacitor", (1,"U102",1))
add("D102", "C266541", {1:"CAR_SW",2:"GND"}, "5A 60V catch Schottky: pin1 cathode", (1,"U102",8))
add("L101", "C3911753", {1:"CAR_SW",2:"CAR_5V"}, "6.8uH buck inductor", (1,"U102",8))
for k in range(109,115): c("C"+str(k), "22u", "CAR_5V", attach=(1,"L101",2) if k==109 else (1,"C"+str(k-1),1))
r("R106", "53.6k", "CAR_5V", "CAR_FB", "buck feedback upper", (2,"U102",5))
r("R107", "10k", "CAR_FB", "GND", "buck feedback lower", (1,"R106",2))
r("R108", "243k", "CAR_RT", "GND", "switching frequency about 400kHz", (1,"U102",4))
r("R109", "16.9k", "CAR_COMP", "CAR_COMP_C", "compensation series R", (1,"U102",6))
c("C115", "4.7n", "CAR_COMP_C", "GND", "compensation series C", (1,"R109",2))
c("C116", "47p", "CAR_COMP", "GND", "compensation high-frequency pole", (1,"U102",6))
r("R110", "1k", "CTL_CAR_EN", "CAR_BUCK_EN", "buck GPIO series resistor", (2,"U102",3))
r("R111", "100k", "CAR_BUCK_EN", "GND", "buck defaults off on MCU reset", (1,"U102",3))

zone(1, "vehicle-aon", "AON VEHICLE COLD START / 5V", "U103")
add("U103", "C468238", {1:"VEH_AON_5V",2:None,3:None,4:"GND",5:"VEH_PROT",6:None,
    7:None,8:"VEH_PROT",9:"GND"}, "TPS7A1650 fixed 5V always-on vehicle LDO",
    nc={2:"Fixed-voltage DNC must remain disconnected.",3:"Optional PG unused.",6:"NC per datasheet.",7:"Optional PG delay unused."})
c("C117", "2.2u100V", "VEH_PROT", attach=(1,"U103",8))
c("C118", "2.2u100V", "VEH_PROT", attach=(1,"U103",8))
c("C119", "10u50V", "VEH_AON_5V", attach=(1,"U103",1))

zone(1, "acc-sense", "ACC OPTO INPUT / MASTER GATED", "U104")
add("U104", "C109227", {1:"ACC_LED_A",2:"ACC_LED_K",3:"GND",4:"ACC_N"}, "ACC optocoupler; shared system return")
for i,(a,b) in enumerate(zip(["ACC_IN","ACC_R1","ACC_R2","ACC_R3"],
                            ["ACC_R1","ACC_R2","ACC_R3","ACC_LED_A"]),112):
    r("R"+str(i), "750_1206", a,b,"ACC LED series 750R / 0.25W")
add("D104", "C2892570", {1:"ACC_LED_A",2:"ACC_LED_K"}, "antiparallel LED reverse protection", (1,"U104",1))
add("D105", "C1982094", {1:"ACC_IN",2:"GND"}, "ACC line transient clamp; fused ACC feed required")
nmos("Q103", "C8545", "ACC_LED_K", "MASTER_CAR_EN", "GND", "master OFF interrupts optocoupler LED", (3,"U104",2))
r("R116", "100k", "3V0_AON", "ACC_N", "ACC logic pullup", (2,"U104",4))
c("C120", "10n", "ACC_N", attach=(1,"U104",4))
add("J102", "C492401", {1:"VEH_IN",2:"ACC_IN"}, "cigarette-lighter mode shunt; remove for separate ACC")

# PAGE 2 — pack isolation, charger, independent source muxes and AON OR.
zone(2, "battery-master", "PROTECTED 1S PACK / MASTER ISOLATION", "Q201")
add("J201", "C53373776", {1:"GND",2:"PACK_IN",3:None,4:None}, "XT30PW-M: pack receptacle on PCB",
    nc={3:"Mechanical fixing leg, no electrical net.",4:"Mechanical fixing leg, no electrical net."})
add("F201", "C44479", {1:"PACK_IN",2:"PACK_FUSED"}, "10A board fuse; external pack protection mandatory")
nmos("Q201", "C461052", "PACK_FUSED", "BAT_GATE", "BAT_CS", "P-channel input MOSFET; common-source pair")
nmos("Q202", "C461052", "BAT_SW", "BAT_GATE", "BAT_CS", "P-channel charger/load MOSFET; bidirectional isolation")
r("R201", "100", "BAT_GATE_CTL", "BAT_GATE", "gate series resistor", (2,"Q201",4))
r("R202", "1M", "BAT_CS", "BAT_GATE", "gate-source fail-safe off bias", (2,"Q201",4))
c("C201", "1n", "BAT_CS", "BAT_GATE", "gate-source transient suppression", (2,"Q201",4))
c("C202", "22u", "BAT_SW", attach=(1,"Q202",5))

zone(2, "charger", "BQ25606 / 4.208V / ABOUT 615mA", "U201")
add("U201", "C374063", {1:"CAR_5V",2:None,3:None,4:None,5:"CHG_STAT_N",6:"GND",7:None,
    8:"CHG_ILIM",9:"CHG_CE_N",10:"CHG_ICHG",11:"BAT_TS",12:None,13:"BAT_SW",14:"BAT_SW",
    15:"BAT_SYS",16:"BAT_SYS",17:"GND",18:"GND",19:"CHG_SW",20:"CHG_SW",21:"CHG_BTST",
    22:"CHG_REGN",23:"CHG_PMID",24:"CAR_5V",25:"GND"}, "standalone charger; SYS supplies AON only",
    nc={2:"Datasheet NC.",3:"No USB data source; unknown-adapter detection uses ILIM; bench verify.",
        4:"No USB data source; do not short D+ and D-.",7:"Optional PG output unused.",
        12:"VSET floating selects 4.208V. Ground would select 4.352V."})
c("C203", "10u50V", "CAR_5V", attach=(1,"U201",24))
c("C204", "100n", "CAR_5V", attach=(1,"U201",24))
c("C205", "10u50V", "CHG_PMID", attach=(1,"U201",23))
c("C206", "4.7u", "CHG_REGN", attach=(1,"U201",22))
c("C207", "47n", "CHG_BTST", "CHG_SW", "charger bootstrap", (1,"U201",21))
add("L201", "C920280", {1:"CHG_SW",2:"BAT_SYS"}, "2.2uH charger inductor", (1,"U201",19))
c("C208", "22u", "BAT_SYS", attach=(1,"U201",15))
c("C209", "22u", "BAT_SYS", attach=(1,"U201",16))
c("C210", "10u50V", "BAT_SW", attach=(1,"U201",13))
r("R203", "620", "CHG_ILIM", "GND", "unknown-adapter input limit about 0.77A", (1,"U201",8))
r("R204", "1.1k", "CHG_ICHG", "GND", "nominal charge current 615mA", (1,"U201",10))
r("R205", "9.76k", "CHG_REGN", "BAT_TS", "temperature upper resistor", (2,"U201",11))
r("R206", "1.5M", "BAT_TS", "GND", "temperature parallel resistor", (1,"U201",11))
c("C211", "10n", "BAT_TS", attach=(1,"U201",11))
add("J202", "C2908600", {1:"BAT_TS",2:"GND"}, "external cell-attached C394021 10k NTC; never on PCB", (1,"U201",11))
r("R207", "100k", "CHG_REGN", "CHG_CE_N", "charge disabled until controller allows", (2,"U201",9))
nmos("Q203", "C8545", "CHG_CE_N", "CHG_GATE", "GND", "isolates 5V charger CE from 3V MCU", (3,"U201",9))
r("R208", "1k", "CTL_CHG_EN", "CHG_GATE", "charge-enable gate resistor", (2,"Q203",1))
r("R209", "100k", "CHG_GATE", "GND", "charge default off", (1,"Q203",1))
r("R210", "100k", "3V0_AON", "CHG_STAT_N", "charger status pullup", (2,"U201",5))

for suffix, idx in [("HOST",202),("MODEM",203)]:
    zone(2, "mux-"+suffix.lower(), suffix+" SOURCE MUX / CAR PRIORITY", "U"+str(idx))
    u="U"+str(idx);bus=suffix+"_SRC";pr=suffix+"_PR";st=suffix+"_CAR_ST"
    add(u,"C22399676",{1:"GND",2:bus,3:"CAR_5V",4:pr,5:"CAR_5V",6:"BAT_SW",7:bus,8:st}, "independent 4A priority mux")
    k=212+(idx-202)*9
    c("C"+str(k),"1u","CAR_5V",attach=(1,u,3));c("C"+str(k+1),"1u","BAT_SW",attach=(1,u,6))
    for j in range(4): c("C"+str(k+2+j),"22u",bus,attach=(1,u,2) if j==0 else (1,"C"+str(k+1+j),1))
    rk=211+(idx-202)*3
    r("R"+str(rk),"330k","CAR_5V",pr,"priority threshold upper",(2,u,4))
    r("R"+str(rk+1),"100k",pr,"GND","priority threshold lower",(1,u,4))
    r("R"+str(rk+2),"330k","3V0_AON",st,"mux source status pullup",(2,u,8))

zone(2, "aon-source", "AON SUPPLY OR / CAR OR BAT_SYS", "U204")
add("U204", "C3235556", {1:"GND",2:"AON_RAW",3:"VEH_AON_5V",4:"GND",5:"GND",
    6:"BAT_SYS",7:"AON_RAW",8:None}, "low-quiescent dual ideal-diode OR",
    nc={8:"Optional source-status output unused."})
c("C230","1u","VEH_AON_5V",attach=(1,"U204",3))
c("C231","1u","BAT_SYS",attach=(1,"U204",6))
c("C232","1u","AON_RAW",attach=(1,"U204",2))

zone(2, "aon-ldo", "3.0V ALWAYS-ON / LOW IQ", "U205")
add("U205", "C3747031", {1:"AON_RAW",2:"GND",3:"AON_RAW",4:None,5:"3V0_AON"},
    "AON 3V regulator for controller, IMU and wake port",nc={4:"Datasheet NC."})
c("C233","1u","AON_RAW",attach=(1,"U205",1))
c("C234","4.7u","3V0_AON",attach=(1,"U205",5))

# PAGE 3 — regulated load rails, real load disconnection and wired terminals.
for suffix, u, li, cbase, rbase in [("HOST",301,301,302,301),("MODEM",304,302,312,306)]:
    zone(3,"reg-"+suffix.lower(),suffix+" BUCK-BOOST / FORCED PWM", "U"+str(u))
    un="U"+str(u);out=suffix+"_REG";src=suffix+"_SRC";en=suffix+"_REG_EN";fb=suffix+"_FB"
    add(un,"C15483",{1:src,2:"GND",3:fb,4:out,5:out,6:suffix+"_L2",7:suffix+"_L2",
        8:suffix+"_L1",9:suffix+"_L1",10:src,11:src,12:en,13:src,14:suffix+"_REG_PG",15:"GND"},
        "TPS63020 forced PWM; disabled during standby")
    add("L"+str(li),"C2651165",{1:suffix+"_L1",2:suffix+"_L2"},"1.5uH rail inductor",(1,un,8))
    c("C"+str(cbase),"100n",src,attach=(1,un,1))
    c("C"+str(cbase+1),"22u",src,attach=(1,un,10))
    c("C"+str(cbase+2),"22u",src,attach=(1,un,11))
    for j in range(3):c("C"+str(cbase+3+j),"22u",out,attach=(1,un,4) if j==0 else (1,"C"+str(cbase+2+j),1))
    if suffix=="HOST":
        r("R301","90.9k_0.1",out,"HOST_FB_MID","host feedback upper series part",(1,un,4))
        r("R302","1k_0.1","HOST_FB_MID",fb,"host feedback trim",(1,"R301",2))
        r("R303","10k_0.1",fb,"GND","host feedback lower",(1,un,3))
        rb=304
    else:
        r("R306","100k_0.1",out,fb,"modem feedback upper",(2,un,3))
        r("R307","14.7k_0.1",fb,"GND","modem feedback lower",(1,un,3))
        rb=308
    r("R"+str(rb),"1k","CTL_"+suffix+"_REG_EN",en,"regulator control series resistor",(2,un,12))
    r("R"+str(rb+1),"100k",en,"GND","regulator default off",(1,un,12))
    r("R"+str(310+(u-301)//3),"330k","3V0_AON",suffix+"_REG_PG","regulator power-good pullup",(2,un,14))

zone(3,"host-disconnect","HOST LOAD SWITCH / TRUE DISCONNECT", "U302")
add("U302","C471046",{1:"HOST_REG",2:"HOST_REG",3:"HOST_REG",4:"HOST_LOAD_EN",5:"GND",
    6:"HOST_CT",7:None,8:"HOST_SW",9:"HOST_SW",10:"HOST_SW",11:"GND"},
    "TPS22953 disconnect with SNS tied directly to OUT",nc={7:"PG unused; regulator PG and output ADC used."})
c("C308","1u","HOST_REG",attach=(1,"U302",1))
c("C309","10n","HOST_CT",attach=(1,"U302",6))
c("C310","1u","HOST_SW",attach=(1,"U302",9))
r("R312","1k","CTL_HOST_LOAD_EN","HOST_LOAD_EN","host load control series",(2,"U302",4))
r("R313","100k","HOST_LOAD_EN","GND","host load default off",(1,"U302",4))

zone(3,"host-reverse","HOST REVERSE BLOCK / USB BACKFEED", "U303")
add("U303","C2869734",{1:"HOST_SW",2:"GND",3:"HOST_5V",4:None,5:None,6:"HOST_5V"},
    "CE tied OUT for reverse blocking; not the disconnect device",
    nc={4:"Datasheet NC.",5:"Optional status output unused."})
c("C311","1u","HOST_5V",attach=(1,"U303",6))
r("R314","10k","HOST_5V","GND","residual output bleed; external load capacitance affects off time",(1,"U303",6))

zone(3,"modem-disconnect","TINY BAT SWITCH / 3.9V ONLY", "U305")
add("U305","C122837",{1:"MODEM_REG",2:"MODEM_REG",3:"MODEM_LOAD_EN",4:"MODEM_REG",5:"GND",
    6:"MODEM_CT",7:"TINY_BAT",8:"TINY_BAT",9:"GND"},"modem load switch with quick output discharge")
c("C318","1u","MODEM_REG",attach=(1,"U305",1))
c("C319","10n","MODEM_CT",attach=(1,"U305",6))
c("C301","470u6.3V","TINY_BAT",role="470uF low-ESR switched bulk; positive pin1",attach=(1,"U305",7))
c("C320","22u","TINY_BAT",attach=(1,"U305",7))
r("R315","1k","CTL_MODEM_LOAD_EN","MODEM_LOAD_EN","modem load control series",(2,"U305",3))
r("R316","100k","MODEM_LOAD_EN","GND","modem load default off",(1,"U305",3))

for ref,code,net in [("J301","C474881","HOST_5V"),("J302","C2908600","HOST_5V"),
                     ("J303","C492401","HOST_5V"),("J304","C474881","TINY_BAT"),
                     ("J305","C2908600","TINY_BAT"),("J306","C492401","TINY_BAT")]:
    if ref=="J301":zone(3,"host-connectors","HOST 5V / ALL PORTS TOTAL 1A", "J301")
    if ref=="J304":zone(3,"tiny-connectors","TINY BAT / ALL PORTS TOTAL 2A", "J304")
    add(ref,code,{1:net,2:"GND"},"parallel output: + pin1 / GND pin2; never another source")

zone(3,"modem-vio","SWITCHED 3.3V FOR TINY UART", "U306")
add("U306","C2887324",{1:"TINY_BAT",2:"GND",3:"TINY_BAT",4:None,5:"MODEM_VIO"},
    "UART level-converter supply follows actual modem output",nc={4:"Datasheet NC."})
c("C321","1u","TINY_BAT",attach=(1,"U306",1))
c("C322","1u","MODEM_VIO",attach=(1,"U306",5))

zone(3,"modem-uart","TINY UART / POWER-OFF ISOLATION", "U307")
add("U307","C5186957",{1:"TINY_TX",2:"GND",3:"HOST_3V3",4:"HOST_MODEM_RX",
    5:"HOST_MODEM_TX",6:"CTL_MODEM_LOAD_EN",7:"MODEM_VIO",8:"TINY_RX"},"TXU0202 overvoltage-tolerant inputs and Ioff")
c("C323","100n","HOST_3V3",attach=(1,"U307",3))
c("C324","100n","MODEM_VIO",attach=(1,"U307",7))
r("R317","100k","CTL_MODEM_LOAD_EN","GND","UART OE default off",(1,"U307",6))
add("J307","C7434310",{1:"HOST_3V3",2:"GND",3:"HOST_MODEM_TX",4:"HOST_MODEM_RX"},"host modem UART XH; 3V3 is reference input")
add("J308","C41425294",{1:"MODEM_VIO",2:"GND",3:"TINY_RX",4:"TINY_TX"},"Tiny UART header; pin1 is test/reference only, no Tiny VCC connection")

# PAGE 4 — controller, independent IMU, user inputs and management interface.
zone(4,"controller","STM32L031 / INDEPENDENT POWER CONTROL", "U401")
add("U401","C94085",{1:"3V0_AON",2:"HOST_REG_PG",3:"MODEM_CAR_ST",4:"AON_NRST",5:"3V0_AON",
    6:"ADC_BAT",7:"ADC_CAR",8:"AON_UART_TX",9:"AON_UART_RX",10:"ADC_HOST",11:"ADC_MODEM",
    12:"CTL_CAR_EN",13:"CTL_HOST_REG_EN",14:"CTL_MODEM_REG_EN",15:"CTL_HOST_LOAD_EN",
    16:"GND",17:"3V0_AON",18:"CTL_MODEM_LOAD_EN",19:"CTL_CHG_EN",20:"CHG_STAT_N",21:"HOST_CAR_ST",
    22:"ACC_N",23:"AON_SWDIO",24:"AON_SWCLK",25:"KEY_N",26:"EXT_WAKE_N",27:"IMU_INT",
    28:"MODEM_REG_PG",29:"AON_SCL",30:"AON_SDA",31:"BOOT0",32:"GND"},"STM32L031K6 LQFP32; internal clock")
c("C401","100n","3V0_AON",attach=(1,"U401",1))
c("C402","100n","3V0_AON",attach=(1,"U401",17))
c("C403","100n","3V0_AON",attach=(1,"U401",5))
c("C404","4.7u","3V0_AON",attach=(1,"C401",1))
r("R401","10k","BOOT0","GND","boot from flash",(1,"U401",31))
r("R402","10k","3V0_AON","AON_NRST","reset pullup",(2,"U401",4))
c("C405","100n","AON_NRST",attach=(1,"R402",2))
for num,net,pin in [(403,"CTL_CAR_EN",12),(404,"CTL_HOST_REG_EN",13),(405,"CTL_MODEM_REG_EN",14),
                    (406,"CTL_HOST_LOAD_EN",15),(407,"CTL_MODEM_LOAD_EN",18),(408,"CTL_CHG_EN",19)]:
    r("R"+str(num),"100k",net,"GND","MCU control default low",(1,"U401",pin))
for k,rail,adc,pin in [(409,"BAT_SW","ADC_BAT",6),(411,"CAR_5V","ADC_CAR",7),
                       (413,"HOST_SW","ADC_HOST",10),(415,"TINY_BAT","ADC_MODEM",11)]:
    r("R"+str(k),"200k",rail,adc,"ADC upper divider, ratio 1/3",(2,"U401",pin))
    r("R"+str(k+1),"100k",adc,"GND","ADC lower divider",(1,"R"+str(k),2))
    c("C"+str(406+(k-409)//2),"100n",adc,attach=(1,"R"+str(k+1),1))

zone(4,"motion-imu","LIS2DW12 / PARKING MOTION WAKE", "U402")
add("U402","C189624",{1:"AON_SCL",2:"3V0_AON",3:"GND",4:"AON_SDA",5:None,6:"GND",
    7:"GND",8:"GND",9:"3V0_AON",10:"3V0_AON",11:None,12:"IMU_INT"},
    "I2C address 0x18; RES tied ground per datasheet",
    nc={5:"Datasheet NC.",11:"Unused second interrupt output."})
c("C410","100n","3V0_AON",attach=(1,"U402",9))
c("C411","100n","3V0_AON",attach=(1,"U402",10))
c("C412","1u","3V0_AON",attach=(1,"U402",9))
r("R417","10k","3V0_AON","AON_SCL","I2C pullup",(2,"U402",1))
r("R418","10k","3V0_AON","AON_SDA","I2C pullup",(2,"U402",4))
r("R419","100k","IMU_INT","GND","interrupt default inactive; configure push-pull active high",(1,"U402",12))

zone(4,"user-key","POWER KEY / LOCAL AND EXTERNAL", "SW401")
add("SW401","C720477",{1:"KEY_EXT",2:"GND"},"momentary independent power key")
add("J403","C2908600",{1:"KEY_EXT",2:"GND"},"external enclosure momentary key",(1,"SW401",1))
r("R420","1k","KEY_EXT","KEY_N","key series filter resistor",(1,"SW401",1))
r("R421","100k","3V0_AON","KEY_N","key pullup",(2,"R420",2))
c("C413","10n","KEY_N",attach=(1,"R420",2))

zone(4,"external-wake","EXTERNAL OPEN-DRAIN WAKE / 10mA MAX", "J401")
add("J401","C2908601",{1:"3V0_AON",2:"GND",3:"WAKE_PORT_N"},"XH external wake connector")
add("J402","C2937625",{1:"3V0_AON",2:"GND",3:"WAKE_PORT_N"},"parallel 2.54mm wake connector")
add("J404","C474882",{1:"3V0_AON",2:"GND",3:"WAKE_PORT_N"},"parallel screw wake connector")
r("R422","1k","WAKE_PORT_N","EXT_WAKE_N","wake input series filter",(1,"J401",3))
r("R423","100k","3V0_AON","EXT_WAKE_N","wake pullup",(2,"R422",2))
c("C414","10n","EXT_WAKE_N",attach=(1,"R422",2))

zone(4,"management-uart","HOST MANAGEMENT UART / Ioff ISOLATION", "U403")
add("U403","C5186957",{1:"HOST_MGMT_TX",2:"GND",3:"3V0_AON",4:"AON_UART_RX",
    5:"AON_UART_TX",6:"CTL_HOST_LOAD_EN",7:"HOST_3V3",8:"HOST_MGMT_RX"},"power-management UART isolation")
c("C415","100n","3V0_AON",attach=(1,"U403",3))
c("C416","100n","HOST_3V3",attach=(1,"U403",7))
r("R424","100k","CTL_HOST_LOAD_EN","GND","management UART default isolation",(1,"U403",6))
add("J405","C7434310",{1:"HOST_3V3",2:"GND",3:"HOST_MGMT_TX",4:"HOST_MGMT_RX"},"host management XH; 3V3 is reference input")
add("J406","C41425294",{1:"HOST_3V3",2:"GND",3:"HOST_MGMT_TX",4:"HOST_MGMT_RX"},"parallel management header")

zone(4,"debug","AON SWD / VTREF IS SENSE ONLY", "J407")
add("J407","C41425294",{1:"3V0_AON",2:"GND",3:"DBG_SWDIO",4:"DBG_SWCLK"},"debugger must float signals when VTREF is absent")
r("R425","1k","DBG_SWDIO","AON_SWDIO","debug series protection; not powered-off isolation",(1,"J407",3))
r("R426","1k","DBG_SWCLK","AON_SWCLK","debug series protection; not powered-off isolation",(1,"J407",4))
zone(4,"debug-reset","OPEN-DRAIN DEBUG RESET", "J408")
add("J408","C492401",{1:"AON_NRST",2:"GND"},"optional debugger open-drain reset")


def generate():
    byref = {c["ref"]:c for c in components}
    for comp in components:
        if "attachment" in comp:
            a=comp["attachment"];owner=byref[a["owner"]]
            assert owner["zone"]==comp["zone"], comp["ref"]
            assert comp["pins"][a["pin"]] == owner["pins"][a["ownerPin"]], (comp["ref"],a)
    design=dict(schemaVersion=1, revision="R1-BAT", state="source-only",
                components=components,zones=zones,
                externalParts=[dict(lcsc="C394021",role="cell-attached 10k NTC",qty=1),
                               dict(role="protected 1S 3000mAh >=9A LiPo pack",qty=1,lcsc=None)],
                forbiddenConnections=["Tiny VIN to any power source while Tiny BAT connected",
                                      "USB VBUS bypass during controlled-off acceptance",
                                      "external power into any output terminal",
                                      "SWD probe drive with AON VTREF absent"])
    (ROOT/"source/design.json").write_text(json.dumps(design,ensure_ascii=False,indent=2)+"\n")
    used={c["lcsc"] for c in components}|{"C394021"}
    (ROOT/"source/selected-parts.json").write_text(json.dumps({k:PARTS[k] for k in sorted(used)},ensure_ascii=False,indent=2)+"\n")
    for page in PROJECT["pages"]:
        pc=[x for x in components if x["page"]==page["number"]]
        nets=sorted({v for x in pc for v in x["pins"].values() if v is not None})
        ir=dict(schemaVersion="1.4",projectId=PROJECT["projectUuid"],documentId=page["uuid"],
                components=[],nets=[dict(id=n,name=n,scope="global",role="ground" if n=="GND" else "signal") for n in nets],
                connections=[],modules=[])
        for x in pc:
            p=PARTS[x["lcsc"]]
            ir["components"].append(dict(id=x["id"],ref=x["ref"],role=x["role"],
                device=dict(libraryUuid=p["libraryUuid"],deviceUuid=p["deviceUuid"],name=p.get("edaName",p["mpn"])),
                pins=[dict(**pin,**({"noConnected":True} if x["pins"][pin["number"]] is None else {})) for pin in p["pins"]]))
            for pin,net in x["pins"].items():
                if net is not None:ir["connections"].append(dict(componentId=x["id"],pinNumber=pin,netId=net,kind="netlist"))
        for z in zones:
            if z["page"]==page["number"]:
                ir["modules"].append(dict(id=z["id"],name=z["title"],coreComponents=["cmp-"+z["core"]],
                    peripheralComponents=["cmp-"+v for v in z["members"] if v!=z["core"]]))
        (ROOT/"source"/f'p{page["number"]}-connectivity.json').write_text(json.dumps(ir,ensure_ascii=False,indent=2)+"\n")
    groups=defaultdict(list)
    for comp in components:groups[comp["lcsc"]].append(comp["ref"])
    with (ROOT/"bom.csv").open("w",newline="",encoding="utf-8-sig") as f:
        w=csv.writer(f,lineterminator="\n");w.writerow(["LCSC","Manufacturer","MPN","Value","Footprint","Quantity","References","Datasheet"])
        for cid,refs in sorted(groups.items()):
            p=PARTS[cid];w.writerow([cid,p["manufacturer"],p["mpn"],p["value"],p["footprint"],len(refs)," ".join(refs),p["datasheet"]])
    print(f'{len(components)} onboard parts / {len(groups)} part types / {len(zones)} functional zones')
    print(dict(Counter(x["page"] for x in components)))


if __name__=="__main__":generate()
