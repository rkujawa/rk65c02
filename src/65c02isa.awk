BEGIN {
	FS=","
	print "#ifndef _65C02ISA_H_"
	print "#define _65C02ISA_H_"
	print "#include <stdint.h>"
	print "#include \"instruction.h\""
	print "#include \"emulation.h\""

	print "const struct instrdef instrs[] = {"
}

{
	printf "#define %s 0x%X\n", $1, NR-1
	if (NR < 256)
		printf "\t{ %s, %s, %s, %d, %s },\n", $1, $2, $3, $4, $5
	else
		printf "\t{ %s, %s, %s, %d, %s }\n", $1, $2, $3, $4, $5

}

END {
	print "};"
	print "#endif /* _65C02ISA_H_ */"
}

