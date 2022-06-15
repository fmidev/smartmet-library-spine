# Use smartmet-plugin-test from build directory when possible
SMARTMET_PLUGIN_TEST := ../../app/smartmet-plugin-test

all:	all-test-engine	all-test-plugin

clean:	clean-test-engine clean-test-plugin
	rm -rf failures

# Run tests (ensure that freshly built libsmartmet-spine.so is being used)
test:	all $(SMARTMET_PLUGIN_TEST)
	pwd
	ls -l ../..
	LD_LIBRARY_PATH=../../ $(SMARTMET_PLUGIN_TEST) -c reactor.conf -H /test

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
