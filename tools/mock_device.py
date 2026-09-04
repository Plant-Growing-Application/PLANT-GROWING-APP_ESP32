#!/usr/bin/env python3
"""Sahte cihaz — arayuzu ESP32 olmadan calistirmak icin.

    python tools/mock_device.py            # http://127.0.0.1:8099
    python tools/mock_device.py --port 9000 --speed 120

NEDEN VAR: donanim elde olmadan arayuzu gelistirmek ve gostermek. Firmware'in
DONDURDUGU SEMAYI taklit eder; alan adlari birebir aynidir, boylece arayuzun
yanlis alan okudugu buradan gorulur.

BU BIR SIMULATORDUR, FIRMWARE DEGILDIR:
  · guvenlik zinciri BASITLESTIRILMISTIR (seviye + acil durum mandali)
  · aktuator kisitlarindan yalnizca minRun ve cooldown uygulanir
  · kural motoru esik/pencere/cevrim mantigini taklit eder ama
    `RuleEvaluator`'in kendisi degildir

Buradaki davranis firmware'in dogru oldugunun KANITI DEGILDIR; arayuzun
dogru ciziliyor olmasinin kanitidir.

ZAMAN HIZLANDIRMA: gercek bir sulama cevrimi 30 dakikadir; ekranda izlemek
icin sanal saat varsayilan olarak 60x hizli akar (`--speed`). Sensor
degisimleri de ayni carpani kullanir ki her sey tutarli gorunsun.
"""

import argparse
import base64
import hashlib
import json
import math
import pathlib
import random
import socket
import struct
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = pathlib.Path(__file__).resolve().parent.parent
FRONT = ROOT / "frontend"
JSDIR = FRONT / "js"

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


# ═══════════════════════════════════════════════════════════════════════════
# Varlıklar — `build_assets.py` ile AYNI sırada birleştirir
# ═══════════════════════════════════════════════════════════════════════════

def bundle_js() -> bytes:
    parts = sorted(JSDIR.glob("*.js"))
    if not parts:
        raise SystemExit(f"HATA: {JSDIR} altinda JS modulu yok")
    chunks = ["'use strict';"]
    for p in parts:
        chunks.append(f"/* ===== {p.name} ===== */\n" + p.read_text(encoding="utf-8"))
    return "\n".join(chunks).encode("utf-8")


# ═══════════════════════════════════════════════════════════════════════════
# Ürün kataloğu — src/core/CropProfile.cpp ile aynı değerler
# ═══════════════════════════════════════════════════════════════════════════

def rng(a, b):
    return {"min": a, "max": b}


def st(name, ph, ec, wt, at, hum, light, irr_on, irr_p, aer_on, aer_p, dur):
    return {"stage": name, "ph": rng(*ph), "ec": rng(*ec), "waterTemp": rng(*wt),
            "airTemp": rng(*at), "humidity": rng(*hum), "lightMinutes": light,
            "irrigationOnS": irr_on, "irrigationPeriodS": irr_p,
            "aerationOnS": aer_on, "aerationPeriodS": aer_p, "durationDays": dur}


