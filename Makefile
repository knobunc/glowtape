all: fab/sync-tape.pdf

fab/%.pdf : %.ps
	ps2pdf $< $@

clean:
	rm -f fab/sync-tape.pdf
