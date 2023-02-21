subdir-ccflags-y += -I$(srctree)/mm
subdir-ccflags-y += -I$(srctree)/drivers/staging/android
subdir-ccflags-y += -I$(srctree)/drivers/staging/android/ion

obj-$(CONFIG_DFX_MEMCHECK) += memstat_imp.o

obj-$(CONFIG_DFX_MEMCHECK_DETAIL) += memdetail_imp.o

obj-$(CONFIG_DFX_MEMCHECK_STACK) += memstack_imp.o memstack_buddy.o \
				    memstack_slub.o memstack_vmalloc.o

obj-$(CONFIG_DFX_MEMCHECK_EXT) += memtrace_ion.o memtrace_ashmem.o \
				  memtrace_gpumem.o memtrace_comm.o

obj-$(CONFIG_DFX_MEMCHECK_TEST) += memtrace_test.o
