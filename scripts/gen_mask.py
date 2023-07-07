import sys

#Provide a comma-separated list of CPUs
#Output: binary and hex masks corresponding to the list
lst=sys.argv[1].split(',')
print(lst)

val=0
for v in lst:
    val = val | (1<<int(v))

print(bin(val))    
print(hex(val))
