#	$OpenBSD: try-ciphers.sh,v 1.23 2014/04/21 22:15:37 djm Exp $
#	Placed in the Public Domain.

tid="try ciphers"

cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

for c in `${SSH} -Q cipher`; do
	n=0
	for m in `${SSH} -Q mac`; do
		trace "proto 2 cipher $c mac $m"
		verbose "test $tid: proto 2 cipher $c mac $m"
		cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
		echo "Ciphers=$c" >> $OBJ/sshd_proxy
		echo "MACs=$m" >> $OBJ/sshd_proxy
		${SSH} -F $OBJ/ssh_proxy -2 -m $m -c $c somehost true
		if [ $? -ne 0 ]; then
			fail "ssh -2 failed with mac $m cipher $c"
		fi
		# No point trying all MACs for AEAD ciphers since they
		# are ignored.
		if ssh -Q cipher-auth | grep "^${c}\$" >/dev/null 2>&1 ; then
			break
		fi
		n=`expr $n + 1`
	done
done

ciphers="3des blowfish"
for c in $ciphers; do
	trace "proto 1 cipher $c"
	verbose "test $tid: proto 1 cipher $c"
	${SSH} -F $OBJ/ssh_proxy -1 -c $c somehost true
	if [ $? -ne 0 ]; then
		fail "ssh -1 failed with cipher $c"
	fi
done

