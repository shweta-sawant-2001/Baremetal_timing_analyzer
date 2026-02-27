/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/

#include "project.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>


// Constants
#define SYSTICK_INTERVAL_MS 1
#define SYSTEM_CLOCK_HZ 24000000  //  24 MHz clock
#define BCLK__BUS_CLK_HZ 24000000  //  24 MHz Bus clock
#define MAX_ANALYZERS 10
#define SYSTICK_RELOAD_VALUE (SYSTEM_CLOCK_HZ / 1000 * SYSTICK_INTERVAL_MS) - 1

// Structs and Enums
typedef enum
{
    DWT_CYCLE_COUNTER,
    DWT_CYCLE_COUNTER_OUTPUT_PIN,
    SYSTICK_TIMER,
    SYSTICK_OUTPUT_PIN,
    OUTPUT_PIN_ONLY
    
} AnalyzerMode;

// Structs and Enums
typedef enum
{  
    GREEN,
    YELLOW,
    RED
    
} PinSelect;
  

typedef struct
{
    const char *name;
    AnalyzerMode mode;
    bool running;
    bool paused;
    uint32_t start_time;
    uint32_t elapsed_time;
    PinSelect pin;
    void (*pin_on)(void);         //variable that takes no argument and stores nothing 
    void (*pin_off)(void);
    
} TimingAnalyzer;

// Global Variables
TimingAnalyzer analyzers[MAX_ANALYZERS];
uint8_t analyzer_count = 0;

// Function Prototypes
void SysTick_Handler(void);
void DWT_Init(void);
uint32_t get_systick_time_ms(void);
uint32_t get_dwt_cycle_count(void);
void timing_analyzer_create(AnalyzerMode mode, PinSelect pin, const char *name);
void timing_analyzer_start(TimingAnalyzer *analyzer);
void timing_analyzer_stop(TimingAnalyzer *analyzer);
void timing_analyzer_pause(TimingAnalyzer *analyzer);
void timing_analyzer_resume(TimingAnalyzer *analyzer);
void print_analyzer_status(TimingAnalyzer *analyzer);
void UART_Send(const char *message);
void LED_Init(void);
void Green_LED_OFF(void);
void Yellow_LED_OFF(void);
void Red_LED_OFF(void);
void Green_LED_ON(void);
void Yellow_LED_ON(void);
void Red_LED_ON(void);
// Implementations

volatile unsigned int TimingAnalyzer_SystickCounter = 0;

CY_ISR_PROTO(isr_timerTick);

void SysTick_Handler(void)
{
    TimingAnalyzer_SystickCounter++;
}

// Initialize SysTick for 1ms interrupts
void SysTick_Init(void)
{
    CySysTickInit();                            //initilize the systic counter
    CySysTickSetCallback(0, SysTick_Handler);
    CySysTickSetReload(BCLK__BUS_CLK_HZ/ 1000 - 1);
    CySysTickEnable();
}

// Initialize DWT for cycle counting
void DWT_Init(void) 
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable DWT
    DWT->CYCCNT = 0;                                // Clear cycle counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // Enable cycle counter
}

// Get current SysTick time in milliseconds
uint32_t get_systick_time_ms(void) 
{
    return TimingAnalyzer_SystickCounter;
}

// Get current DWT cycle count
uint32_t get_dwt_cycle_count(void)
{
    return DWT->CYCCNT;
}

// Initialize LED for toggle (if available)
void LED_Init(void)
{
    Green_LED_Write(1);
    Yellow_LED_Write(1);
    Red_LED_Write(1);
}

// GREEN LED Toggle
void Green_LED_OFF(void)
{
    Green_LED_Write(0);
}

// YELLOW LED Toggle
void Yellow_LED_OFF(void)
{
    Yellow_LED_Write(0);
}

// RED LED Toggle
void Red_LED_OFF(void)
{
    Red_LED_Write(0);
}
// GREEN LED Toggle
void Green_LED_ON(void)
{
    Green_LED_Write(1);
}

// YELLOW LED Toggle
void Yellow_LED_ON(void)
{
    Yellow_LED_Write(1);
}

// RED LED Toggle
void Red_LED_ON(void)
{
    Red_LED_Write(1);
}
//---------------------------------------------------------------------------------------------------//


