# Pressurized Water Reactor Simulator

A real-time pressurized water reactor (PWR) simulator with heartbeat-synchronized modules, realistic point kinetics, and thermal-hydraulic modeling.

## Architecture

The simulator is composed of independent modules, each running on its own thread. All modules are synchronized via a `std::barrier` — every tick, each module reads its inputs, computes, writes its outputs, and waits at the barrier. The barrier's completion function swaps the output buffers so that the next tick's inputs reflect the previous tick's outputs.

```
┌─────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ ControlRods │───▶│  ReactorCore │───▶│  CoolantLoop │───▶│   Turbine    │
│  (module)   │ ρ   │   (module)   │  Q  │   (module)   │ T_out│  (module)   │
└─────────────┘     └──────────────┘     └──────▲───────┘     └──────┬───────┘
       ▲                                         │  T_in (next tick)  │
       │            ┌──────────────┐             └─────────────────────┘
       │            │              │
       └────────────│  Coordinator │
         commands   │  (barrier)   │
                    └──────────────┘
```

### Data Flow

1. **ControlRods** receives a target position command and outputs the current rod position (which determines reactivity).
2. **ReactorCore** takes rod reactivity and coolant temperature, solves neutron kinetics and fuel thermal equations, and outputs thermal power and fuel temperature.
3. **CoolantLoop** takes thermal power and the (fixed) pump flow rate, computes heat delivered to the coolant and the new outlet temperature, using the coolant inlet temperature set by **Turbine** on the previous tick.
4. **Turbine** (secondary loop / steam generator) extracts heat from the hot-leg (outlet) coolant to generate steam/electrical power, and cools the coolant down before it returns to the core as next tick's inlet temperature — closing the primary/secondary loop.
5. **Coordinator** manages the tick clock and `std::barrier`, swapping double-buffered state each tick.

### Double Buffering

Inter-module data is stored in a `ReactorState` struct with two buffers. During a tick, modules read from the "previous" buffer and write to the "current" buffer. At the barrier, the buffers are swapped. This eliminates the need for locks during computation.

### Tick Rate

Default: **10 Hz** (100 ms period). Configurable via `SimulatorConfig::tick_period`. All other physics parameters below are fixed `constexpr` values in `reactor::constants` (see `constants.h`), not runtime-configurable — the only runtime-configurable simulation input is `steam_generator_effectiveness` (via `Simulator::set_steam_generator_effectiveness`), which lives in `ReactorState`/`Commands`.

---

## Physics Model

### 1. Control Rod Reactivity

Control rods are modeled as a single aggregate position from 0% (fully inserted) to 100% (fully withdrawn).

**Rod movement:**

$$\frac{d(\text{position})}{dt} = v_{\text{rod}}$$

where $v_{\text{rod}}$ is clamped to $\pm v_{\max}$ (default: 2 %/s).

**Reactivity contribution:**

$$\rho_{\text{rod}} = -\rho_{\text{worth}} \cdot \left(1 - \frac{\text{position}}{100}\right)$$

At 100% (fully withdrawn): $\rho_{\text{rod}} = 0$ (critical). At 0% (fully inserted): $\rho_{\text{rod}} = -\rho_{\text{worth}}$ (subcritical).

| Parameter | Symbol | Default Value | Unit |
|-----------|--------|---------------|------|
| Total rod worth | $\rho_{\text{worth}}$ | 0.08 | Δk/k |
| Max rod speed | $v_{\max}$ | 2.0 | %/s |

### 2. Neutron Point Kinetics (One Delayed Group)

The reactor's neutron population is governed by the point kinetics equations with a single effective delayed neutron group:

$$\frac{dn}{dt} = \frac{\rho - \beta}{\Lambda} \cdot n + \lambda \cdot C$$

$$\frac{dC}{dt} = \frac{\beta}{\Lambda} \cdot n - \lambda \cdot C$$

where:

| Symbol | Description | Default Value | Unit |
|--------|-------------|---------------|------|
| $n$ | Neutron population (normalized to nominal power) | — | — |
| $C$ | Delayed neutron precursor concentration | — | — |
| $\rho$ | Total reactivity | — | Δk/k |
| $\beta$ | Delayed neutron fraction | 0.0065 | — |
| $\Lambda$ | Prompt neutron generation time | 0.08 | s |
| $\lambda$ | Delayed neutron precursor decay constant | 0.08 | 1/s |

**Total reactivity** includes rod reactivity and Doppler feedback:

