#include <stdio.h>
#include <stdlib.h>
#include <linux/kvm.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <memory.h>

#define MAX_CPU 16
#define MAX_SLOTS 16

struct kvm {
	int fd;
	int api_version;
	int run_size;
} kvm;

struct cpu {
	int fd;
	struct kvm_run* run;
};

struct region {
	unsigned long start;
	char* user_start;
	unsigned long len;
};

struct vm {
	int fd;
	int cpu_nr;
	struct cpu *cpus[MAX_CPU];
	int slot_nr;
	struct region slots[MAX_SLOTS];
};

/* Create a kvm 		*
 * a. Open kvm node 		*
 * b. Create a virtual machine  *
 * c. Get api version		*/

int kvm_init(){
	kvm.fd = open("/dev/kvm", O_RDWR);
	if (kvm.fd == -1){
		printf ("Failed to open kvm file node.\n");
		return -1;
	}

	kvm.api_version = ioctl(kvm.fd, KVM_GET_API_VERSION, 0);
	if (kvm.api_version == -1){
		printf ("Failed to get api version.\n");
		return -1;
	}

	kvm.run_size = ioctl(kvm.fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (kvm.run_size < 0){
		printf ("Failed to get run size.\n");
		return -1;
	}

	return 0;
}

struct vm* kvm_create_vm(){
	struct vm * vm = malloc(sizeof(struct vm));
	
	if (vm == NULL){
		printf ("Failed to alloc struct vm.\n");
		return NULL;
	}

	memset(vm, sizeof(struct vm), 0);

	vm->fd = ioctl(kvm.fd, KVM_CREATE_VM, 0);
	if (vm->fd < 0){
		printf ("Failed to create virtaul machine.\n");
		return NULL;
	}

	return vm;
}

int kvm_create_cpu(struct vm* vm){
	int cpu_nr;
	struct cpu* cpu;
	int ret;

	if (vm->cpu_nr == MAX_CPU){
		printf ("No more than %d cpu.\n", MAX_CPU);
		return -1;
	}

	cpu_nr = vm->cpu_nr;

	ret = ioctl(vm->fd, KVM_CREATE_VCPU, cpu_nr);
	if (ret < 0){
		printf ("Failed to create cpu.\n");
		return -1;
	}
	
	cpu = malloc(sizeof(struct cpu));
	if (cpu == NULL){
		printf ("Failed to alloc struct cpu.\n");
		return -1;
	}

	cpu->fd = ret;

	cpu->run = mmap(NULL, kvm.run_size, PROT_READ|PROT_WRITE, 
							MAP_SHARED, ret, 0);
	
	vm->cpus[cpu_nr] = cpu;

	vm->cpu_nr ++;

	return vm->cpu_nr - 1;
}

/* Set memory region, start and len should be mutiple of pages */
/* Should not set overlap regions(KVM can detect that)         */
int kvm_set_address(struct vm* vm, unsigned long start, unsigned long len){
	int ret;
	unsigned char* addr;
	struct kvm_userspace_memory_region  region;

	/* TODO Check slots limit */

	/* Use mmap for page-align */
	addr = mmap(NULL, len, PROT_READ|PROT_WRITE, 
				MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

	if (addr == MAP_FAILED){
		printf ("Failed to alloc user memory region: %m.\n");
		return -1;
	}

	region.slot = vm->slot_nr;
	region.guest_phys_addr = start;
	region.memory_size = len;
	region.userspace_addr = (unsigned long)addr;

	ret = ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &region);
	if (ret < 0){
		printf ("Faile to set user memory region: %m\n");
		return -1;
	}

	vm->slots[vm->slot_nr].start = start;
	vm->slots[vm->slot_nr].len = len;
	vm->slots[vm->slot_nr].user_start = addr;
	vm->slot_nr ++;

	return vm->slot_nr - 1;
}

int kvm_find_slot(struct vm* vm, unsigned long addr){
	int i;
	
	for (i = 0; i < vm->slot_nr; ++i)
		if (addr >= vm->slots[i].start &&
			addr <= vm->slots[i].start + vm->slots[i].len)
			break;
	
	return i == vm->slot_nr ? -1 : i;
}

int kvm_run(struct vm *vm, int cpu_id){
	return ioctl(vm->cpus[cpu_id]->fd, KVM_RUN, 0);
}

int kvm_load_content(struct vm* vm, unsigned long addr, 
				char* content, unsigned long len){
	int ret;
	unsigned long offset;

	ret = kvm_find_slot(vm, addr);

	if (ret < 0){
		printf ("Address %lx not created yet.\n", addr);
		return -1;
	}

	if (addr + len > vm->slots[ret].start + vm->slots[ret].len){
		printf ("%lx:%lx beyond slot %lx %lx.\n", 
			addr, len, vm->slots[ret].start, vm->slots[ret].len);
		return -1;
	}

	memcpy(vm->slots[ret].user_start + (addr - vm->slots[ret].start),
					 content, len);
	
	return 0;
}

int kvm_io_port(struct cpu *cpu){
	return cpu->run->io.port;
}

int kvm_io_data(struct cpu *cpu){
	int offset;
	struct kvm_run * run = cpu->run;
	char* data;

	offset = run->io.data_offset;
	
	data = (char*)run + offset;

	return *(int*)data;
}

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
		int fd = open("test.bin", O_RDONLY);
		unsigned char fil[7];
		
		read(fd, fil, sizeof fil);

		kvm_load_content(vm1, 0xffff0, fil, 7);
	}

	while (!(ret = kvm_run(vm1, 0))){
		if (vm1->cpus[0]->run->exit_reason == KVM_EXIT_IO){
			printf ("Exit IO, port:%d data:%d\n",
					kvm_io_data(vm1->cpus[0]),
					kvm_io_port(vm1->cpus[0]));
			sleep(1);
		} else	break;
	}
	
	printf ("Failed to run kvm: %m.\n");

	return 0;
}
