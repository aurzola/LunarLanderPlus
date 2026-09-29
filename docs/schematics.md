# Lunar Lander ESP32 — Esquemáticos ASCII

Diagramas de conexiones en texto plano de las **tres placas** del proyecto:

1. **ESP32-S3** (placa principal, `esp32LanderS3/`) — video compuesto LCD_CAM+GDMA.
2. **Board VGA** (segundo ESP32 WROOM-32, `esp32LanderVGA/`).
3. **ESP32 clásico composite** (`esp32LanderComposite/`) — **DESCONTINUADO**, solo referencia histórica.

Fuente de verdad de valores: `docs/hardware.md` y AGENTS.md (sección "Hardware eléctrico / pinado").
Donde el driver no documenta el circuito (red de resistencias del video S3), se marca `[ver video_s3.cpp]`.

---

## 1. ESP32-S3 — placa principal (CRT B/N)

### 1.1 Diagrama general

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                            ESP32-S3 DevKit                                   │
│                      VERSIÓN PRINCIPAL · CRT B/N NTSC                        │
│                 sketch: esp32LanderS3/ · FQBN PSRAM=opi                      │
└──────────────────────────────────────────────────────────────────────────────┘

  ALIMENTACIÓN (común a todo):
     5V   ── (USB o fuente externa; el nunchuck y la lógica van a 3V3)
     3V3  ──► raíl 3V3  ──► nunchuck (VCC) · pot (extremo 1) · DAC del video
     GND  ──► raíl GND  ──► nunchuck · pot · start · RCA · cap de audio

                                  ┌───────────────────┐
                        ┌─────────┤  ESP32-S3  (chip) ├──────────┐
                        │         └───────────────────┘          │
                        │                                        │
   ┌──────────────┐     │  I2C 50 kHz, pull-ups internos         │
   │ Nunchuck Wii │     │                                        │
   │              │     │                                        │
   │  SDA  ───────┼─────┼──── GPIO21 ──► (I2C SDA)               │
   │  SCL  ───────┼─────┼──── GPIO9  ──► (I2C SCL)               │
   │  +3.3V ──────┼──► 3V3                                     │
   │  GND  ───────┼──► GND                                     │
   └──────────────┘     │                                        │
                        │                                        │
   ┌──────────────┐     │  ADC                                 │
   │ Pot 10kΩ     │     │                                        │
   │  extremo 1 ──┼──► 3V3                                     │
   │  cursor ─────┼──── GPIO8 (ADC1)                            │
   │  extremo 2 ──┼──► GND                                     │
   └──────────────┘     │   (0.1µF opcional cursor→GND)         │
                        │                                        │
   ┌──────────────┐     │  INPUT_PULLUP                         │
   │ Botón start  │     │                                        │
   │  ┌─┐  ───────┼──── GPIO13 ──► (flanco → startPressed)      │
   │  └─┘  │       │     │                                        │
   │      GND ────┼──► GND                                     │
   └──────────────┘     │                                        │
                        │                                        │
   ┌──────────────┐     │  LCD_CAM (bus de datos, D0–D7)        │
   │ DAC resistivo│     │                                        │
   │  D0 ◄────────┼──── GPIO4                                   │
   │  D1 ◄────────┼──── GPIO5                                   │
   │  D2 ◄────────┼──── GPIO6                                   │
   │  D3 ◄────────┼──── GPIO7                                   │
   │  D4 ◄────────┼──── GPIO15                                  │
   │  D5 ◄────────┼──── GPIO16                                  │
   │  D6 ◄────────┼──── GPIO40                                  │
   │  D7 ◄────────┼──── GPIO41                                  │
   │   ▼           │     │                                        │
   │  RCA amarillo │     │                                        │
   └──────────────┘     │                                        │
                        │  LEDC PWM 312.5 kHz @ 8-bit (APB)     │
   ┌──────────────┐     │                                        │
   │ Amp / RCA    │     │                                        │
   │  blanco ◄────┼──── GPIO18 ──► cap 1–10µF en serie          │
   └──────────────┘     │                                        │
                        └────────────────────────────────────────┘
