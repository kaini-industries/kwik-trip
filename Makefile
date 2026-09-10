PYTHON ?= .venv/bin/python
PIO ?= .venv/bin/pio

.PHONY: check test build build-hosts build-tag validate
check:
	$(PYTHON) tools/etag.py check
test: check
	$(PYTHON) -m unittest discover -s tests -v
	$(PIO) test -e native
build:
	$(PIO) run -e cardputer-adv
build-hosts:
	$(PIO) run -e cardputer-adv -e esp32-devkit -e esp32-s3-devkit
build-tag:
	$(PIO) run -d firmware/targets/cc2510
validate: test build-hosts build-tag
