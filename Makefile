SRC := ./src
TARGET := SNES_coin_chase

include ${PVSNESLIB_HOME}/devkitsnes/snes_rules

# Explicit default goal for older rulesets
.PHONY: all clean assets
all: assets $(TARGET).sfc

assets: tilfont.pic tilfont.pal sprites.pic sprites.pal

sprites.pic sprites.pal: sprites.png
	$(GFXCONV) -s 8 -o 16 -u 16 -t png -i $<
