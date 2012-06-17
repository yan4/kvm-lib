#include <linux/kvm.h>
#include <kvm.h>
#include <stdio.h>
#include <fcntl.h>

int main (void){
	int ret;
	struct vm* vm1;
	int cpu1;

	ret = kvm_init();
	if (ret < 0)
		return ret;
	
	printf ("KVM : FD=%d, API VERSION=%d, RUN_SIZE:%d\n", kvm.fd, 
						kvm.api_version, kvm.run_size);

	vm1 = kvm_create_vm();	
	if (vm1 == NULL)
		return -1;

	printf ("VM1 FD:%d\n", vm1->fd);

	cpu1 = kvm_create_cpu(vm1);
	if (cpu1 < 0)
		return -1;

	printf ("CPU FD:%d, CPU RUN ADDR:%p\n", vm1->cpus[cpu1]->fd,
						vm1->cpus[cpu1]->run);
		
	ret = kvm_set_address(vm1, 0xff000, 4096);
	if (ret < 0)
		return ret;

	ret = kvm_set_address(vm1, 0, 4096);
	if (ret < 0)
		return ret;

	ret = kvm_find_slot(vm1, 0xfff00);
	printf ("Slot:%d\n", ret);

	ret = kvm_find_slot(vm1, 0x23);
	printf ("Slot:%d\n", ret);

	ret = kvm_find_slot(vm1, 0x12345);
	printf ("Slot:%d\n", ret);

	{
		int fd = open("bin", O_RDONLY);
		unsigned char fil[13];
		
		read(fd, fil, sizeof fil);

		kvm_load_content(vm1, 0xffff0, fil, 7);
	}

	while (!(ret = kvm_run(vm1, 0))){
		if (vm1->cpus[0]->run->exit_reason == KVM_EXIT_IO){
			printf ("Exit IO, port:%d data:%d\n",
					kvm_io_data(vm1->cpus[0]),
					kvm_io_port(vm1->cpus[0]));
			sleep(1);
		} else break;
	}
	
	printf ("KVM eixt for %d.\n", vm1->cpus[0]->run->exit_reason);

	return 0;
}
