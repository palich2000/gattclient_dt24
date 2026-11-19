.DEFAULT_GOAL := help
.PHONY: help install install-check

help:
	@printf '%s\n' 'install-check  Verify the prepared release on zigbeegate without restarting' 'install        Install that release and restart ble-mqtt.service on zigbeegate'

install-check:
	@sh scripts/deploy-combined.sh check

install: install-check
	@sh scripts/deploy-combined.sh install
