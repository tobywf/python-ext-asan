install:
	python3 -m venv env
	env/bin/python setup.py develop

.PHONY: clean
clean:
	rm -rf asan_test.egg-info/ build/ env/ asan.cpython-*.so
