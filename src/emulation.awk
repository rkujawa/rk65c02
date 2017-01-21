BEGIN {
	FS=","
	print "#ifndef _EMULATION_H_"
	print "#define _EMULATION_H_"
}

/^OP_/{
	if ($5 != "NULL") {
		emuls[$5] = $5
	}
}

END {
	for (i in emuls)
		printf "void %s(rk65c02emu_t *, instruction_t *)\n",i

	print "#endif /* _EMULATION_H_ */"
}

