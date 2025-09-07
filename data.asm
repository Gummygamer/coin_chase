; data.asm — assets section
.include "hdr.asm"

    .section ".rodata1" superfree

tilfont:
    .incbin "tilfont.pic"

palfont:
    .incbin "tilfont.pal"

    .ends