CATALOG = {"crops": [
    {"key": "strawberry", "name": "Çilek", "difficulty": 2, "stageCount": 4, "stages": [
        st("seedling",   (5.5, 6.2), (0.8, 1.2), (18, 22), (18, 24), (65, 80), 600,  60, 1800,  900, 1800, 21),
        st("vegetative", (5.5, 6.2), (1.2, 1.4), (18, 22), (18, 24), (65, 80), 720, 120, 1800,  900, 1800, 30),
        st("flowering",  (5.5, 6.2), (1.6, 2.0), (18, 22), (18, 24), (60, 75), 840, 180, 1800, 1200, 1800, 21),
        st("fruiting",   (5.5, 6.2), (1.8, 2.2), (18, 22), (18, 24), (60, 75), 840, 180, 1200, 1200, 1800, 0)]},
    {"key": "tomato", "name": "Domates", "difficulty": 3, "stageCount": 4, "stages": [
        st("seedling",   (5.5, 6.3), (1.0, 1.5), (20, 24), (21, 27), (60, 75), 840,  90, 1800,  900, 1800, 21),
        st("vegetative", (5.5, 6.3), (1.8, 2.5), (20, 24), (21, 27), (60, 70), 960, 150, 1800, 1200, 1800, 30),
        st("flowering",  (5.5, 6.3), (2.2, 3.0), (20, 24), (21, 27), (60, 70), 960, 210, 1800, 1200, 1800, 25),
        st("fruiting",   (5.5, 6.3), (2.5, 3.5), (20, 24), (21, 27), (55, 70), 960, 240, 1200, 1200, 1800, 0)]},
    {"key": "pepper", "name": "Biber", "difficulty": 3, "stageCount": 4, "stages": [
        st("seedling",   (5.8, 6.3), (1.0, 1.4), (20, 25), (21, 28), (60, 75), 840,  90, 1800,  900, 1800, 25),
        st("vegetative", (5.8, 6.3), (1.8, 2.2), (20, 25), (21, 28), (60, 75), 840, 150, 1800, 1200, 1800, 35),
        st("flowering",  (5.8, 6.3), (2.0, 2.5), (20, 25), (21, 28), (60, 75), 960, 180, 1800, 1200, 1800, 25),
        st("fruiting",   (5.8, 6.3), (2.5, 3.0), (20, 25), (21, 28), (55, 70), 960, 240, 1500, 1200, 1800, 0)]},
    {"key": "cucumber", "name": "Salatalık", "difficulty": 2, "stageCount": 4, "stages": [
        st("seedling",   (5.5, 6.0), (1.0, 1.4), (20, 24), (22, 28), (65, 80), 720,  90, 1800,  900, 1800, 14),
        st("vegetative", (5.5, 6.0), (1.7, 2.0), (20, 24), (22, 28), (65, 80), 840, 150, 1800, 1200, 1800, 21),
        st("flowering",  (5.5, 6.0), (1.8, 2.2), (20, 24), (22, 28), (60, 75), 840, 210, 1500, 1200, 1800, 14),
        st("fruiting",   (5.5, 6.0), (2.0, 2.5), (20, 24), (22, 28), (60, 75), 900, 240, 1200, 1200, 1800, 0)]},
    {"key": "lettuce", "name": "Marul", "difficulty": 1, "stageCount": 2, "stages": [
        st("seedling",   (5.5, 6.5), (0.6, 0.9), (18, 22), (15, 22), (50, 70), 720, 120, 1800, 900, 1800, 14),
        st("vegetative", (5.5, 6.5), (0.8, 1.2), (18, 22), (15, 22), (50, 70), 840, 240, 1800, 900, 1800, 0)]},
    {"key": "basil", "name": "Fesleğen", "difficulty": 1, "stageCount": 2, "stages": [
        st("seedling",   (5.5, 6.0), (0.6, 1.0), (20, 24), (20, 27), (55, 70), 720, 120, 1800, 900, 1800, 18),
        st("vegetative", (5.5, 6.0), (1.0, 1.6), (20, 24), (20, 27), (55, 70), 840, 240, 1800, 900, 1800, 0)]},
]}

ACT_IDS = ["waterPump", "airPump", "growLight", "heater", "nutrientPump"]
SENSOR_IDS = ["level", "flow", "waterTemp", "ph", "ec", "airTemp", "humidity", "light"]

# Gecmis kaydinin slot sirasi — `services/HistoryStore.h::SLOT_ORDER` ile AYNI.
# Yazici ve okuyucunun ayri siralar kullanmasi ISSUE-034 idi; burada da tek
# tablo tutuyoruz ki mock ayni hatayi tekrarlamasin.
HIST_FIELDS = ["waterTemp", "flow", "ph", "ec", "level", "humidity", "airTemp", "light"]

# Hata kodları — src/core/ErrorCodes.h
ERR_LEVEL_INSUFFICIENT = 0x0601
ERR_EMERGENCY_LATCHED = 0x0605
ERR_MIN_RUNTIME = 0x0501
ERR_COOLDOWN = 0x0502


# ═══════════════════════════════════════════════════════════════════════════
# Cihaz durumu
# ═══════════════════════════════════════════════════════════════════════════