// Create a Timing Analyzer
void timing_analyzer_create(AnalyzerMode mode, PinSelect pin, const char *name)
{
    if (analyzer_count >= MAX_ANALYZERS) 
    return;  
    
    
    TimingAnalyzer *analyzer = &analyzers[analyzer_count++];
    
    analyzer->name = name;
    
    analyzer->mode = mode;
    analyzer->pin = pin;
    
    //change state
    analyzer->running = false;
    analyzer->paused = false;
    analyzer->elapsed_time = 0;
    analyzer->start_time = 0;
    
    switch(pin)
    {
            case GREEN:
                analyzer->pin_on  = Green_LED_ON;   // no ()
                analyzer->pin_off = Green_LED_OFF;
                break;

            case YELLOW:
                analyzer->pin_on  = Yellow_LED_ON;
                analyzer->pin_off = Yellow_LED_OFF;
                break;

            case RED:
                analyzer->pin_on  = Red_LED_ON;
                analyzer->pin_off = Red_LED_OFF;
                break;

            default:
                analyzer->pin_on  = NULL;
                analyzer->pin_off = NULL;
                break;
        }
}
//---------------------------------------------------------------------------------------------------//
// Start the analyzer
void timing_analyzer_start(TimingAnalyzer *analyzer)
{
    uint32_t time_now_start = 0;
    //FIRST INSTRUCTION : CAPTURE the mode
    if (analyzer-> mode == SYSTICK_TIMER || analyzer-> mode == SYSTICK_OUTPUT_PIN)
    {
        time_now_start = get_systick_time_ms();   
    }
    else if( analyzer->mode == DWT_CYCLE_COUNTER || analyzer-> mode == DWT_CYCLE_COUNTER_OUTPUT_PIN)
    {
         time_now_start = get_dwt_cycle_count();
    }
    
        if(analyzer->mode == OUTPUT_PIN_ONLY)
    {
        analyzer->start_time = time_now_start;
    }
    
    //2nd: check the state
    if(analyzer->running && !analyzer->paused)
    {
        return;
    }
    
    //3rd: STORE THE START TIME ONLY IF TIMING IS USED

    
    //4th UPDATE:
    analyzer->running = true;
    analyzer->paused = false;
    
    if(analyzer->pin_on){
       analyzer->pin_on();
    }
    else{
        analyzer->pin_off();
    }
    
    //UART_1_PutString("HI I am in Started\r\n");
}

//---------------------------------------------------------------------------------------------------//
// Pause the analyzer
void timing_analyzer_pause(TimingAnalyzer *analyzer)
{
        
    //Without reading the current time in pause(), we don’t know how long has passed since start.
    uint32_t time_now_pause = 0;
    
    if (analyzer->mode == SYSTICK_TIMER || analyzer->mode == SYSTICK_OUTPUT_PIN)
    {
        time_now_pause = get_systick_time_ms();
    }
    else if (analyzer->mode == DWT_CYCLE_COUNTER || analyzer->mode == DWT_CYCLE_COUNTER_OUTPUT_PIN)
    {
        time_now_pause = get_dwt_cycle_count();
    }
    
    if (!analyzer->running || analyzer->paused)
    {
        return;
    }
    
    analyzer->elapsed_time += time_now_pause - analyzer->start_time;
    analyzer->paused = true;
    //UART_1_PutString("Analyzer Paused\r\n");
}

//---------------------------------------------------------------------------------------------------//

// Resume the analyzer
void timing_analyzer_resume(TimingAnalyzer *analyzer)
{
 
    uint32_t time_now_resume = 0;
    
    if (analyzer->mode == SYSTICK_TIMER || analyzer->mode == SYSTICK_OUTPUT_PIN) {
        time_now_resume = get_systick_time_ms();
    }
    else if (analyzer->mode == DWT_CYCLE_COUNTER || analyzer->mode == DWT_CYCLE_COUNTER_OUTPUT_PIN)
    {
        time_now_resume = get_dwt_cycle_count();
    }
    
    
    if (!analyzer->paused) 
    {
        return;
    }
    
    analyzer->start_time = time_now_resume;
    
    analyzer->paused = false;
    
    //UART_1_PutString("Analyzer Resumed\r\n");
}
//---------------------------------------------------------------------------------------------------//

// Stop the analyzer
void timing_analyzer_stop(TimingAnalyzer *analyzer)
{
    //️ Capture timestamp FIRST
    uint32_t end_time = 0;   
    if (analyzer->mode == SYSTICK_TIMER || analyzer->mode == SYSTICK_OUTPUT_PIN)
    {
        end_time = get_systick_time_ms();
    }
    else if (analyzer->mode == DWT_CYCLE_COUNTER || analyzer->mode == DWT_CYCLE_COUNTER_OUTPUT_PIN)
    {
        end_time = get_dwt_cycle_count();
    }
        //// Only stop if running
    if (!analyzer->running)
    {
        return;
    }
        // Update elapsed time if not paused
    if (!analyzer->paused)
        analyzer->elapsed_time += end_time - analyzer->start_time;
    
        // Reset state
    analyzer->running = false;
    analyzer->paused = false;
    
   /* if(analyzer->mode == OUTPUT_PIN_ONLY || analyzer->mode == SYSTICK_OUTPUT_PIN || analyzer->mode == DWT_CYCLE_COUNTER_OUTPUT_PIN)
    {
         LED_Init();
    }   
    if (!analyzer->paused)
    {
        analyzer->elapsed_time += end_time - analyzer->start_time;
    }    
    analyzer->running = false;
    */
    //UART_1_PutString("Analyzer Stopped\r\n");
}



