
# this is normal Makefile, _not_ generated by autotools

SPHINX = sphinx-build -a -d ./doctrees/

html: *.rst fityk.css conf.py
	$(SPHINX) -b html . html
	rm -f html/objects.inv html/.buildinfo

fityk-banner.pdf: fityk-banner.svg
	inkscape --export-text-to-path --export-pdf=$@ $<

pdf:
	rm -rf doctrees/
	$(SPHINX) -b latex . latex
	cd latex/ && make all-pdf

website:
	rm -rf web
	BUILD_WEBSITE=1 $(SPHINX) -b html -Awebsite=1 . web
	cp screenshots/fityk-1.0.1-osx-so.png web/_images/
	rm -f web/objects.inv web/.buildinfo
	sed -i 's/&mdash; Fityk ..... manual//' web/index.html web/screens.html

.PHONY: pdf website