class Device:
    """Cihazın tüm durumu ve simülasyonu. Tek kilit altında."""

    def __init__(self, speed):
        self.lock = threading.RLock()
        self.speed = speed
        self.t0 = time.time()
        self.vt = 0.0                    # sanal saniye (hızlandırılmış)
        self.version = 1

        self.config = {
            "safety": {"flowVerifyDelayMs": 10000, "flowMinRate": 0.5,
                       "maxRuntimeGraceMs": 5000, "maxRuntimeViolations": 3,
                       "requireLevelSensor": True},
            "automation": {"mode": "manual", "manualOverrideMs": 900000},
            "system": {"timezone": "EET-2EEST,M3.5.0/3,M10.5.0/4",
                       "telemetryIntervalMs": 1000, "logLevel": 0},
            # Sensor bolumu (ISSUE-035): firmware ile ayni sema.
            "sensors": [
                {"id": "waterTemp", "enabled": True,  "offset": 0.0, "scale": 1.0,
                 "filterStrength": 4, "maxChangePerSec": 0.0,
                 "validRange": {"min": -1000.0, "max": 1000.0}},
                {"id": "flow",      "enabled": True,  "offset": 0.0, "scale": 1.0,
                 "filterStrength": 4, "maxChangePerSec": 0.0,
                 "validRange": {"min": -1000.0, "max": 1000.0}},
                {"id": "ph",        "enabled": False, "offset": 0.0, "scale": 1.0,
                 "filterStrength": 4, "maxChangePerSec": 0.0,
                 "validRange": {"min": -1000.0, "max": 1000.0}},
                {"id": "ec",        "enabled": False, "offset": 0.0, "scale": 1.0,
                 "filterStrength": 4, "maxChangePerSec": 0.0,
                 "validRange": {"min": -1000.0, "max": 1000.0}},
                {"id": "level",     "enabled": True,  "offset": 0.0, "scale": 1.0,
                 "filterStrength": 0, "maxChangePerSec": 0.0,
                 "validRange": {"min": -1000.0, "max": 1000.0}},
                {"id": "humidity",  "enabled": False, "offset": 0.0, "scale": 1.0,
                 "filterStrength": 4, "maxChangePerSec": 0.0,
                 "validRange": {"min": 0.0, "max": 100.0}},
                {"id": "airTemp",   "enabled": False, "offset": 0.0, "scale": 1.0,
                 "filterStrength": 4, "maxChangePerSec": 0.0,
                 "validRange": {"min": -40.0, "max": 85.0}},
                {"id": "light",     "enabled": False, "offset": 0.0, "scale": 1.0,
                 "filterStrength": 4, "maxChangePerSec": 0.0,
                 "validRange": {"min": 0.0, "max": 120000.0}},
            ],
            "actuators": [
                {"id": "waterPump",    "enabled": True,  "minRunMs": 5000,  "maxRunMs": 300000,   "cooldownMs": 30000},
                {"id": "airPump",      "enabled": True,  "minRunMs": 5000,  "maxRunMs": 300000,   "cooldownMs": 30000},
                {"id": "growLight",    "enabled": False, "minRunMs": 60000, "maxRunMs": 64800000, "cooldownMs": 60000},
                {"id": "heater",       "enabled": False, "minRunMs": 60000, "maxRunMs": 14400000, "cooldownMs": 180000},
                {"id": "nutrientPump", "enabled": False, "minRunMs": 2000,  "maxRunMs": 60000,    "cooldownMs": 600000},
            ],
        }

        self.crop = {"crop": "none", "stage": "seedling", "intensity": "normal",
                     "autoStage": True, "plantedAt": 0, "daysSincePlanting": 0,
                     "autoStageActive": False}
        self.rules = []

        # --- fiziksel benzetim ---
        self.level = 2                   # 0 kritik · 1 düşük · 2 yeterli
        self.tank_liters = 40.0
        self.water_temp = 17.5
        self.ph = 6.05
        self.ec = 1.35
        self.air_temp = 22.0
        self.humidity = 66.0
        self.light = 0.0

        self.act = {a: {"on": False, "since": 0.0, "off_at": -1e9, "run_ms": 0,
                        "cycles": 0, "block": 0, "source": 0}
                    for a in ACT_IDS}
        self.rule_state = {}             # kural indeksi -> histerezis tarafı

        self.latched = False
        self.latch_reason = 0
        self.events = [{"t": 1200, "code": 0x0201, "level": 1, "d": "ilk acilis"}]
        self.history = []

    # ── Güvenlik ───────────────────────────────────────────────────────────

    def permit(self, act_id):
        """Firmware'in `SafetyPermitFn`'inin basitleştirilmiş karşılığı."""
        if self.latched:
            return ERR_EMERGENCY_LATCHED
        # Su pompası VE ısıtıcı seviye kilidine tabidir (TASK-066 Karar 7).
        if act_id in ("waterPump", "heater") and self.level <= 1:
            return ERR_LEVEL_INSUFFICIENT
        return 0

    def cfg_of(self, act_id):
        return next(a for a in self.config["actuators"] if a["id"] == act_id)

    def set_actuator(self, act_id, on, source):
        """Kısıtları ve güvenliği uygulayarak röleyi sürer.

        @return (kabul_edildi, hata_kodu)
        """
        a = self.act[act_id]
        c = self.cfg_of(act_id)

        if on:
            rc = self.permit(act_id)
            if rc:
                a["block"] = rc
                return False, rc
            if not a["on"]:
                # cooldown
                if a["off_at"] > -1e8 and (self.vt * 1000 - a["off_at"]) < c["cooldownMs"]:
                    a["block"] = ERR_COOLDOWN
                    return False, ERR_COOLDOWN
                a["on"] = True
                a["since"] = self.vt
                a["cycles"] += 1
                a["source"] = source
                a["block"] = 0
            return True, 0

        if a["on"]:
            # minRun — güvenlik reddi bunu EZER (firmware ile aynı)
            if self.permit(act_id) == 0:
                if (self.vt - a["since"]) * 1000 < c["minRunMs"]:
                    a["block"] = ERR_MIN_RUNTIME
                    return False, ERR_MIN_RUNTIME
            a["on"] = False
            a["off_at"] = self.vt * 1000
            a["source"] = source
            a["block"] = 0
        return True, 0

    # ── Kural motoru (basitleştirilmiş) ────────────────────────────────────

    def eval_rules(self):
        if self.config["automation"]["mode"] != "auto":
            return

        minute_of_day = int((self.vt / 60) % 1440)
        wants = {}

        for i, r in enumerate(self.rules):
            if not r.get("enabled"):
                continue
            tgt = r.get("target")
            if tgt not in self.act:
                continue

            want = None
            kind = r.get("kind")

            if kind == "cycle":
                per = max(1, int(r.get("cyclePeriodS", 1)))
                want = (self.vt % per) < int(r.get("cycleOnS", 0))

            elif kind == "window":
                s, e = int(r.get("startMin", 0)), int(r.get("endMin", 0))
                want = (s <= minute_of_day < e) if s < e else (minute_of_day >= s or minute_of_day < e)

            elif kind == "threshold":
                v = self.sensor_value(r.get("sensor"))
                if v is None:
                    continue
                on_t, off_t = float(r.get("onThreshold", 0)), float(r.get("offThreshold", 0))
                cur = self.rule_state.get(i, False)
                if on_t < off_t:                       # altına düşünce AÇ
                    want = True if v <= on_t else (False if v >= off_t else cur)
                else:                                  # üstüne çıkınca AÇ
                    want = True if v >= on_t else (False if v <= off_t else cur)
                self.rule_state[i] = want

            if want is not None:
                wants[tgt] = wants.get(tgt, False) or want

        for tgt, want in wants.items():
            self.set_actuator(tgt, want, 1)   # source = AUTOMATION

    def sensor_value(self, sid):
        return {"level": float(self.level), "flow": self.flow(), "waterTemp": self.water_temp,
                "ph": self.ph, "ec": self.ec, "airTemp": self.air_temp,
                "humidity": self.humidity, "light": self.light}.get(sid)

    def flow(self):
        return round(2.35 + random.uniform(-0.12, 0.12), 2) if self.act["waterPump"]["on"] else 0.0

    # ── Fiziksel benzetim ──────────────────────────────────────────────────

    def tick(self, dt_real):
        dt = dt_real * self.speed          # sanal saniye
        self.vt += dt

        pump = self.act["waterPump"]["on"]
        heat = self.act["heater"]["on"]
        dose = self.act["nutrientPump"]["on"]
        lamp = self.act["growLight"]["on"]

        # Depo: pompa çalışırken tüketim (buharlaşma + bitki alımı)
        if pump:
            self.tank_liters -= 0.004 * dt
            self.act["waterPump"]["run_ms"] += int(dt * 1000)
        for a in ("airPump", "growLight", "heater", "nutrientPump"):
            if self.act[a]["on"]:
                self.act[a]["run_ms"] += int(dt * 1000)

        if self.tank_liters <= 0:
            # Simülasyon dursun diye kendi kendine doldurulur; gerçek cihazda
            # bunu kullanıcı yapar.
            self.tank_liters = 40.0
            self.log(0x0601, 1, "mock: depo otomatik dolduruldu")

        self.level = 2 if self.tank_liters > 16 else (1 if self.tank_liters > 6 else 0)

        # Sıcaklık: ortama doğru sürüklenir, ısıtıcı iterler
        self.water_temp += ((self.air_temp - 4.5) - self.water_temp) * 0.0009 * dt
        if heat:
            self.water_temp += 0.0032 * dt

        # EC: bitki alımı düşürür, dozaj yükseltir
        self.ec -= 0.00012 * dt
        if dose:
            self.ec += 0.0028 * dt
        self.ec = max(0.05, min(4.0, self.ec + random.uniform(-0.0015, 0.0015)))

        # pH: besin tüketimi yukarı iter, tampon çözelti geri çeker.
        # Geri çekme olmadan pH tek yönde kayar ve birkaç dakikada ölçek
        # sınırına yapışırdı — demoda "hep kırmızı" bir sensör.
        self.ph += (0.000035 * dt
                    + (6.15 - self.ph) * 0.00025 * dt
                    + random.uniform(-0.004, 0.004))
        self.ph = max(4.2, min(8.5, self.ph))

        # Ortam: gün içi salınım
        hour = (self.vt / 3600) % 24
        self.air_temp = 22.0 + 4.0 * math.sin((hour - 9) / 24 * 2 * math.pi) + random.uniform(-0.2, 0.2)
        self.humidity = 66.0 - 8.0 * math.sin((hour - 9) / 24 * 2 * math.pi) + random.uniform(-0.6, 0.6)

        daylight = max(0.0, 22000 * math.sin((hour - 6) / 12 * math.pi)) if 6 <= hour <= 18 else 0.0
        self.light = round(daylight + (14000 if lamp else 0) + random.uniform(-150, 150), 0)
        self.light = max(0.0, self.light)

        # Güvenlik: izin kalkınca ÇALIŞAN aktüatör derhal durur
        for aid in ACT_IDS:
            if self.act[aid]["on"]:
                rc = self.permit(aid)
                if rc:
                    self.act[aid]["on"] = False
                    self.act[aid]["off_at"] = self.vt * 1000
                    self.act[aid]["block"] = rc
                    self.log(rc, 2, f"mock: {aid} guvenlik nedeniyle durduruldu")
            else:
                # Engel kalktıysa rozet temizlensin
                if self.act[aid]["block"] in (ERR_LEVEL_INSUFFICIENT, ERR_EMERGENCY_LATCHED):
                    if self.permit(aid) == 0:
                        self.act[aid]["block"] = 0

        self.eval_rules()

        if self.crop["plantedAt"]:
            self.crop["daysSincePlanting"] = max(
                0, int((time.time() - self.crop["plantedAt"]) / 86400) + int(self.vt / 86400))
            self.crop["autoStageActive"] = bool(self.crop.get("autoStage"))

        self.version += 1

    def log(self, code, lvl, text):
        self.events.append({"t": int(self.vt * 1000), "code": code, "level": lvl, "d": text})
        if len(self.events) > 40:
            self.events.pop(0)

    # ── Yayınlanan durum ───────────────────────────────────────────────────

    def sensorEnabled(self, sid):
        s = next((x for x in self.config["sensors"] if x["id"] == sid), None)
        return bool(s and s["enabled"])

    def sensorEntry(self, sid, value):
        """Kapali sensor OKUNMAZ: firmware `NOT_PRESENT` yayinlar."""
        if not self.sensorEnabled(sid):
            return {"id": sid, "quality": "notPresent", "value": 0.0, "faults": 0}
        return {"id": sid, "quality": "ok", "value": value, "faults": 0}

    def state_json(self):
        interlocks = 0
        reason = 0
        if self.latched:
            interlocks |= 1
            reason = self.latch_reason or ERR_EMERGENCY_LATCHED
        if self.level <= 1:
            interlocks |= 2
            if not reason:
                reason = ERR_LEVEL_INSUFFICIENT

        return {
            "type": "state", "v": self.version,
            "system": {"mode": "emergency" if self.latched else "running",
                       "uptimeMs": int(self.vt * 1000), "freeHeap": 148000 + random.randint(-3000, 3000),
                       "minFreeHeap": 121000, "faults": 1 if interlocks else 0,
                       "faultMask": 0, "resetReason": 1},
            # Yayin sirasi firmware'in kayit tablosuyla ayni.
            "sensors": [
                self.sensorEntry("level",     float(self.level)),
                self.sensorEntry("flow",      self.flow()),
                self.sensorEntry("waterTemp", round(self.water_temp, 1)),
                self.sensorEntry("ph",        round(self.ph, 2)),
                self.sensorEntry("ec",        round(self.ec, 2)),
                self.sensorEntry("airTemp",   round(self.air_temp, 1)),
                self.sensorEntry("humidity",  round(self.humidity, 0)),
                self.sensorEntry("light",     self.light),
            ],
            "actuators": [
                {"id": a, "on": self.act[a]["on"], "source": self.act[a]["source"],
                 "block": self.act[a]["block"], "runMs": self.act[a]["run_ms"],
                 "cycles": self.act[a]["cycles"]} for a in ACT_IDS],
            "safety": {"interlocks": interlocks, "latched": self.latched, "reason": reason},
            "network": {"state": "connected", "ssid": "SahteSeraAgi", "rssi": -47,
                        "apActive": False, "apClients": 0, "retries": 0,
                        "lastError": 0, "ip": "192.168.1.42"},
            "time": {"valid": True, "epoch": int(time.time())},
            "crop": {"key": self.crop["crop"], "stage": self.crop["stage"],
                     "day": self.crop.get("daysSincePlanting", 0)},
            "automation": {"mode": 1 if self.config["automation"]["mode"] == "auto" else 0,
                           "schedulesPaused": False},
        }

    # ── Ürün profili ───────────────────────────────────────────────────────

    def profile(self, key):
        return next((c for c in CATALOG["crops"] if c["key"] == key), None)

    def build_plan(self, body, applied):
        key = body.get("crop", self.crop["crop"])
        stage = body.get("stage", self.crop["stage"])
        intensity = body.get("intensity", self.crop["intensity"])

        prof = self.profile(key)
        if prof is None:
            return None
        sdef = next((s for s in prof["stages"] if s["stage"] == stage), None)
        if sdef is None:
            return None

        factor = {"sparse": 0.7, "normal": 1.0, "abundant": 1.4}.get(intensity, 1.0)

        def scale(base, period):
            v = min(240, int(base * factor))
            return max(1, min(v, period - 10))

        en = {a["id"]: a["enabled"] for a in self.config["actuators"]}
        rules = []
        if en["waterPump"]:
            rules.append({"kind": "cycle", "target": "waterPump",
                          "cycleOnS": scale(sdef["irrigationOnS"], sdef["irrigationPeriodS"]),
                          "cyclePeriodS": sdef["irrigationPeriodS"]})
        if en["airPump"]:
            rules.append({"kind": "cycle", "target": "airPump",
                          "cycleOnS": min(240, sdef["aerationOnS"]),
                          "cyclePeriodS": sdef["aerationPeriodS"]})
        if en["growLight"] and 0 < sdef["lightMinutes"] < 1440:
            rules.append({"kind": "window", "target": "growLight",
                          "startMin": 360, "endMin": (360 + sdef["lightMinutes"]) % 1440})
        if en["heater"]:
            rules.append({"kind": "threshold", "target": "heater", "sensor": "waterTemp",
                          "onThreshold": sdef["waterTemp"]["min"],
                          "offThreshold": sdef["waterTemp"]["min"] + 1.5})
        if en["nutrientPump"]:
            rules.append({"kind": "threshold", "target": "nutrientPump", "sensor": "ec",
                          "onThreshold": sdef["ec"]["min"],
                          "offThreshold": sdef["ec"]["min"] + 0.2})

        return {"applied": applied, "stage": stage, "ruleCount": len(rules),
                "replacedCount": len([r for r in self.rules if r.get("kind") != "inactive"]),
                "automationMode": self.config["automation"]["mode"], "rules": rules}

    def crop_json(self):
        c = dict(self.crop)
        prof = self.profile(c["crop"])
        if prof is None and c["crop"] == "custom":
            # Bantlar bitkiyi anlatir, kurallari degil (TASK-068 Karar 4)
            prof = self.profile(c.get("derivedFrom", ""))
            c["targetsFromProfile"] = prof is not None
        if prof:
            c["name"] = prof["name"]
            c["stageCount"] = prof["stageCount"]
            c["difficulty"] = prof["difficulty"]
            sdef = next((s for s in prof["stages"] if s["stage"] == c["stage"]), prof["stages"][0])
            c["targets"] = sdef
        return c


