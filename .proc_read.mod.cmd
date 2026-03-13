savedcmd_proc_read.mod := printf '%s\n'   proc_read.o | awk '!x[$$0]++ { print("./"$$0) }' > proc_read.mod