```

### 1.2 Nunchuck Wii → I2C

```
                  ESP32-S3                Nunchuck Wii (conector 6P)
               ┌──────────────┐          ┌───────────────────┐
   GPIO21 ─────┤ SDA          │◄─────────┤ SDA (pin 2)       │
   GPIO9  ─────┤ SCL          │◄─────────┤ SCL (pin 3)       │
   3V3    ─────┤ 3V3          │──────────┤ +3.3V (pin 6)     │
   GND    ─────┤ GND          │──────────┤ GND (pin 5)       │
               └──────────────┘          └───────────────────┘
                                          (pines 1 y 4: sin conectar)

   - I2C a 50 kHz, dirección 0x52.
   - Pull-ups INTERNOS vía gpio_set_pull_mode(GPIO_PULLUP_ONLY)
     (el core de Arduino no los activa solo; ver src/nunchuck.cpp).
   - Nunchuck sin cifrado en este proyecto (ENCRYPTED:0), se lee en crudo.
```

### 1.3 Potenciómetro 10kΩ → GPIO8 (nivel de potencia)

```
                 ESP32-S3
              ┌──────────────┐
   3V3  ──┬───┤ (raíl 3V3)   │
          │   │              │
     [pot 10kΩ]              │
          │   │              │
      cursor ─┼──┬───► GPIO8 (ADC1)
          │   │  │           │
          │   │ [0.1µF]      │   ← opcional: paso bajo para ruido
          │   │  │           │
   3V3  ──┴───┴──┴───► GND   │
              └──────────────┘

   - Pin 1 (extremo) → 3V3 · pin 2 (extremo) → GND · pin 3 (cursor) → ADC.
   - Si el sentido sale invertido: se invierte en software o se cambian los extremos.
   - NOTA: el pot está cableado pero DESHABILITADO en juego (POT_DISABLED=1):
     la potencia se fija solo con C+stick del nunchuck. El GPIO8 no se lee.
```

### 1.4 Botón start → GPIO13

```
                 ESP32-S3
              ┌──────────────┐
   GPIO13 ─────┤ (INPUT_PULLUP)│
              │              │
     ┌─┐      │              │
     └─┘ │    │              │
         │    │              │
        GND ──┴──► GND       │
              └──────────────┘

   - INPUT_PULLUP interno: reposo = HIGH, pulsado = LOW.
   - Flanco detectado en el .ino (debounce) → game.startPressed.
```

### 1.5 Video compuesto — bus LCD_CAM → DAC resistivo → RCA amarillo

El driver `video_s3.cpp` conecta el periférico LCD_CAM a 8 GPIO en paralelo
(`DATA_PINS[8] = {4,5,6,7,15,16,40,41}` → `LCD_DATA_OUT0..7`) y genera NTSC 320×240 B/N.
La **red de resistencias** que convierte el bus de 8 bits en analógico **no está
documentada en el código** → `[ver video_s3.cpp]`.

```
             ESP32-S3                       DAC resistivo
          ┌──────────────┐                ┌───────────────┐
 GPIO4  ───┤ D0          │◄───────────────┤ D0            │
 GPIO5  ───┤ D1          │◄───────────────┤ D1            │
 GPIO6  ───┤ D2          │◄───────────────┤ D2            │
 GPIO7  ───┤ D3          │◄───────────────┤ D3            │
 GPIO15 ───┤ D4          │◄───────────────┤ D4            │
 GPIO16 ───┤ D5          │◄───────────────┤ D5            │
 GPIO40 ───┤ D6          │◄───────────────┤ D6            │
 GPIO41 ───┤ D7          │◄───────────────┤ D7            │
└──────────────┘                │    │          │
                                           │    ▼          │
                                           │  RCA (vídeo)  │
                                           └───────┬───────┘
                                                   │
                                           ┌───────▼───────┐
                                           │  RCA AMARILLO │
                                           │   del CRT B/N │
                                           └───────────────┘

   [ver video_s3.cpp] — topología típica para un bus paralelo de 8 bits:
   red R-2R o resistencias ponderadas (bit 7 = MSB) + terminación 75Ω
   hacia el RCA. Verificar los valores reales en el esquema físico de la
   placa / protoboard; el driver solo maneja los registros LCD_CAM y GDMA.