// Print the status of a single analyzer
void print_analyzer_status(TimingAnalyzer *analyzer)
{
    char buffer[100];
    
    if (analyzer->mode == SYSTICK_TIMER || analyzer->mode == SYSTICK_OUTPUT_PIN)
    {
        snprintf(buffer, sizeof(buffer), "\r\n%s: Elapsed time: %lu ms\n", analyzer->name, analyzer->elapsed_time);
    
        UART_Send(buffer);
    }
    if (analyzer->mode == DWT_CYCLE_COUNTER || analyzer->mode == DWT_CYCLE_COUNTER_OUTPUT_PIN)
    {
        char buffer1[100];
        
        snprintf(buffer1, sizeof(buffer), "\r\n%s: Elapsed time: %lu cycles\n", analyzer->name, analyzer->elapsed_time);
        uint32_t timeDWT = (analyzer->elapsed_time / SYSTEM_CLOCK_HZ) * 1000;
        snprintf(buffer, sizeof(buffer), "\r\n%s: Elapsed time: %lu ms\n", analyzer->name, timeDWT);
    
        UART_Send(buffer);
        UART_Send(buffer1);
    }
}

// UART Send function
void UART_Send(const char *message)
{
    UART_PutString(message);      // Send string over UART
}

CY_ISR(ISR_1ms_Handler)
{
    Timer_1ms_ReadStatusRegister();  // Clears the interrupt by reading the status register   
    timing_analyzer_start(&analyzers[7]);  // Start analyzer
    timing_analyzer_stop(&analyzers[7]);   // Stop analyzer
    //print_analyzer_status(&analyzers[0]);  // Print the status of analyzer
}

CY_ISR(ISR_2s_Handler)
{   
    Timer_2s_ReadStatusRegister();  // Clears the interrupt by reading the status register
    timing_analyzer_start(&analyzers[8]);  // Start analyzer
    timing_analyzer_stop(&analyzers[8]);   // Stop analyzer
    //print_analyzer_status(&analyzers[0]);  // Print the status of analyzer
}

