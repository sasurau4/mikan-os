all: hello.efi

%.o: %.c
	clang++-18 -target x86_64-pc-win32-coff -o $@ -c $<

%.efi: %.o
	lld-link-18 /subsystem:efi_application /entry:EfiMain /out:$@ $<

.PHONY: run
run: hello.efi
	../../devenv/run_qemu.sh hello.efi

clean:
	rm -r hello.efi
	rm -f disk.img
	rm -rf mnt