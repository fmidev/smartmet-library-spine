# Use smartmet-plugin-test from build directory when possible
ifeq ($(wildcard ../../app/smartmet-plugin-test),)
SMARTMET_PLUGIN_TEST := ../../app/smartmet-plugin-test
else
SMARTMET_PLUGIN_TEST := smartmet-plugin-test
endif

all:	all-test-engine	all-test-plugin

clean:	clean-test-engine clean-test-plugin
	rm -rf failures

test:	all
	$(SMARTMET_PLUGIN_TEST) -c reactor.conf -H /test

all-test-engine:
	$(MAKE) -C test_engine all

all-test-plugin:
	$(MAKE) -C test_plugin all

clean-test-engine:
	$(MAKE) -C test_engine clean

clean-test-plugin:
	$(MAKE) -C test_plugin clean
