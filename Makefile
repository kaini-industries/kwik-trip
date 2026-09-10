PYTHON ?= .venv/bin/python
PIO ?= .venv/bin/pio

.PHONY: check test test-display studio build build-hosts build-tag validate
check:
	$(PYTHON) tools/etag.py check
test: check
	$(PYTHON) -m unittest discover -s tests -v
	$(PIO) test -e native
	$(PYTHON) tools/test_display.py
test-display:
	$(PYTHON) tools/test_display.py
studio:
	$(PYTHON) -m http.server 8000 --bind 127.0.0.1 --directory web
build:
	$(PIO) run -e cardputer-adv
build-hosts:
	$(PIO) run -e cardputer-adv -e esp32-devkit -e esp32-s3-devkit
build-tag:
	$(PIO) run -d firmware/targets/cc2510
validate: test build-hosts build-tag
