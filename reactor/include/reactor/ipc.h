#include <atomic>
#include <type_traits>

namespace reactor {
    constexpr const char* shm_control_panel_name = "/reactor_control_panel";
    constexpr const char* shm_status_panel_name = "/reactor_status_panel";

    struct control_panel {
        const char magic_value[8] = "CTRLPNL";
        std::atomic<double> control_rod_target;
        std::atomic<double> pump_flow_target;
    };

    struct status_panel {
        const char magic_value[8] = "STATPNL";
        std::atomic<double> control_rod_position;
        std::atomic<double> pump_flow_rate;
        std::atomic<double> thermal_power;
        std::atomic<double> electrical_power;
        std::atomic<double> coolant_inlet_temperature;
        std::atomic<double> coolant_outlet_temperature;
        std::atomic_uint64_t tick_1;
        std::atomic_uint64_t tick_2;
    };

    static_assert(std::atomic<double>::is_always_lock_free,
                  "shared-memory command/status fields must be lock-free across processes");

    // atomic<double> must occupy exactly a naturally-aligned 8-byte double, so an
    // external reader (possibly another language) sees each field as a plain double.
    static_assert(sizeof(std::atomic<double>) == sizeof(double));
    static_assert(alignof(std::atomic<double>) == alignof(double));

    // Standard layout gives well-defined, reproducible field offsets across
    // processes and compilers mapping the same shared segment.
    static_assert(std::is_standard_layout_v<control_panel>);
    static_assert(std::is_standard_layout_v<status_panel>);

}