int main(void)
{
    // --------------------- Initialization --------------------- //
    CyGlobalIntEnable;    // Enable global interrupts
    UART_Start();         // UART for printing
    //LED_Init();           // Initialize LEDs
    SysTick_Init();       // Initialize 1ms SysTick
    DWT_Init();           // Initialize DWT cycle counter

    UART_PutString("\r\n=== Timing Analyzer Test Start ===\r\n");

    // --------------------- 1. DWT Cycle Counter --------------------- //
    timing_analyzer_create(DWT_CYCLE_COUNTER, RED, "\r\nDWT Only\r\n");
    timing_analyzer_start(&analyzers[0]);
    
    for (volatile int i = 0; i < 1000000; i++); // Work to measure
    
    timing_analyzer_stop(&analyzers[0]);
    print_analyzer_status(&analyzers[0]);

    // --------------------- 2. DWT + Output Pin --------------------- //
    timing_analyzer_create(DWT_CYCLE_COUNTER_OUTPUT_PIN, GREEN, "\r\nDWT + LED\r\n");
    timing_analyzer_start(&analyzers[1]);
    
    for (volatile int i = 0; i < 2000000; i++); // Work to measure
    
    timing_analyzer_stop(&analyzers[1]);
    print_analyzer_status(&analyzers[1]);

    // --------------------- 3. SysTick Timer --------------------- //
    timing_analyzer_create(SYSTICK_TIMER, YELLOW, "\r\nSysTick Only\r\n");
    timing_analyzer_start(&analyzers[2]);
    
    CyDelay(1500); // 1.5 seconds delay
    
    timing_analyzer_stop(&analyzers[2]);
    print_analyzer_status(&analyzers[2]);

    // --------------------- 4. SysTick + Output Pin --------------------- //
    timing_analyzer_create(SYSTICK_OUTPUT_PIN, RED, "\r\nSysTick + LED\r\n");
    timing_analyzer_start(&analyzers[3]);
    
    CyDelay(2000); // 2 seconds delay
    
    timing_analyzer_stop(&analyzers[3]);
    print_analyzer_status(&analyzers[3]);

    // --------------------- 5. Output Pin Only --------------------- //
    timing_analyzer_create(OUTPUT_PIN_ONLY, GREEN, "\r\nPin Only\r\n");
    timing_analyzer_start(&analyzers[4]);
    
    CyDelay(1000); // Toggle pin for 1 second
    
    timing_analyzer_stop(&analyzers[4]);

    // --------------------- 6. Multiple Analyzers --------------------- //
    // Reuse DWT + SysTick
    timing_analyzer_create(DWT_CYCLE_COUNTER, RED, "\r\nDWT Multi\r\n");
    timing_analyzer_create(SYSTICK_TIMER, YELLOW, "\r\nSysTick Multi\r\n");
    
    timing_analyzer_start(&analyzers[5]); // DWT Multi
    timing_analyzer_start(&analyzers[6]); // SysTick Multi
    
    for (volatile int i = 0; i < 3000000; i++); // Work to measure
    
    timing_analyzer_stop(&analyzers[5]);
    timing_analyzer_stop(&analyzers[6]);
    
    print_analyzer_status(&analyzers[5]);
    print_analyzer_status(&analyzers[6]);

    // --------------------- 7. Interrupt-based Timing --------------------- //

    
    timing_analyzer_create(OUTPUT_PIN_ONLY, GREEN , "OUTPUT_PIN_ONLY");  // For 1ms interrupt
    timing_analyzer_create(OUTPUT_PIN_ONLY, YELLOW , "OUTPUT_PIN_ONLY"); // For 2s interrupt
    
    //Timer 1ms
    Timer_1ms_Init();                                // Initialize Timer 1
    Timer_1ms_WriteCounter(0);                       // Reset counter to 0
     Timer_1ms_Enable();
    Timer_1ms_Start();                               // Enable terminal count interrupt
    Timer_1ms_ReadStatusRegister();                  // Clear any pending interrupt for Timer 2
    ISR_1ms_StartEx(ISR_1ms_Handler);                // Attach ISR to Timer_1 interrupt
    
    //Timer 2s
    Timer_2s_Init();                                 // Initialize Timer 2
    Timer_2s_WriteCounter(0);                        // Reset counter to 0
    Timer_2s_Start();                                // Start Timer 2
    Timer_2s_ReadStatusRegister();                   // Clear any pending interrupt for Timer 2
    ISR_2s_StartEx(ISR_2s_Handler);                  // Attach ISR to Timer_2 interrupt   
    

    
    
    //  ======================= Interrupts DWT 1ms Start =======================  //
    
    timing_analyzer_create(DWT_CYCLE_COUNTER, GREEN, "DWT Analyzer");
    
    //Timer 1ms
    Timer_1ms_Init();                                // Initialize Timer 1
    Timer_1ms_WriteCounter(0);                       // Reset counter to 0
    Timer_1ms_Start();                               // Enable terminal count interrupt
    Timer_1ms_ReadStatusRegister();                  // Clear any pending interrupt for Timer 2
    ISR_1ms_StartEx(ISR_1ms_Handler);                // Attach ISR to Timer_1 interrupt
    
    //  ======================= Interrupts DWT 1ms End =======================  //
    
    
    
    //  ======================= Interrupts DWT 2s Start =======================  //
    
    timing_analyzer_create(DWT_CYCLE_COUNTER, GREEN, "DWT Analyzer");
    
    //Timer 2s
    Timer_2s_Init();                                 // Initialize Timer 2
    Timer_2s_WriteCounter(0);                        // Reset counter to 0
    Timer_2s_Start();                                // Start Timer 2
    Timer_2s_ReadStatusRegister();                   // Clear any pending interrupt for Timer 2
    ISR_2s_StartEx(ISR_2s_Handler);                  // Attach ISR to Timer_2 interrupt   
    
    //  ======================= Interrupts DWT 2s End =======================  //
    
    
    
    
    //  ======================= Interrupts Systick 1ms Start =======================  //
    
    timing_analyzer_create(SYSTICK_TIMER, RED, "SysTick Analyzer 1 sec");
    
    //Timer 1ms
    Timer_1ms_Init();                                // Initialize Timer 1
    Timer_1ms_WriteCounter(0);                       // Reset counter to 0
     Timer_1ms_Enable();
    Timer_1ms_Start();                               // Enable terminal count interrupt
    Timer_1ms_ReadStatusRegister();                  // Clear any pending interrupt for Timer 2
    ISR_1ms_StartEx(ISR_1ms_Handler);                // Attach ISR to Timer_1 interrupt
    
    //  ======================= Interrupts Systick 1ms End =======================  //
    
   
    
    //  ======================= Interrupts Systick 2s Start =======================  //
    
    timing_analyzer_create(SYSTICK_TIMER, RED, "SysTick Analyzer");
    
    //Timer 2s
    Timer_2s_Init();                                 // Initialize Timer 2
    Timer_2s_WriteCounter(0);                        // Reset counter to 0
    Timer_2s_Start();                                // Start Timer 2
    Timer_2s_ReadStatusRegister();                   // Clear any pending interrupt for Timer 2
    ISR_2s_StartEx(ISR_2s_Handler);                  // Attach ISR to Timer_2 interrupt   
    return 0;
}


/* [] END OF FILE */