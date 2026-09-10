PYTHON ?= .venv/bin/python
PIO ?= .venv/bin/pio

.PHONY: check test test-display studio build build-hosts build-tag validate package-cardputer release-cardputer verify-release
check:
	$(PYTHON) tools/etag.py check
test: check
	$(PYTHON) -m unittest discover -s tests -v
	$(PIO) test -e native
	$(PYTHON) tools/test_display.py
test-display:
	$(PYTHON) tools/test_display.py
studio:
	$(PYTHON) tools/studio.py
build:
	$(PIO) run -e cardputer-adv
package-cardputer:
	$(PIO) run -e cardputer-adv -t package
release-cardputer:
	$(PIO) run -e cardputer-adv -t clean
	$(PIO) run -e cardputer-adv -t package
	$(PYTHON) tools/cardputer_release.py publish
verify-release:
	$(PYTHON) tools/cardputer_release.py verify
build-hosts:
	$(PIO) run -e cardputer-adv -e esp32-devkit -e esp32-s3-devkit
build-tag:
	$(PIO) run -d firmware/targets/cc2510
validate: test build-hosts build-tag verify-release
