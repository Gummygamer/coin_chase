SRC := ./src
TARGET := SNES_coin_chase

include ${PVSNESLIB_HOME}/devkitsnes/snes_rules

# Explicit default goal for older rulesets
.PHONY: all clean assets
all: assets $(TARGET).sfc

assets: tilfont.pic palfont.pal
tilfont.pic palfont.pal: tilfont.png
	$(GFXCONV) -s 8 -o 2 -u 16 -e 1 -p -t png -m -R -i $<
