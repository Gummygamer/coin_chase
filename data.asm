; data.asm — assets
.include "hdr.asm"

    .section ".rodata1" superfree

; BG text font (already in your project)
tilfont:   .incbin "tilfont.pic"
palfont:   .incbin "tilfont.pal"

; --- NEW: sprite sheet (3 tiles of 8x8: player, coin, enemy) ---
sprgfx:    .incbin "sprites.pic"
sprgfx_end:
sprpal:    .incbin "sprites.pal"

    .ends
