# VoltWatch Hardware & Electrical Interface Guide

This document details hardware selection, circuit design, voltage divider calculations, and electrical protection for the **VoltWatch** embedded monitoring platform.

---

## 1. Safety & Electrical Warning

> [!WARNING]
> The ESP32 ADC inputs are **strictly rated for 0V to 3.3V DC maximum**. Applying voltages exceeding 3.3V or negative voltages will permanently damage the microcontroller GPIO and potentially destroy the board. Always measure voltage divider output with a calibrated multimeter before connecting to the ESP32 pin.

---

## 2. Voltage Divider Circuit

When monitoring voltages greater than 3.3V, a precision two-resistor voltage divider is required:

```
    Monitored DC Rail (Vin)
             │
             ▼
           ┌───┐
           │R1 │ (Upper Resistor)
           └───┘
             │
             ├────────────► To ESP32 ADC Pin (GPIO 34)
             │      │
           ┌───┐   ─── C1 (100nF Ceramic Filter)
           │R2 │   ───
           └───┘    │
             │      │
             ▼      ▼
        System Ground (GND)
```

### Voltage Transfer Equation

$$V_{out} = V_{in} \cdot \left( \frac{R_2}{R_1 + R_2} \right)$$

$$V_{in(max)} = V_{ref} \cdot \left( \frac{R_1 + R_2}{R_2} \right)$$

Where:
- $V_{ref} = 3.3\text{V}$ (ESP32 ADC reference)
- Multiplier Ratio $= \frac{R_1 + R_2}{R_2}$

### Recommended Resistor Configurations

| Target Application | Nominal Voltage | Max Input ($V_{in}$) | Resistor R1 | Resistor R2 | Ratio | Max ADC Pin Voltage |
|:---|:---|:---|:---|:---|:---|:---|
| **5V USB / Logic Rail** | 5.0V | 6.6V | $10\text{ k}\Omega$ | $10\text{ k}\Omega$ | 2.00 : 1 | 3.30V @ 6.6V |
| **12V Battery / Auto (Default)** | 12.0V | 16.5V | $30\text{ k}\Omega$ | $7.5\text{ k}\Omega$ | 5.00 : 1 | 3.30V @ 16.5V |
| **24V Industrial Rail** | 24.0V | 33.0V | $90\text{ k}\Omega$ | $10\text{ k}\Omega$ | 10.00 : 1 | 3.30V @ 33.0V |
| **48V Telecom / Solar / E-Bike** | 48.0V | 66.0V | $190\text{ k}\Omega$ | $10\text{ k}\Omega$ | 20.00 : 1 | 3.30V @ 66.0V |

*Notes:*
- Use **1% metal film precision resistors** to minimize thermal drift and variance.
- Total impedance $(R_1 + R_2)$ should remain between $10\text{ k}\Omega$ and $100\text{ k}\Omega$ to balance low quiescent current draw with adequate ADC sampling capacitor charging speed.

---

## 3. ADC Protection Circuitry

To protect against inductive flyback, reverse polarity, and transient spikes:

1. **Clamping Schottky Diode (e.g., BAT54S or 1N5819)**:
   - Connect diode cathode to ESP32 3.3V rail and anode to ADC pin. If pin voltage exceeds $3.3\text{V} + 0.3\text{V}$, diode conducts, clamping excess voltage safely to the rail.
2. **Reverse Polarity Diode (1N4007 or Schottky)**:
   - Connect in series with $V_{in}$ to block accidental inverted connection.
3. **Decoupling Capacitor ($C_1 = 100\text{nF}$)**:
   - Place as close to the ESP32 pin and GND as possible. Forms a low-pass filter ($f_c \approx 1/(2\pi \cdot (R_1 \parallel R_2) \cdot C_1)$) eliminating high-frequency switching noise.

---

## 4. Default Pinout Mapping

