STL_TARGETS=fab/upper.stl fab/lower.stl

all: $(STL_TARGETS) fab/sync-tape.pdf

%.stl: %.scad
	openscad -o $@ $^

fab/%.scad : casing.scad
	mkdir -p fab
	echo "use <../$<>; $*();" > $@

fab/%.pdf : %.ps
	ps2pdf $< $@

%.dxf: %.ps
	pstoedit -psarg "-r600x600" -nb -mergetext -dt -f "dxf_s:-mm -ctl -splineaspolyline -splineprecision 30" $< $@

clean:
	rm -f $(STL_TARGETS)
