#!/bin/bash
#通过netstat 统计tcp数据，每次1秒间隔的相对值数据
netstat -s | grep -A 10 Tcp:
while true; do
	netstat -s | grep -A 10 Tcp: | awk 'NR>1 {$1=$1; print}'
	printf "\n"
	sleep 1
done |
awk -v RS="" -v FS="\n" '{
for (i = 1; i <= NF; i++) {
	VALUE = substr($i, 0, index($i, " ") - 1)
	if (NR > 1) {
		printf("%10d", VALUE - DATA[i])
	}
	DATA[i] = VALUE
}
if (NR > 1) {
	printf("\n")
} }'