| Subsystem / Component | ESP32-WROOM / DevKit Pin | ESP32-S3 Pin | Notes |
|:---|:---|:---|:---|
| **PZEM RX (Serial2 RX2)** | **GPIO 16** | GPIO 16 | Connected to TX of PZEM #1 & #2 |
| **PZEM TX (Serial2 TX2)** | **GPIO 17** | GPIO 17 | Connected to RX of PZEM #1 & #2 |
| **PZEM Logic VCC** | **VIN (5V Rail)** | 5V | Powers PZEM optocoupler LEDs |
| **Status LED** | **GPIO 2** (Onboard) | GPIO 2 | Active-high LED indicator |
| **Alarm Buzzer / Relay** | **GPIO 25** | GPIO 5 | Active buzzer or relay driver transistor |
| **Auxiliary DC ADC (Optional)** | **GPIO 34** (ADC1_CH6) | GPIO 4 (ADC1_CH3) | ADC1 channel for DC battery rail monitoring |
| **I2C SDA (Optional)** | **GPIO 21** | GPIO 8 | Optional OLED / INA219 sensor |
| **I2C SCL (Optional)** | **GPIO 22** | GPIO 9 | Optional OLED / INA219 sensor |

> [!IMPORTANT]
> The optical isolators on the PZEM-004T v3.0 board function reliably with 3.3V logic on ESP32 GPIO 16/17, but VCC requires 5V to drive the internal optocoupler LEDs. Connect PZEM VCC to ESP32 `VIN` (5V USB rail) and common GND.

---

## 5. Dual PZEM-004T v3.0 Metrology Bus Topology

Both PZEM-004T v3.0 modules communicate over TTL UART using Modbus-RTU over shared Hardware `Serial2`:

```
   +-------------------------------------------------------+
   |                   ESP32 Dev Module                    |
   |                                                       |
   |   GPIO 16 (RX2) <---+------------------+              |
   |   GPIO 17 (TX2) ----+--------+         |              |
   |   5V (VIN) ---------+---+    |         |              |
   |   GND --------------+---|----+----+    |              |
   +---------------------|---|----|----|----+--------------+
                         |   |    |    |    |
               +---------+   |    |    +----+---------+
               |             |    |                   |
        +------v------+      |    |            +------v------+
        | PZEM #1     |      |    +----------> | PZEM #2     |
        | (Grid)      |      |                 | (Inverter)  |
        | Addr: 0x01  | <----+                 | Addr: 0x02  |
        +-------------+                        +-------------+
        | AC In: Grid |                        | AC In: Inv  |
        | 100A CT     |                        | 100A CT     |
        +-------------+                        +-------------+
```

---

## 6. Bill of Materials (BOM)

| Item | Description | Quantity | Notes |
|:---|:---|:---|:---|
| 1 | ESP32-WROOM-32 Development Board | 1 | Standard 30-pin or 38-pin module |
| 2 | Peacefair PZEM-004T v3.0 Module + 100A CT | 2 | Dual AC metrology (Grid `0x01`, Inverter `0x02`) |
| 3 | Isolated 5V DC 2A Power Supply | 1 | Inverter-backed supply for uninterrupted operation |
| 4 | Resistor $30\text{ k}\Omega$ 1/4W 1% | 1 | Optional upper divider resistor ($R_1$) for DC rail |
| 5 | Resistor $7.5\text{ k}\Omega$ 1/4W 1% | 1 | Optional lower divider resistor ($R_2$) for DC rail |
| 6 | Ceramic Capacitor $100\text{nF}$ (0.1µF) 50V | 1 | Noise filter ($C_1$) |
| 7 | Active Buzzer 3.3V / 5V | 1 | Audio alarm indicator |
| 8 | 3mm / 5mm LED + $330\Omega$ resistor | 1 | Visual indicator (or use built-in LED) |
| 9 | BAT54S dual Schottky barrier diode | 1 | Over-voltage clamp protection |
| 10 | Jumper wires & terminal blocks | 1 set | Interconnect wiring |