DEV = None


# ═══════════════════════════════════════════════════════════════════════════
# WebSocket (RFC 6455 — yalnızca metin çerçevesi)
# ═══════════════════════════════════════════════════════════════════════════

CLIENTS = []
CLIENTS_LOCK = threading.Lock()


def ws_encode(payload: bytes) -> bytes:
    head = bytearray([0x81])
    n = len(payload)
    if n < 126:
        head.append(n)
    elif n < 65536:
        head.append(126)
        head += struct.pack(">H", n)
    else:
        head.append(127)
        head += struct.pack(">Q", n)
    return bytes(head) + payload


def ws_read_frame(rfile):
    """@return (opcode, payload) · bağlantı kapandıysa (None, None)"""
    hdr = rfile.read(2)
    if len(hdr) < 2:
        return None, None
    opcode = hdr[0] & 0x0F
    masked = bool(hdr[1] & 0x80)
    n = hdr[1] & 0x7F
    if n == 126:
        n = struct.unpack(">H", rfile.read(2))[0]
    elif n == 127:
        n = struct.unpack(">Q", rfile.read(8))[0]
    mask = rfile.read(4) if masked else b""
    data = rfile.read(n)
    if masked:
        data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
    return opcode, data


class WsClient:
    def __init__(self, sock):
        self.sock = sock
        self.lock = threading.Lock()
        self.alive = True

    def send(self, obj):
        try:
            with self.lock:
                self.sock.sendall(ws_encode(json.dumps(obj).encode("utf-8")))
        except OSError:
            self.alive = False


