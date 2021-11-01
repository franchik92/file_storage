.PHONY: all cleanall test1 test2 test3

all:
	-@make -C client
	-@make -C server
	-@make -C tests all
cleanall:
	-@make -C client clean
	-@make -C server clean
	-@make -C tests clean
test1:
	@make -C tests test1
test2:
	@make -C tests test2
test3:
	@make -C tests test3