#include <stdint.h>

/* Compile/link smoke image, not a board-specific display application.
 * Keep GPIO, RF, oscillator selection and attached peripherals at reset defaults.
 * The counter gives a future debugger an observable RAM location without guessing
 * a board's LED or display-power pin. Only valid for a confirmed CC2510F32.
 */
volatile __xdata uint32_t etag_ticks = 0;
const __code char etag_signature[] = "etag/cc2510f32/smoke/0.1.0";

void main(void) {
    for (;;) {
        ++etag_ticks;
    }
}