def broadcast(obj):
    with CLIENTS_LOCK:
        for c in list(CLIENTS):
            c.send(obj)
            if not c.alive:
                CLIENTS.remove(c)


def handle_ws_command(client, msg):
    """`WsProtocol.cpp` ile aynı sözleşme: cmd → ack → state."""
    with DEV.lock:
        req = msg.get("reqId")
        target = msg.get("target")
        action = msg.get("action")

        if action == "emergencyStop":
            DEV.latched = True
            DEV.latch_reason = ERR_EMERGENCY_LATCHED
            for a in ACT_IDS:
                DEV.act[a]["on"] = False
                DEV.act[a]["block"] = ERR_EMERGENCY_LATCHED
            DEV.log(ERR_EMERGENCY_LATCHED, 3, "ACIL DURDURMA (web)")
            client.send({"type": "ack", "reqId": req, "result": 0, "reason": 0})

        elif action == "emergencyClear":
            # Firmware `SafetyMonitor::acknowledge()` ile AYNI kural: yalnizca
            # CANLI su seviyesi kosulu engeller. Seviye "yeterli" degilse
            # (0 veya 1) temizleme reddedilir ve mandal DURUR.
            if DEV.level < 2:
                client.send({"type": "ack", "reqId": req, "result": 2,
                             "reason": ERR_LEVEL_INSUFFICIENT})
            else:
                DEV.latched = False
                DEV.latch_reason = 0
                for a in ACT_IDS:
                    DEV.act[a]["block"] = 0
                DEV.log(0, 0, "acil durum temizlendi")
                client.send({"type": "ack", "reqId": req, "result": 0, "reason": 0})

        elif action in ("on", "off") and target in DEV.act:
            ok, rc = DEV.set_actuator(target, action == "on", 2)   # source = MANUAL
            client.send({"type": "ack", "reqId": req,
                         "result": 0 if ok else 2, "reason": rc})
        else:
            client.send({"type": "ack", "reqId": req, "result": 3, "reason": 0x0902})

        client.send(DEV.state_json())


