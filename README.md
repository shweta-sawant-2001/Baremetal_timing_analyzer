# Timing Analyzer Framework – PSoC 5LP

## Overview

This project implements a configurable **bare-metal timing analysis framework** for the PSoC 5LP microcontroller.

It enables precise measurement of:

- Code execution time
- Interrupt latency and preemption behavior
- CPU cycle consumption
- External timing verification using GPIO pins and a logic analyzer

The framework supports multiple analyzers running in parallel and follows object-oriented design principles in C.

---

## Key Features

- SysTick-based timing (1 ms resolution)
- DWT cycle counter (high precision CPU cycle measurement)
- Optional GPIO pin toggling for external measurement
- Multiple concurrent timing analyzers
- Start / Stop / Pause / Resume functionality
- Interrupt-safe timing capture
- Fully bare-metal (no RTOS)

---

## Hardware Platform

- MCU: PSoC 5LP
- CPU Clock: 24 MHz
- SysTick interval: 1 ms
- DWT cycle counter enabled
- UART used for logging
- Red, Green, Yellow LEDs used as instrumentation pins

---

## Architecture

Each analyzer instance is represented by a structured object:

```c
typedef struct
{
    const char *name;
    AnalyzerMode mode;
    bool running;
    bool paused;
    uint32_t start_time;
    uint32_t elapsed_time;
    PinSelect pin;
    void (*pin_on)(void);
    void (*pin_off)(void);
} TimingAnalyzer;