```

### 1.6 Audio → GPIO18 → cap → amp / RCA blanco

```
             ESP32-S3
          ┌──────────────┐
 GPIO18 ───┤ LEDC ch0    │
          │  (312.5 kHz  │
          │   @ 8-bit,   │
          │   APB clock) │
          └──────┬───────┘
                 │
                [1–10µF]      ← condensador de acople en serie
                 │              (bloquea DC ~1.65V del PWM)
                 │
          ┌──────▼────────┐
          │ Amp externo / │
          │ RCA BLANCO    │
          │ del TV/CRT    │
          └───────────────┘

   - Requiere ledcSetClockSource(LEDC_USE_APB_CLK) antes de attach (quirks S3).
   - El duty se adopta solo pulsando duty_start + low_speed_update (ver audio.cpp).
```

---

## 2. Board VGA (segundo ESP32 WROOM-32, `esp32LanderVGA/`)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                       ESP32 Dev Module (WROOM-32)                            │
│                              BOARD VGA · SVGA 640x480@60 mono                │
│                    sketch: esp32LanderVGA/ · no_ota                          │
└──────────────────────────────────────────────────────────────────────────────┘

                                     ┌───────────────────┐
                           ┌─────────┤  ESP32  (chip)    ├─────────┐
                           │         └───────────────────┘         │
                           │                                       │
   ┌──────────────┐        │                                       │
   │ Nunchuck Wii │        │  I2C 50 kHz, pull-ups internos        │
   │  SDA ────────┼────────┼── GPIO21                              │
   │  SCL ────────┼────────┼── GPIO22                              │
   │  +3.3V ──────┼──► 3V3                                       │
   │  GND  ───────┼──► GND                                       │
   └──────────────┘        │                                       │
                           │  ADC (solo entrada, sin pull)         │
   ┌──────────────┐        │                                       │
   │ Pot 10kΩ     │        │                                       │
   │  cursor ─────┼────────┼── GPIO34                              │
   │  (3V3/GND)   │        │                                       │
   └──────────────┘        │                                       │
                           │  INPUT_PULLUP                         │
   ┌──────────────┐        │                                       │
   │ Botón start  │        │                                       │
   │  GPIO13 ─────┼────────┼── GPIO13                              │
   │  GND ────────┼──► GND                                       │
   └──────────────┘        │                                       │
                           │  DAC1 (analógico, 0–3.3V)             │
   GPIO25 ─────────────────┼──► divisor 270Ω×3 ──► VGA R/G/B      │
                           │                                       │
                           │  GPIO digital (TTL directo)           │
   GPIO32 ─────────────────┼──► VGA HSYNC (pin 13)                 │
   GPIO33 ─────────────────┼──► VGA VSYNC (pin 14)                 │
                           │                                       │
                           │  LEDC PWM (igual que composite)       │
   GPIO26 ─────────────────┼──► cap 1–10µF ──► amp / RCA blanco    │
                           └───────────────────────────────────────┘
```

### 2.1 Conector VGA (DE-15) — circuito completo

```
               ESP32 (WROOM-32)                        Conector DE-15
               ┌──────────────┐                       ┌──────────────┐
 GPIO25 ───────┤ DAC1         ├──[270Ω]──┬───────────►│  1  R        │
               │              ├──[270Ω]──┤───────────►│  2  G        │
               │              ├──[270Ω]──┴───────────►│  3  B        │
               │              │                        │              │
 GPIO32 ───────┤ HSYNC        ├───────────────────────►│ 13  HSYNC    │
 GPIO33 ───────┤ VSYNC        ├───────────────────────►│ 14  VSYNC    │
               │              │                        │              │
 GND ───────────┤ GND          ├───────────────────────►│  5,6,7,8,10  │
               └──────────────┘                        │   (GND)      │
                                                       │  9  (+5V)  ✗ NO conectar
                                                       │  4,11,12,15  │ (DDC) ✗
                                                       └──────────────┘

   - Las 3 resistencias DEBEN ser iguales (270Ω) → divisor con los 75Ω de
     terminación interna del monitor: V_blanco = 3.3·75/(270+75) = 0.717 V
     (spec VGA 0.7 V); V_negro = 0 V; ~9.6 mA por rama (~29 mA total).
   - NO usar el circuito 100/100/220 de bitluni (es para 2 DACs; en mono
     recorta a 1.414 V y desbalancea).
   - Alternativas: 280Ω (0.697 V) o 330Ω (0.611 V, margen de corriente).
   - Sync directo a 13/14: TTL, sin terminación.
   - GPIO32/33 son XTAL_32K_P/N en algunos DevKit: verificar que la placa
     no tenga el cristal de 32 kHz montado.
   - NO conectar el pin 9 (+5V) ni los DDC (4/11/12/15).
   - init(VGAMode::MODE320x240, PIN_HSYNC, PIN_VSYNC, PIN_DAC, voltageDivider=true)
```