# ═══════════════════════════════════════════════════════════════════════════
# HTTP + WS işleyici
# ═══════════════════════════════════════════════════════════════════════════

class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    # -- yardımcılar --------------------------------------------------------

    def _send(self, code, body, ctype="application/json"):
        raw = body if isinstance(body, bytes) else json.dumps(body).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype + "; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def _body(self):
        n = int(self.headers.get("Content-Length") or 0)
        if not n:
            return {}
        try:
            return json.loads(self.rfile.read(n).decode("utf-8"))
        except Exception:
            return {}

    def _err(self, code, field=None, status=400):
        self._send(status, {"error": {"code": code, "message": "hata", "field": field}})

    # -- WebSocket ----------------------------------------------------------

    def _try_websocket(self):
        if self.headers.get("Upgrade", "").lower() != "websocket":
            return False

        key = self.headers.get("Sec-WebSocket-Key", "")
        accept = base64.b64encode(
            hashlib.sha1((key + WS_GUID).encode()).digest()).decode()

        self.wfile.write(
            ("HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\nConnection: Upgrade\r\n"
             f"Sec-WebSocket-Accept: {accept}\r\n\r\n").encode())
        self.wfile.flush()

        client = WsClient(self.connection)
        with CLIENTS_LOCK:
            CLIENTS.append(client)
        with DEV.lock:
            client.send(DEV.state_json())

        try:
            while client.alive:
                op, data = ws_read_frame(self.rfile)
                if op is None or op == 0x8:
                    break
                if op == 0x9:                       # ping → pong
                    with client.lock:
                        self.connection.sendall(bytes([0x8A, len(data)]) + data)
                    continue
                if op != 0x1:
                    continue
                try:
                    msg = json.loads(data.decode("utf-8"))
                except Exception:
                    continue
                if msg.get("type") == "cmd":
                    handle_ws_command(client, msg)
        except OSError:
            pass
        finally:
            client.alive = False
            with CLIENTS_LOCK:
                if client in CLIENTS:
                    CLIENTS.remove(client)
        return True

    # -- GET ----------------------------------------------------------------

    def do_GET(self):
        path = self.path.split("?")[0]

        if path == "/ws":
            if self._try_websocket():
                return
            return self._err(0x0902, status=400)

        if path in ("/", "/index.html"):
            return self._send(200, (FRONT / "index.html").read_bytes(), "text/html")
        if path == "/app.css":
            return self._send(200, (FRONT / "app.css").read_bytes(), "text/css")
        if path == "/app.js":
            return self._send(200, bundle_js(), "application/javascript")

        if path == "/api/auth/status":
            return self._send(200, {"setupMode": False})

        with DEV.lock:
            if path == "/api/state":
                return self._send(200, DEV.state_json())
            if path == "/api/crops":
                return self._send(200, CATALOG)
            if path == "/api/crop":
                return self._send(200, DEV.crop_json())
            if path == "/api/config":
                return self._send(200, DEV.config)
            if path == "/api/config/rules":
                return self._send(200, {"rules": DEV.rules})
            if path == "/api/diagnostics":
                return self._send(200, {
                    "active": [ERR_LEVEL_INSUFFICIENT] if DEV.level <= 1 else [],
                    "tasks": [
                        {"name": "app_core", "maxLoopUs": 4200, "minStack": 2100, "overruns": 0},
                        {"name": "io_sense", "maxLoopUs": 3100, "minStack": 2600, "overruns": 0},
                        {"name": "net", "maxLoopUs": 9100, "minStack": 3300, "overruns": 1},
                        {"name": "ui", "maxLoopUs": 26000, "minStack": 1900, "overruns": 0},
                        {"name": "store", "maxLoopUs": 800, "minStack": 2400, "overruns": 0}],
                    "events": DEV.events})
            if path == "/api/history":
                # Alan sirasi firmware'in `SLOT_ORDER`'i ile AYNI (ISSUE-034).
                return self._send(200, {
                    "count": len(DEV.history), "stored": len(DEV.history),
                    "fields": HIST_FIELDS,
                    "rows": DEV.history})
            if path == "/api/network/scan":
                return self._send(200, {"status": "done", "truncated": False, "networks": [
                    {"ssid": "SahteSeraAgi", "rssi": -47, "channel": 6, "open": False},
                    {"ssid": "Komsu_2G", "rssi": -71, "channel": 11, "open": False},
                    {"ssid": "MisafirAg", "rssi": -83, "channel": 1, "open": True}]})

        return self._err(0x0902, status=404)

    # -- POST ---------------------------------------------------------------

    def do_POST(self):
        path = self.path.split("?")[0]
        body = self._body()

        if path == "/api/auth/login":
            return self._send(200, {"token": "sahte-oturum"})
        if path in ("/api/auth/logout", "/api/setup/password", "/api/auth/password"):
            return self._send(200, {"ok": True})
        if path == "/api/network/scan":
            return self._send(200, {"status": "running", "truncated": False, "networks": []})

        with DEV.lock:
            if path == "/api/crop/preview":
                plan = DEV.build_plan(body, False)
                if plan is None:
                    return self._err(0x0902, "crop")
                return self._send(200, plan)

            if path == "/api/system/factory-reset":
                DEV.crop = {"crop": "none", "stage": "seedling", "intensity": "normal",
                            "autoStage": True, "plantedAt": 0, "daysSincePlanting": 0,
                            "autoStageActive": False}
                DEV.rules = []
                DEV.config["automation"]["mode"] = "manual"
                for a in DEV.config["actuators"][2:]:
                    a["enabled"] = False
                return self._send(200, {"ok": True})

        return self._send(200, {"ok": True})

    # -- PUT ----------------------------------------------------------------

    def do_PUT(self):
        path = self.path.split("?")[0]
        body = self._body()

        with DEV.lock:
            if path == "/api/crop":
                plan = DEV.build_plan(body, True)
                if plan is None:
                    return self._err(0x0902, "crop")
                for k in ("crop", "stage", "intensity", "plantedAt", "autoStage"):
                    if k in body:
                        DEV.crop[k] = body[k]
                DEV.crop["derivedFrom"] = DEV.crop["crop"]
                DEV.rules = [dict(r, enabled=True, priority=10, minTriggerIntervalS=0)
                             for r in plan["rules"]]
                DEV.rule_state.clear()
                DEV.log(0, 0, f"urun profili uygulandi: {DEV.crop['crop']}")
                return self._send(200, plan)

            if path == "/api/config/sensors":
                i = body.get("index", -1)
                if not (0 <= i < len(DEV.config["sensors"])):
                    return self._err(0x0902, "index")

                s = DEV.config["sensors"][i]

                # Firmware ile ayni alanlar arasi kural: guvenlik kilidi
                # acikken seviye sensoru kapatilamaz.
                if (s["id"] == "level" and body.get("enabled") is False
                        and DEV.config["safety"]["requireLevelSensor"]):
                    return self._err(0x0204, "sensors.waterLevel.enabled")

                if body.get("scale") == 0:
                    return self._err(0x0204, "sensors.scale")

                vr = body.get("validRange")
                if vr is not None and not (vr.get("min", 0) < vr.get("max", 0)):
                    return self._err(0x0204, "sensors.validRange")

                s.update({k: v for k, v in body.items() if k != "index"})
                DEV.log(0, 0, f"sensor guncellendi: {s['id']}")
                return self._send(200, {"ok": True})

            if path == "/api/config/actuators":
                i = body.get("index", -1)
                if not (0 <= i < len(DEV.config["actuators"])):
                    return self._err(0x0902, "index")
                DEV.config["actuators"][i].update({k: v for k, v in body.items() if k != "index"})
                return self._send(200, {"ok": True})

            if path == "/api/config/automation":
                DEV.config["automation"].update(body)
                DEV.log(0, 1, f"otomasyon modu: {DEV.config['automation']['mode']}")
                return self._send(200, {"ok": True})

            if path == "/api/config/safety":
                DEV.config["safety"].update(body)
                return self._send(200, {"ok": True})

            if path == "/api/config/system":
                DEV.config["system"].update(body)
                return self._send(200, {"ok": True})

            if path == "/api/config/rules":
                DEV.rules = body.get("rules", [])
                DEV.rule_state.clear()
                if DEV.crop["crop"] not in ("none", "custom"):
                    DEV.crop["derivedFrom"] = DEV.crop["crop"]
                    DEV.crop["crop"] = "custom"
                    DEV.crop["autoStage"] = False
                return self._send(200, {"ok": True})

        return self._send(200, {"ok": True})