$$\rho_{\text{total}} = \rho_{\text{rod}} + \alpha_{\text{fuel}} \cdot (T_{\text{fuel}} - T_{\text{fuel,ref}})$$

| Parameter | Symbol | Default Value | Unit |
|-----------|--------|---------------|------|
| Fuel temperature coefficient (Doppler) | $\alpha_{\text{fuel}}$ | −1.7284 × 10⁻⁴ | Δk/k/°C |
| Reference fuel temperature | $T_{\text{fuel,ref}}$ | 614 | °C |

**Thermal power** from the neutron population:

$$P_{\text{fission}} = n \cdot P_{\text{nominal}}$$

| Parameter | Symbol | Default Value | Unit |
|-----------|--------|---------------|------|
| Nominal thermal power | $P_{\text{nominal}}$ | 3000 × 10⁶ | W |

### 3. Fuel Thermal Model

The fuel has thermal inertia. Fission deposits energy in the fuel, which then transfers heat to the coolant through a thermal resistance (gap + cladding):

$$M_f \cdot c_f \cdot \frac{dT_{\text{fuel}}}{dt} = P_{\text{fission}} - h_{\text{gap}} \cdot A_{\text{gap}} \cdot (T_{\text{fuel}} - T_{\text{coolant}})$$

where $T_{\text{coolant}}$ is the average coolant temperature in the core: $T_{\text{coolant}} = (T_{\text{in}} + T_{\text{out}}) / 2$.

| Parameter | Symbol | Default Value | Unit |
|-----------|--------|---------------|------|
| Fuel mass | $M_f$ | 100,000 | kg |
| Fuel specific heat | $c_f$ | 300 | J/(kg·K) |
| Gap heat transfer coefficient × area | $h_{\text{gap}} \cdot A_{\text{gap}}$ | 1.0 × 10⁷ | W/K |

The fuel thermal time constant is:

$$\tau_f = \frac{M_f \cdot c_f}{h_{\text{gap}} \cdot A_{\text{gap}}} = \frac{100000 \times 300}{10^7} = 3.0 \text{ s}$$

### 4. Coolant Loop

The coolant (water) in the reactor core is modeled as a lumped volume with thermal inertia. The pump flow rate is fixed (not commandable). The inlet (cold-leg) temperature $T_{\text{in}}$ is **not** a fixed constant — it is set dynamically every tick by the Turbine's secondary loop, based on the outlet temperature and heat extraction from the *previous* tick (one-tick double-buffer lag), closing the primary/secondary loop:

$$M_c \cdot c_p \cdot \frac{dT_{\text{out}}}{dt} = Q_{\text{core}} - Q_{\text{removed}}$$

**Heat input from fuel:**

$$Q_{\text{core}} = h_{\text{gap}} \cdot A_{\text{gap}} \cdot (T_{\text{fuel}} - T_{\text{coolant}})$$

**Heat removed by coolant flow:**

$$Q_{\text{removed}} = \dot{m} \cdot c_p \cdot (T_{\text{out}} - T_{\text{in}})$$

| Parameter | Symbol | Default Value | Unit |
|-----------|--------|---------------|------|
| Coolant mass in core | $M_c$ | 20,000 | kg |
| Coolant specific heat | $c_p$ | 4,180 | J/(kg·K) |
| Initial inlet temperature (cold leg) | $T_{\text{in}}$ | 290 | °C |
| Pump flow rate (fixed) | $\dot{m}$ | 15,000 | kg/s |

The coolant thermal time constant is:

$$\tau_c = \frac{M_c \cdot c_p}{\dot{m} \cdot c_p} = \frac{M_c}{\dot{m}} = \frac{20000}{15000} \approx 1.3 \text{ s}$$

### 5. Turbine (Secondary Loop / Power Conversion)

The Turbine module models the secondary loop (steam generator): it extracts heat from the hot-leg (outlet) primary coolant to generate steam/electrical power, and cools the coolant down before it returns to the core as the *next* tick's inlet temperature. The amount extracted is bounded by the maximum possible — cooling the coolant down to the **secondary-side saturation temperature** (the pinch-point limit of the heat exchanger: the primary side can never be cooled below the boiling point of the secondary water, since heat only flows from hotter to colder fluid) — scaled by a configurable effectiveness:

$$Q_{\text{max}} = \max\left(\dot{m} \cdot c_p \cdot (T_{\text{out}} - T_{\text{sat}}),\ 0\right), \qquad Q_{\text{extracted}} = \frac{\varepsilon_{\text{sg}}}{100} \cdot Q_{\text{max}}$$

