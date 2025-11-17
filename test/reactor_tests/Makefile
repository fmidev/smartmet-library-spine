# Use smartmet-plugin-test from build directory when possible
SMARTMET_PLUGIN_TEST := ../../app/smartmet-plugin-test

all:	all-test-engine	all-test-plugin

clean:	clean-test-engine clean-test-plugin
	rm -rf failures
	rm -rf log

# Run tests (ensure that freshly built libsmartmet-spine.so is being used)
run-test:	all $(SMARTMET_PLUGIN_TEST)
	if [ -d log ] ; then rm -f log/* ; fi
	mkdir -p log
	LD_LIBRARY_PATH=../../ $(TEST_RUNNER) $(SMARTMET_PLUGIN_TEST) -c reactor.conf -H /test

test:	run-test
	@if [ -f log/nomatch-access-log ] ; then \
		if [ "$$(wc -l <log/nomatch-access-log)" -eq 1 ] ; then \
			true; \
		else \
			echo "ERROR: There must be exactly 1 request that did not match any handler! See log/nomatch-access-log for details."; \
		fi; \
	else \
		echo "ERROR: Missing log/nomatch-access-log file!"; \
	fi

all-test-engine:
	$(MAKE) -C test_engine all

all-test-plugin:
	$(MAKE) -C test_plugin all

clean-test-engine:
	$(MAKE) -C test_engine clean

clean-test-plugin:
	$(MAKE) -C test_plugin clean

$(SMARTMET_PLUGIN_TEST):
	$(MAKE) -C $(shell dirname $(SMARTMET_PLUGIN_TEST))
