; 1050 equates

; 2793 FDC

FDCCMD	= $0400
FDCSTA	= $0400
FDCTRK	= $0401
FDCSEC	= $0402
FDCDAT	= $0403

; 6532 RIOT

PORTA	= $0280
DDRA	= $0281
PORTB	= $0282
DDRB	= $0283
RINT	= $0284 ; reset interrupt
RTIM	= $028C ; read timer
STIM1	= $029E ; 64 clock ticks
STIM2	= $029F ; 1024 clock ticks

; track timing

TRKTIM	= 210	; disk rotation time
TRKREM	= 200	; time to wait after first sector

; stepping timing
STEPTIM	= 82	; 5248 usec half-track stepping time

; head settling time - 20480 usec
SETLTIM	= 20

