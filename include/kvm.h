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

int kvm_init();
struct vm* kvm_create_vm();
int kvm_create_cpu(struct vm* vm);
int kvm_set_address(struct vm* vm, unsigned long start, unsigned long len);
int kvm_run(struct vm *vm, int cpu_id);
int kvm_load_content(struct vm* vm, unsigned long addr, 
				char* content, unsigned long len);
int kvm_io_port(struct cpu *cpu);
int kvm_io_data(struct cpu *cpu);
