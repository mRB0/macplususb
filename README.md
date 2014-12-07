## Pin assignments

### Keyboard

Colour | Mac keyboard pin | Teensy pin | Meaning
------ | ---------------- | ---------- | -------
gray   | 1                | GND        | GND
purple | 2                | PE7 (INT7) | KBD1 (Keyboard Clock)
blue   | 3                | ???        | KBD2 (Keyboard Data)
green  | 4                | +5V        | +5V

KBD2 (data) could be on PE4/5 (INT4/5) if an interrupt is required, but that will require more soldering.  Otherwise any other pin should suffice.


### Mouse

Colour | Mac mouse pin | Teensy pin | Meaning
------ | ------------- | ---------- | -------
green  | 1             | GND        | Ground
purple | 2             | +5V        | +5 Volts
blue   | 3             | GND        | Ground
black  | 4             | PD0 (INT0) | X Quadrature 1
orange | 5             | PD1 (INT1) | X Quadrature 2
n/c    | 6             | N/C        | No Connection
gray   | 7             | PE6 (INT6) | Button (Ground when pressed; needs pull-up)
white  | 8             | PD2 (INT2) | Y Quadrature 1
yellow | 9             | PD3 (INT3) | Y Quadrature 2