---

## 3. Placa composite clásica (`esp32LanderComposite/`) — DESCONTINUADA

> **DESCONTINUADO (24/8/2026)**: reemplazada por la ESP32-S3. Se conserva como
> archivo histórico; ya no recibe sync ni uploads. Este esquemático es solo
> referencia.

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                       ESP32 Dev Module (WROOM-32)                            │
│                     COMPOSITE CLÁSICO · DESCONTINUADO                        │
│                    sketch: esp32LanderComposite/ · no_ota                    │
└──────────────────────────────────────────────────────────────────────────────┘

                                     ┌───────────────────┐
                           ┌─────────┤  ESP32  (chip)    ├─────────┐
                           │         └───────────────────┘         │
                           │                                       │
   ┌──────────────┐        │  I2C 50 kHz, pull-ups internos        │
   │ Nunchuck Wii │        │                                       │
   │  SDA ────────┼────────┼── GPIO21                              │
   │  SCL ────────┼────────┼── GPIO22                              │
   │  +3.3V ──────┼──► 3V3                                       │
   │  GND  ───────┼──► GND                                       │
   └──────────────┘        │                                       │
                           │  ADC (solo entrada, sin pull)         │
   ┌──────────────┐        │                                       │
   │ Pot 10kΩ     │        │                                       │
   │  cursor ─────┼────────┼── GPIO34                              │
   └──────────────┘        │                                       │
                           │  ADC (LEGACY, desconectado)           │
   ┌──────────────┐        │                                       │
   │ Gatillo      │        │                                       │
   │ reóstato ────┼────────┼── GPIO35  ✗ NO CONECTADO (histórico)  │
   │ (divisor 120Ω│        │     (ver "Medición del reóstato" en   │
   │  2.5–2.9V)   │        │      docs/hardware.md)                │
   └──────────────┘        │                                       │
                           │  INPUT_PULLUP                         │
   ┌──────────────┐        │                                       │
   │ Botón start  │        │                                       │
   │  GPIO13 ─────┼────────┼── GPIO13                              │
   │  GND ────────┼──► GND                                       │
   └──────────────┘        │                                       │
                           │  DAC interno (aquaticus)              │
   GPIO25 ─────────────────┼──► DAC1 ──► RCA (vídeo, luma 255)     │
                           │                                       │
                           │  LEDC PWM + timer ISR                 │
   GPIO26 ─────────────────┼──► cap 1–10µF ──► RCA blanco          │
                           └───────────────────────────────────────┘

   - Video: librería aquaticus NTSC_320x240, FB_FORMAT_GREY_8BPP, DAC GPIO25.
   - Audio: GPIO26 (dac_output_disable(DAC_CHANNEL_2) para liberar el pad).
   - GPIO34/35 son solo-entrada (ideales para ADC).
