projectName ?= fio-test

.PHONY: help
help:
	@echo "help:"
	@echo "use [ make run-demo] run fio io_uring libaio demo seq and random w/r test"
	@echo "use [ make run-iouring] run fio io_uring seq and random w/r test"
	@echo "use [ make run-seq] run fio libaio vs io_uring seq w/r test"
	@echo "use [ make run-random] run fio libaio vs io_uring random w/r test"
	@echo "use [ make clear] clear log svg"

.PHONY: clear
clear:
	@rm -f ./*.log
	@rm -f ./*.svg
	@rm -f ./*fio-test-file
	@rm -f ./*.fio.data

.PHONY: run-demo
run-demo:
	@fio fio-iouring-libaio.demo.conf 

.PHONY: run-iouring
run-iouring:
	@fio fio-iouring.conf 

.PHONY: run-seq
run-seq:
	@fio fio-libaio-iouring.seq.conf 

.PHONY: run-random
run-random:
	@fio fio-libaio-iouring.rand.conf 