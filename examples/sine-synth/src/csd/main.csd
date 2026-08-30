<CsoundSynthesizer>
<CsOptions>

</CsOptions>
<CsInstruments>
sr      =  48000
ksmps   =  32
nchnls  =  2
nchnls_i = 0
0dbfs   =  1.0


chn_k "gain", 1, 2, 0.6, 0, 1
; chnset 0.6, "gain"

chn_k "attack", 1, 2, 0.02, 0, 1
chnset 0.6, "attack"
chn_k "decay", 1, 2, 0.6, 0, 1
; chnset 0.6, "decay"

chn_k "sustain", 1, 2, 0.6, 0, 1
; chnset 0.6, "sustain"


chn_k "release", 1, 2, 0.6, 0, 1
; chnset 0.6, "release"


; TODO kcutOff - non-linear scale for frequencies
        instr 1
kgain chngetk "gain"
printk2 kgain

kattack chngetk "attack"
printk2 kattack
iattack = i(kattack) + 0.01

kdecay chngetk "decay"
printk2 kdecay
idecay = i(kdecay) + 0.001

ksustain chngetk "sustain"
printk2 ksustain
isustain = i(ksustain) + 0.0

krelease chngetk "release"
printk2 krelease
irelease = i(krelease) + 0.01

iNote notnum
iamp ampmidi 1.0
icps cpsmidinn iNote

kenv madsr iattack, idecay, isustain, irelease
asig oscil3 1, icps, 1
aoutL = asig * iamp * kgain * kenv
aoutR = asig * iamp * kgain * kenv

outs aoutL, aoutR
endin
</CsInstruments>
<CsScore>
f 0 36000
f1 0 128 10 1

e
</CsScore>
</CsoundSynthesizer>
