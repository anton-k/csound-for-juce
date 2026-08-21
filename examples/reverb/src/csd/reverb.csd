<CsoundSynthesizer>
<CsOptions>

</CsOptions>
<CsInstruments>
sr      =  48000
ksmps   =  4
nchnls  =  2
nchnls_i = 2
0dbfs   =  1.0

chn_k "size", 1, 2, 0.6, 0, 1
chnset 0.6, "size"

chn_k "tone", 1, 2, 0.6, 0, 1
chnset 0.6, "tone"

chn_k "mix", 1, 2, 0.6, 0, 1
chnset 0.6, "mix"

        instr 1
kfeedback chngetk "size"
kcutOff chngetk "tone"
kmix chngetk "mix"
printk2 kfeedback
printk2 kcutOff
printk2 kmix

ainL, ainR  inch 1, 2
awetL, awetR  reverbsc ainL, ainR, kfeedback, 20000 * kcutOff
aoutL = (1 - kmix) * ainL + kmix * (awetL)
aoutR = (1 - kmix) * ainR + kmix * (awetR)
outs aoutL, aoutR
endin
</CsInstruments>
<CsScore>
f 0 36000
i 1 0 -1
e
</CsScore>
</CsoundSynthesizer>
