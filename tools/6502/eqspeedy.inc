; speedy 1050 equates

TRACKNO	= $0D

DAUX1	= $82
DAUX2	= $83

MOTON	= $FF09
SEEK0	= $FF1E
SEEKTRK	= $FF21

SENDBUF	= $FF54 ; A=count, X=lo adr, Y=hi adr
SEND41	= $FF57
SEND43	= $FF5A

; configuration for sector reading
RDID	= $8400
RDREFS	= RDID+$20
RDDLY	= RDID+$40
RDTRK	= RDID+$60
RDRTRK	= RDID+$61

; statusses of sectors
RDSTAT	= RDID+$80

