kvm_test: test/test.c kvm.so include/kvm.h
	gcc -o $@ test/test.c kvm.so -L. -Iinclude 

kvm.so: lib/kvm.c include/kvm.h
	gcc -shared -fPIC lib/kvm.c -Iinclude -o kvm.so