# ═══════════════════════════════════════════════════════════════════════════
# Simülasyon döngüsü
# ═══════════════════════════════════════════════════════════════════════════

def sim_loop():
    last = time.time()
    hist_at = 0.0
    while True:
        time.sleep(0.5)
        now = time.time()
        dt, last = now - last, now

        with DEV.lock:
            DEV.tick(dt)
            state = DEV.state_json()
            if DEV.vt - hist_at > 600:          # sanal 10 dakikada bir kayıt
                hist_at = DEV.vt
                sample = {"waterTemp": round(DEV.water_temp, 1), "flow": DEV.flow(),
                          "ph": round(DEV.ph, 2), "ec": round(DEV.ec, 2),
                          "level": float(DEV.level), "humidity": round(DEV.humidity, 0),
                          "airTemp": round(DEV.air_temp, 1), "light": DEV.light}
                DEV.history.append({"t": int(DEV.vt),
                                    "v": [sample[f] for f in HIST_FIELDS]})
                if len(DEV.history) > 120:
                    DEV.history.pop(0)

        broadcast(state)


def main():
    global DEV
    ap = argparse.ArgumentParser(description="SALIXUS sahte cihaz sunucusu")
    ap.add_argument("--port", type=int, default=8099)
    ap.add_argument("--speed", type=float, default=60.0,
                    help="sanal saat carpani (varsayilan 60 = 30 dk cevrim 30 sn'de gorunur)")
    args = ap.parse_args()

    DEV = Device(args.speed)
    threading.Thread(target=sim_loop, daemon=True).start()

    srv = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)

    # NOT: konsol ciktisinda YALNIZCA ASCII kullaniyoruz. Windows konsolu
    # cp1254 ile kodluyor ve "->" yerine ok karakteri yazmak sunucuyu
    # UnicodeEncodeError ile dusuruyordu.
    print("")
    print(f"  SALIXUS sahte cihaz  ->  http://127.0.0.1:{args.port}")
    print(f"  Parola: herhangi bir sey  |  Sanal saat: {args.speed:.0f}x")
    print("  Ctrl+C ile durdurun")
    print("")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\n  durduruldu")


if __name__ == "__main__":
    main()