$$T_{\text{in}}^{\text{next}} = T_{\text{out}} - \frac{Q_{\text{extracted}}}{\dot{m} \cdot c_p}$$

Electrical output is a fixed fraction of the ideal Carnot limit, applied to the extracted heat. The cold sink for this Carnot limit is the **condenser** temperature — a separate, downstream heat exchanger where the low-pressure turbine exhaust steam condenses, unrelated to the steam-generator pinch point above — so hotter coolant converts more efficiently:

$$P_{\text{electric}} = \eta(T_{\text{out}}) \cdot Q_{\text{extracted}}, \qquad \eta(T_{\text{out}}) = \eta_{\text{rel}} \left(1 - \frac{T_{\text{cold}}}{T_{\text{hot}}}\right)$$

where $T_{\text{hot}} = T_{\text{out}} + 273.15$ K and $T_{\text{cold}} = T_{\text{condenser}} + 273.15$ K. At nominal conditions ($T_{\text{out}} \approx 338\,°$C) this yields $\eta \approx 0.33$. Because both electrical output and the returned inlet temperature depend only on the current outlet temperature and flow (no state of their own), they track coolant conditions directly (with the usual one-tick double-buffer lag).

| Parameter | Symbol | Default Value | Unit |
|-----------|--------|---------------|------|
| Carnot fraction (relative efficiency) | $\eta_{\text{rel}}$ | 0.655 | — |
| Condenser temperature | $T_{\text{condenser}}$ | 30 | °C |
| Secondary saturation (pinch-point) temperature | $T_{\text{sat}}$ | 270 | °C |
| Steam generator effectiveness (initial value; commandable at runtime) | $\varepsilon_{\text{sg}}$ | 70.52 | % |

The default effectiveness is tuned so nominal full-power operation (rods fully withdrawn) reproduces the classic ~290 °C / ~338 °C inlet/outlet steady state. Because the pinch-point limit (270 °C) is much closer to the nominal outlet temperature than the condenser temperature, the coolant *can* physically be over-cooled toward 270 °C during transients (e.g. a power reduction where heat extraction briefly outpaces core heat generation) — useful as a realistic bound for detecting an overly cold primary loop.

### Nominal Steady-State Operating Point

At full power with rods fully withdrawn:

| Quantity | Value |
|----------|-------|
| Thermal power | 3000 MW |
| Electrical power | ~990 MW |
| Fuel temperature | ~614 °C |
| Coolant inlet | 290 °C |
| Coolant outlet | ~338 °C |
| Pump flow rate | 15,000 kg/s |
| Rod position | 100% (fully withdrawn) |

### ODE Integration

All differential equations are integrated using **forward Euler** with a fixed time step equal to the tick period (default 100 ms). This is sufficient for the time constants involved (3.0 s fuel, 1.3 s coolant, ~10 s delayed neutrons). Upgrade to RK4 is possible if higher accuracy is needed.

---

## Module Interface

Every module implements:

```cpp
class Module {
public:
    virtual ~Module() = default;
    virtual void tick(std::chrono::duration<double> dt, const ReactorState& read, ReactorState& write) = 0;
};
```

- `dt` — time step in seconds
- `read` — previous tick's state (read-only during tick)
- `write` — current tick's state (write target)

---

## Public API

The `Simulator` class provides a thread-safe wrapper for external processes:

```cpp
class Simulator {
public:
    explicit Simulator(SimulatorConfig config);
    void start();
    void stop();

    // Commands (thread-safe, applied on next tick)
    void move_control_rods(double target_percent);
    void set_steam_generator_effectiveness(double percent); // clamped to [0, 100]

    // Queries (thread-safe, reads latest completed state)
    ReactorState get_reactor_state() const;
    uint64_t get_tick_count() const;
};
```

---

## Future Extensions

- **Xenon poisoning**: Xe-135 buildup/decay affecting reactivity ($\rho_{\text{Xe}}$)
- **Void coefficient**: reduced moderation at high coolant temperature
- **Secondary loop dynamics**: steam generator thermal inertia, condenser dynamics, pump-power penalty for a net-power optimum
- **Multiple delayed neutron groups**: 6-group model for higher fidelity
- **Decay heat**: residual heat after shutdown

---

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
```

Requires a C++23-capable compiler (GCC 13+ or Clang 17+).
