clean:
	rm -rf llvm_ir/*/*.ll correct.err test.err
	rm -rf build/*
	touch build/.gitkeep

.PHONY: clean