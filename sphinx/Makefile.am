html-local:
	sphinx-build $(top_srcdir)/sphinx/source build
install-html-local:
	mkdir -p $(docdir)
	cp -rd build/* $(docdir)/

pdf-local:
	sphinx-build -M latexpdf $(top_srcdir)/sphinx/source/ build_pdf
install-pdf-local:
	mkdir -p $(docdir)
	cp build_pdf/latex/performanceanalysis.pdf $(docdir)/
