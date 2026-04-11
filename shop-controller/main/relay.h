#pragma once

/* Initialize all relay GPIOs to OFF (boot-safe) state. */
void relay_init(void);

/* Pulse relay for door index (0–4).  Does nothing if door has no relay.
 * Non-blocking — spawns a FreeRTOS task for the pulse duration. */
void relay_pulse(int door_index);
