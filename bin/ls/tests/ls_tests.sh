#
# Copyright 2015 EMC Corp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

create_test_inputs()
{
	ATF_TMPDIR=$(pwd)

	# XXX: need to nest this because of how kyua creates $TMPDIR; otherwise
	# it will run into EPERM issues later
	TEST_INPUTS_DIR="${ATF_TMPDIR}/test/inputs"

	atf_check -e empty -s exit:0 mkdir -m 0777 -p $TEST_INPUTS_DIR
	cd $TEST_INPUTS_DIR
	atf_check -e empty -s exit:0 mkdir -m 0755 -p a/b
	atf_check -e empty -s exit:0 ln -s a/b c
	atf_check -e empty -s exit:0 touch d
	atf_check -e empty -s exit:0 ln d e
	atf_check -e empty -s exit:0 touch .f
	atf_check -e empty -s exit:0 mkdir .g
	atf_check -e empty -s exit:0 mkfifo h
	atf_check -e ignore -s exit:0 dd if=/dev/zero of=i count=1000 bs=1
}

atf_test_case A_flag
A_flag_head()
{
	atf_set "descr" "Verify -A support with unprivileged users"
}

A_flag_body()
{
	create_test_inputs

	WITH_A=$PWD/../with_A.out
	WITHOUT_A=$PWD/../without_A.out

	atf_check -e empty -o save:$WITH_A -s exit:0 ls -A
	atf_check -e empty -o save:$WITHOUT_A -s exit:0 ls

	echo "-A usage"
	cat $WITH_A
	echo "No -A usage"
	cat $WITHOUT_A

	for dot_path in '\.f' '\.g'; do
		atf_check -e empty -o not-empty -s exit:0 grep "${dot_path}" \
		    $WITH_A
		atf_check -e empty -o empty -s not-exit:0 grep "${dot_path}" \
		    $WITHOUT_A
	done
}

atf_test_case A_flag_implied_when_root
A_flag_implied_when_root_head()
{
	atf_set "descr" "Verify that -A is implied for root"
	atf_set "require.user" "root"
}

A_flag_implied_when_root_body()
{
	create_test_inputs

	WITH_EXPLICIT=$PWD/../with_explicit_A.out
	WITH_IMPLIED=$PWD/../with_implied_A.out

	atf_check -e empty -o save:$WITH_EXPLICIT -s exit:0 ls -A
	atf_check -e empty -o save:$WITH_IMPLIED -s exit:0 ls

	echo "Explicit -A usage"
	cat $WITH_EXPLICIT
	echo "Implicit -A usage"
	cat $WITH_IMPLIED

	atf_check_equal "$(cat $WITH_EXPLICIT)" "$(cat $WITH_IMPLIED)"
}

atf_test_case B_flag
B_flag_head()
{
	atf_set "descr" "Verify that the output from ls -B prints out non-printable characters"
}

B_flag_body()
{

	atf_check -e empty -o empty -s exit:0 touch "$(printf "y\013z")"
	atf_check -e empty -o match:'y\\013z' -s exit:0 ls -B
}

atf_test_case C_flag
C_flag_head()
{
	atf_set "descr" "Verify that the output from ls -C is multi-column"
}

C_flag_body()
{
	create_test_inputs

	atf_check -e empty -o match:"$(printf 'a[[:space:]]+c[[:space:]]+d[[:space:]]+e[[:space:]]+h[[:space:]]i\n')" -s exit:0 ls -C
}

atf_test_case I_flag
I_flag_head()
{
	atf_set "descr" "Verify that the output from ls -I is the same as ls for an unprivileged user"
}

I_flag_body()
{
	create_test_inputs

	WITH_I=$PWD/../with_I.out
	WITHOUT_I=$PWD/../without_I.out

	atf_check -e empty -o save:$WITH_I -s exit:0 ls -I
	atf_check -e empty -o save:$WITHOUT_I -s exit:0 ls

	echo "Explicit -I usage"
	cat $WITH_I
	echo "No -I usage"
	cat $WITHOUT_I

	atf_check_equal "$(cat $WITH_I)" "$(cat $WITHOUT_I)"
}

atf_test_case I_flag_voids_A_flag_when_root
I_flag_voids_A_flag_when_root_head()
{
	atf_set "descr" "Verify that -I voids out implied -A for root"
	atf_set "require.user" "root"
}

I_flag_voids_A_flag_when_root_body()
{
	create_test_inputs

	atf_check -o not-match:'\.f' -s exit:0 ls -I
	atf_check -o not-match:'\.g' -s exit:0 ls -I

	atf_check -o match:'\.f' -s exit:0 ls -A -I
	atf_check -o match:'\.g' -s exit:0 ls -A -I
}

lcomma_flag_head()
{
	atf_set "descr" "Verify that -l, prints out the size with , delimiters"
}

lcomma_flag_body()
{
	create_test_inputs

	atf_check \
	    -o match:'\-rw\-r\-\-r\-\-[[:space:]]+.+[[:space:]]+1,000[[:space:]]+.+i' \
	    env LC_ALL=en_US.ISO8859-1 ls -l, i
}

1_flag_head()
{
	atf_set "descr" "Verify that -1 prints out one item per line"
}

1_flag_body()
{
	create_test_inputs

	WITH_1=$PWD/../with_1.out
	WITHOUT_1=$PWD/../without_1.out

	atf_check -e empty -o save:$WITH_1 -s exit:0 ls -1
	atf_check -e empty -o save:$WITHOUT_1 -s exit:0 \
		sh -c 'for i in $(ls); do echo $i; done'

	echo "Explicit -1 usage"
	cat $WITH_1
	echo "No -1 usage"
	cat $WITHOUT_1

	atf_check_equal "$(cat $WITH_1)" "$(cat $WITHOUT_1)"
}

atf_init_test_cases()
{

	atf_add_test_case A_flag
	atf_add_test_case A_flag_implied_when_root
	atf_add_test_case B_flag
	atf_add_test_case C_flag
	atf_add_test_case I_flag
	atf_add_test_case I_flag_voids_A_flag_when_root
	atf_add_test_case lcomma_flag
	atf_add_test_case 1_flag
}