```

---

## 4. Tabla de referencia cruzada por placa

### 4.1 ESP32-S3 (principal) — `esp32LanderS3/`

| GPIO | Señal | Destino físico |
|------|-------|----------------|
| GPIO21 | I2C SDA nunchuck | Nunchuck pin SDA (pull-up interno, 50 kHz, dir 0x52) |
| GPIO9 | I2C SCL nunchuck | Nunchuck pin SCL (pull-up interno) |
| GPIO8 | Pot 10kΩ (cursor) | Divisor 3V3–pot–GND; **deshabilitado en juego** (POT_DISABLED=1) |
| GPIO13 | Botón start | Pulsador → GND (INPUT_PULLUP) |
| GPIO4 | LCD_CAM D0 | Bus de datos video → DAC resistivo → RCA amarillo |
| GPIO5 | LCD_CAM D1 | ídem |
| GPIO6 | LCD_CAM D2 | ídem |
| GPIO7 | LCD_CAM D3 | ídem |
| GPIO15 | LCD_CAM D4 | ídem |
| GPIO16 | LCD_CAM D5 | ídem |
| GPIO40 | LCD_CAM D6 | ídem |
| GPIO41 | LCD_CAM D7 | ídem |
| GPIO18 | Audio LEDC ch0 | Cap 1–10µF en serie → amp / RCA blanco |
| 3V3 | Alimentación lógica | Nunchuck VCC, pot extremo 1, DAC video |
| GND | Común | Nunchuck, pot extremo 2, start, RCA, cap audio |

### 4.2 Board VGA — `esp32LanderVGA/`

| GPIO | Señal | Destino físico |
|------|-------|----------------|
| GPIO21 | I2C SDA nunchuck | Nunchuck pin SDA |
| GPIO22 | I2C SCL nunchuck | Nunchuck pin SCL |
| GPIO34 | Pot 10kΩ (cursor) | Divisor 3V3–pot–GND |
| GPIO13 | Botón start | Pulsador → GND (INPUT_PULLUP) |
| GPIO25 | DAC1 video | [270Ω]×3 en paralelo → VGA R (1), G (2), B (3) |
| GPIO32 | HSYNC | VGA pin 13 (TTL directo) |
| GPIO33 | VSYNC | VGA pin 14 (TTL directo) |
| GPIO26 | Audio LEDC | Cap 1–10µF → amp / RCA blanco |
| GND | Común | Nunchuck, pot, start, VGA pines 5/6/7/8/10 |

### 4.3 Composite clásico (DESCONTINUADO) — `esp32LanderComposite/`

| GPIO | Señal | Destino físico |
|------|-------|----------------|
| GPIO21 | I2C SDA nunchuck | Nunchuck pin SDA |
| GPIO22 | I2C SCL nunchuck | Nunchuck pin SCL |
| GPIO34 | Pot 10kΩ (cursor) | Divisor 3V3–pot–GND |
| GPIO35 | Gatillo reóstato | **Desconectado** (LEGACY, divisor 120Ω histórico) |
| GPIO13 | Botón start | Pulsador → GND (INPUT_PULLUP) |
| GPIO25 | DAC video (aquaticus) | RCA amarillo (luma 255 B/N) |
| GPIO26 | Audio LEDC | Cap 1–10µF → RCA blanco |

---

## 5. Notas de seguridad

- **GND común en todo el sistema**: todas las placas, el nunchuck, el pot, los
  botones, el DAC de video y el audio DEBEN compartir el mismo GND. Sin GND común
  las lecturas ADC/I2C son erráticas y el video/audio puede acoplarse ruido.
- **Lógica 3.3V**: todos los GPIO del ESP32 son de 3.3V. NO aplicar 5V a ningún
  pin (el nunchuck se alimenta a 3V3; el botón y el pot van a 3V3, no a 5V).
- **VGA: no conectar +5V ni DDC** (pines 4/9/11/12/15 del DE-15). El monitor
  alimenta la terminación interna (75Ω) que forma el divisor; el +5V del pin 9
  puede dañar los GPIO si se conecta por error. Sync directo solo a 13/14.
- **Condensador de acople en audio**: la salida PWM/LEDC tiene DC (~1.65V).
  Siempre en serie el cap de 1–10µF antes del amp o del RCA blanco; sin él el DC
  corrompe la polarización de la entrada del amplificador.
- **Video S3**: el bus LCD_CAM sale con señal digital 3.3V; la red DAC resistiva
  (ver `[ver video_s3.cpp]`) es la que produce el nivel de video correcto. No
  conectar los GPIO4/5/6/7/15/16/40/41 directamente al RCA sin el DAC.
- **Reóstato del gatillo (histórico)**: el divisor con 120Ω da una ventana muy
  angosta (2.5→2.9V); no usarlo para otra cosa sin revisar la medición original
  en `docs/hardware.md`.