FRONT_STL=a8pico_front.stl
BACK_STL=a8pico_back.stl
LOGO_STL=a8pico_logo.stl
COMBINED_STL=a8pico_combined.stl

MAIN_SCAD=a8pico.scad

SCAD_MODULES=\
	a8pico_back_v2.scad\
	a8pico_front.scad

all: $(FRONT_STL) $(BACK_STL) $(COMBINED_STL)

$(FRONT_STL): $(MAIN_SCAD) $(SCAD_MODULES)
	openscad -q -o $@ -D item=1 $<

$(BACK_STL): $(MAIN_SCAD) $(SCAD_MODULES)
	openscad -q -o $@ -D item=2 $<

$(LOGO_STL): $(MAIN_SCAD) $(SCAD_MODULES)
	openscad -q -o $@ -D item=3 $<

$(COMBINED_STL): $(MAIN_SCAD) $(SCAD_MODULES)
	openscad -q -o $@ -D item=4 $<

