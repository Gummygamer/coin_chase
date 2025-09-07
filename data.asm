.include "hdr.asm"

    .section ".rodata1" superfree

tilfont:   .incbin "tilfont.pic"
palfont:   .incbin "tilfont.pal"

sprgfx:    .incbin "sprites.pic"
sprgfx_end:
sprpal:    .incbin "sprites.pal"

; --- NEW: BRR sound effects ---
sfx_coin:       .incbin "sfx_coin.brr"
sfx_coin_end:
sfx_gameover:   .incbin "sfx_gameover.brr"
sfx_gameover_end:

    .ends