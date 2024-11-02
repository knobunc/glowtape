all: sync-tape.pdf

%.pdf : %.ps
	ps2pdf $< $@

clean:
	rm -f sync-tape.pdf
